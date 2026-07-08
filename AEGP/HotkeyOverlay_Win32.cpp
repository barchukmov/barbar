#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "HotkeyOverlay.h"
#include "win32_popup_bridge.h"
#include "WsClient.h"
#include <unordered_map>
#include <vector>
#include <sstream>
#include <cstdlib>

namespace {
	// Custom thread message carrying a freshly-pushed hotkey table. Posted
	// from whatever thread receives the WS message (IXWebSocket's own), but
	// must be applied on this thread - RegisterHotKey/UnregisterHotKey are
	// tied to the thread whose message queue receives WM_HOTKEY.
	const UINT kHotkeysUpdatedMsg = WM_APP + 1;
	// Polls GetForegroundWindow so the hotkeys can be unregistered while AE
	// isn't focused - see kForegroundPollTimerId below.
	const UINT kForegroundPollTimerId = 1;
	const UINT kForegroundPollMs = 250;

	HANDLE g_thread = NULL;
	DWORD g_threadId = 0;
	// Signaled by ThreadProc once its message queue exists; kept alive for the
	// plugin's lifetime (closed in StopHotkeyOverlay) so ThreadProc can never
	// signal a handle that a timed-out StartHotkeyOverlay already closed.
	HANDLE g_queueReady = NULL;

	// Only ever touched on the overlay thread (ThreadProc), so no lock needed.
	std::unordered_map<int, std::string> g_idToFn;
	// Last payload applied, kept around so the foreground-poll timer can
	// re-register the same table once AE regains focus.
	std::string g_lastPayload;
	bool g_registered = false;

	struct Binding { int id; UINT vkey; UINT mods; std::string fn; };
}

namespace {

	std::vector<Binding> ParseHotkeyPayload(const std::string& payload)
	{
		std::vector<Binding> out;
		std::stringstream ss(payload);
		std::string entry;
		while (std::getline(ss, entry, ';')) {
			if (entry.empty()) continue;
			std::stringstream es(entry);
			std::string idStr, vkeyStr, modsStr, fn;
			std::getline(es, idStr, ',');
			std::getline(es, vkeyStr, ',');
			std::getline(es, modsStr, ',');
			std::getline(es, fn, ',');
			if (idStr.empty() || vkeyStr.empty() || modsStr.empty()) continue;
			out.push_back({ std::atoi(idStr.c_str()), (UINT)std::atoi(vkeyStr.c_str()), (UINT)std::atoi(modsStr.c_str()), fn });
		}
		return out;
	}

	void UnregisterAllHotkeys()
	{
		for (auto& entry : g_idToFn) UnregisterHotKey(NULL, entry.first);
		g_idToFn.clear();
		g_registered = false;
	}

	// RegisterHotKey is global - the instant a combo is registered, Windows
	// swallows it for every app, not just AE, before any of our own
	// foreground checks run. So the table must only be registered while AE is
	// actually the foreground app; ForegroundPollTick (driven by a timer)
	// calls this/UnregisterAllHotkeys as focus changes, instead of keeping
	// the keys grabbed system-wide all the time.
	void ApplyHotkeyTable(const std::string& payload)
	{
		UnregisterAllHotkeys();

		for (auto& b : ParseHotkeyPayload(payload)) {
			UINT winMods = MOD_NOREPEAT;
			if (b.mods & 0x1) winMods |= MOD_CONTROL;
			if (b.mods & 0x2) winMods |= MOD_SHIFT;
			if (b.mods & 0x4) winMods |= MOD_ALT;
			if (RegisterHotKey(NULL, b.id, winMods, b.vkey)) {
				g_idToFn[b.id] = b.fn;
			}
		}
		g_registered = true;
	}

	bool IsAeForeground()
	{
		DWORD fgPid = 0;
		GetWindowThreadProcessId(GetForegroundWindow(), &fgPid);
		return fgPid == GetCurrentProcessId();
	}

	void ForegroundPollTick()
	{
		bool aeForeground = IsAeForeground();
		if (aeForeground && !g_registered && !g_lastPayload.empty()) {
			ApplyHotkeyTable(g_lastPayload);
		} else if (!aeForeground && g_registered) {
			UnregisterAllHotkeys();
		}
	}

	// Function names AEGP can run itself vs. forwarding to CEP over WS.
	// Just "Easing" today - see RunNativeFunction's caller for the fallback.
	bool RunNativeFunction(const std::string& fn, int x, int y, const RECT& monitor)
	{
		if (fn == "Easing") {
			std::string reply = WsRequest(R"({"type":"keyframeSelectionQuery"})", "keyframeSelectionReply", 300);
			// ponytail: an empty/timed-out reply (CEP slow to answer, or not
			// connected yet) is treated as "selected" - this is a UX nicety,
			// not a guarantee, so erring toward showing the popup is right.
			bool selected = reply.empty() || WsJsonGetString(reply, "selected") != "false";
			int monW = monitor.right - monitor.left;
			int monH = monitor.bottom - monitor.top;
			if (selected) {
				RunPopupAtCursor(x, y, monitor.left, monitor.top, monW, monH);
			} else {
				RunNoSelectionToast(x, y, monitor.left, monitor.top, monW, monH);
			}
			return true;
		}
		return false;
	}

	DWORD WINAPI ThreadProc(LPVOID queueReadyEvent)
	{
		// A thread has no message queue until its first User32 call - and
		// PostThreadMessage to a queue-less thread just fails. Force the queue
		// into existence, then release StartHotkeyOverlay, so an
		// UpdateHotkeyTable racing in right after startup can't be dropped.
		MSG peek;
		PeekMessageW(&peek, NULL, WM_USER, WM_USER, PM_NOREMOVE);
		SetEvent((HANDLE)queueReadyEvent);

		SetTimer(NULL, kForegroundPollTimerId, kForegroundPollMs, NULL);

		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0)) {
			if (msg.message == kHotkeysUpdatedMsg) {
				std::string* payload = reinterpret_cast<std::string*>(msg.lParam);
				g_lastPayload = *payload;
				delete payload;
				// Only actually grab the keys if AE is foreground right now -
				// otherwise wait for ForegroundPollTick to do it once it is.
				if (IsAeForeground()) ApplyHotkeyTable(g_lastPayload);
			} else if (msg.message == WM_TIMER && msg.wParam == kForegroundPollTimerId) {
				ForegroundPollTick();
			} else if (msg.message == WM_HOTKEY) {
				auto it = g_idToFn.find((int)msg.wParam);
				if (it == g_idToFn.end()) continue;

				POINT pt;
				GetCursorPos(&pt);
				// The overlay must cover the monitor the cursor is on, not the
				// primary - SM_CXSCREEN/SM_CYSCREEN describe only the primary, so
				// on a secondary display the popup used to open on the wrong
				// screen with the cursor "outside" the window.
				RECT monitor = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
				MONITORINFO mi = { sizeof(MONITORINFO) };
				if (GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi)) {
					monitor = mi.rcMonitor;
				}
				if (!RunNativeFunction(it->second, pt.x, pt.y, monitor)) {
					WsSend(R"({"type":"hotkeyTriggered","fn":")" + it->second + "\"}");
				}
			}
		}

		KillTimer(NULL, kForegroundPollTimerId);
		UnregisterAllHotkeys();
		return 0;
	}
}

void StartHotkeyOverlay()
{
	if (g_thread) return;
	g_queueReady = CreateEventW(NULL, TRUE, FALSE, NULL);
	g_thread = CreateThread(NULL, 0, ThreadProc, g_queueReady, 0, &g_threadId);
	// Don't return until the thread can actually receive posted messages (or
	// 2s passes - then we're no worse off than before this wait existed).
	if (g_thread) WaitForSingleObject(g_queueReady, 2000);
}

void StopHotkeyOverlay()
{
	if (!g_thread) return;
	// ponytail: if a popup happens to be open right now the thread is inside
	// RunPopupAtCursor's own loop, not GetMessage, so this WM_QUIT sits queued
	// until the popup closes; the 2s wait just gives up rather than blocking
	// AE's shutdown. Fine for a demo - revisit if that's ever user-visible.
	PostThreadMessage(g_threadId, WM_QUIT, 0, 0);
	WaitForSingleObject(g_thread, 2000);
	CloseHandle(g_thread);
	g_thread = NULL;
	if (g_queueReady) {
		CloseHandle(g_queueReady);
		g_queueReady = NULL;
	}
}

void UpdateHotkeyTable(const std::string& payload)
{
	if (!g_threadId) return;
	std::string* heapPayload = new std::string(payload);
	if (!PostThreadMessage(g_threadId, kHotkeysUpdatedMsg, 0, (LPARAM)heapPayload)) {
		delete heapPayload; // post failed - don't leak; CEP re-pushes on every (re)connect
	}
}

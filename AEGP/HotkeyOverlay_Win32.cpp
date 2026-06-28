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

	HANDLE g_thread = NULL;
	DWORD g_threadId = 0;

	// Only ever touched on the overlay thread (ThreadProc), so no lock needed.
	std::unordered_map<int, std::string> g_idToFn;

	struct Binding { int id; UINT vkey; UINT mods; std::string fn; };

	// Module-relative path is resolved once and cached - it can't change at runtime.
	std::string g_fontPath;
}

const char* GetFontPath()
{
	if (g_fontPath.empty()) {
		HMODULE module = nullptr;
		GetModuleHandleExA(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)&GetFontPath, &module);
		char path[MAX_PATH];
		GetModuleFileNameA(module, path, MAX_PATH);
		std::string dir(path);
		dir = dir.substr(0, dir.find_last_of("\\/"));
		g_fontPath = dir + "\\Fonts\\Mannin-Regular.otf";
	}
	return g_fontPath.c_str();
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

	void ApplyHotkeyTable(const std::string& payload)
	{
		for (auto& entry : g_idToFn) UnregisterHotKey(NULL, entry.first);
		g_idToFn.clear();

		for (auto& b : ParseHotkeyPayload(payload)) {
			UINT winMods = MOD_NOREPEAT;
			if (b.mods & 0x1) winMods |= MOD_CONTROL;
			if (b.mods & 0x2) winMods |= MOD_SHIFT;
			if (b.mods & 0x4) winMods |= MOD_ALT;
			if (RegisterHotKey(NULL, b.id, winMods, b.vkey)) {
				g_idToFn[b.id] = b.fn;
			}
		}
	}

	// Function names AEGP can run itself vs. forwarding to CEP over WS.
	// Just "Easing" today - see RunNativeFunction's caller for the fallback.
	bool RunNativeFunction(const std::string& fn, int x, int y, int screenW, int screenH)
	{
		if (fn == "Easing") {
			std::string reply = WsRequest(R"({"type":"keyframeSelectionQuery"})", "keyframeSelectionReply", 300);
			// ponytail: an empty/timed-out reply (CEP slow to answer, or not
			// connected yet) is treated as "selected" - this is a UX nicety,
			// not a guarantee, so erring toward showing the popup is right.
			bool selected = reply.empty() || WsJsonGetString(reply, "selected") != "false";
			if (selected) {
				RunPopupAtCursor(x, y, screenW, screenH);
			} else {
				RunNoSelectionToast(x, y, screenW, screenH);
			}
			return true;
		}
		return false;
	}

	DWORD WINAPI ThreadProc(LPVOID)
	{
		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0)) {
			if (msg.message == kHotkeysUpdatedMsg) {
				std::string* payload = reinterpret_cast<std::string*>(msg.lParam);
				ApplyHotkeyTable(*payload);
				delete payload;
			} else if (msg.message == WM_HOTKEY) {
				auto it = g_idToFn.find((int)msg.wParam);
				if (it == g_idToFn.end()) continue;

				DWORD fgPid = 0;
				GetWindowThreadProcessId(GetForegroundWindow(), &fgPid);
				if (fgPid != GetCurrentProcessId()) continue;

				POINT pt;
				GetCursorPos(&pt);
				if (!RunNativeFunction(it->second, pt.x, pt.y, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN))) {
					WsSend(R"({"type":"hotkeyTriggered","fn":")" + it->second + "\"}");
				}
			}
		}

		for (auto& entry : g_idToFn) UnregisterHotKey(NULL, entry.first);
		ClosePopupWindow(); // same thread that created it - must tear down here, not from DeathHook's thread
		return 0;
	}
}

void StartHotkeyOverlay()
{
	if (g_thread) return;
	g_thread = CreateThread(NULL, 0, ThreadProc, NULL, 0, &g_threadId);
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
}

void UpdateHotkeyTable(const std::string& payload)
{
	if (!g_threadId) return;
	PostThreadMessage(g_threadId, kHotkeysUpdatedMsg, 0, (LPARAM)new std::string(payload));
}

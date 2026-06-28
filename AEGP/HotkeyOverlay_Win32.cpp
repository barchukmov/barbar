#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "HotkeyOverlay.h"
#include "win32_popup_bridge.h"

namespace {
	const int kHotkeyId = 1;
	HANDLE g_thread = NULL;
	DWORD g_threadId = 0;

	DWORD WINAPI ThreadProc(LPVOID)
	{
		if (!RegisterHotKey(NULL, kHotkeyId, MOD_CONTROL | MOD_NOREPEAT, 'H')) return 1;

		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0)) {
			if (msg.message == WM_HOTKEY && (int)msg.wParam == kHotkeyId) {
				DWORD fgPid = 0;
				GetWindowThreadProcessId(GetForegroundWindow(), &fgPid);
				if (fgPid == GetCurrentProcessId()) {
					POINT pt;
					GetCursorPos(&pt);
					// Pass primary-monitor size so the raylib side can create the
					// window at final size (it can't include windows.h itself).
					RunPopupAtCursor(pt.x, pt.y, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)); // blocks until the popup closes
				}
			}
		}

		UnregisterHotKey(NULL, kHotkeyId);
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

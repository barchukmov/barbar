// Hides the popup's window from the taskbar and Alt-Tab. GLFW (raylib's
// backend) has no SetConfigFlags for this, so it's applied straight to the
// HWND: raylib's GetWindowHandle() hands that HWND to the raylib.h side as a
// void*, which passes it here rather than including windows.h itself
// (win32_popup_bridge.h - the two headers can't share a translation unit).
//
// Must be called while the window is still hidden (OpenOverlayWindow creates
// it with FLAG_WINDOW_HIDDEN and reveals it after this runs) - an ex-style
// change doesn't reach an *already-shown* window's taskbar button without a
// hide/show cycle.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "win32_popup_bridge.h"

// %TEMP%\barbar-popup.log - the sink for the raylib trace log (see
// RaylibLogToDebug in HotkeyOverlay_Popup.cpp), so the popup's startup
// diagnostics (display/GL/window sizes) are readable when it runs inside
// AfterFX.exe, where there is no console. Every line also goes to
// OutputDebugString (prefix "barbar-popup:") for live viewing in DebugView.
// Lives here rather than the raylib TU because timestamps/ODS need windows.h.
static void PopupLogPath(char* buf, DWORD bufLen)
{
	DWORD n = GetTempPathA(bufLen, buf);
	lstrcpynA(buf + n, "barbar-popup.log", bufLen - n);
}

static void PopupLogWrite(const char* line, const char* mode)
{
	char path[MAX_PATH + 32];
	PopupLogPath(path, sizeof(path));
	FILE* f = fopen(path, mode);
	if (!f) return;
	SYSTEMTIME st;
	GetLocalTime(&st);
	fprintf(f, "[%02u:%02u:%02u.%03u] %s\n", st.wHour, st.wMinute, st.wSecond,
	        st.wMilliseconds, line);
	fclose(f);
}

// Truncate + header: the file always holds exactly the last popup's lifetime.
void PopupDebugLogReset(void)
{
	PopupLogWrite("=== popup window open ===", "w");
}

void PopupDebugLog(const char* line)
{
	PopupLogWrite(line, "a");
	char ods[1152];
	snprintf(ods, sizeof(ods), "barbar-popup: %s\n", line);
	OutputDebugStringA(ods);
}

void ApplyPopupWindowStyle(void* hwnd)
{
	HWND hWnd = (HWND)hwnd;
	LONG_PTR exStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
	exStyle = (exStyle & ~WS_EX_APPWINDOW) | WS_EX_TOOLWINDOW;
	SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle);
}

// Opt the calling thread out of Windows 11's execution-speed throttling
// (EcoQoS). Windows throttles processes with no foreground window - which
// froze PopupExe's draw loop for seconds at a time (see popup_main.cpp,
// where the real fix lives: the timer-resolution half of the throttling is
// process-level-only and that's what stretched the frame-pacing Sleep).
// Inside AfterFX.exe the process is foreground whenever a popup can fire,
// so throttling shouldn't engage at all; this per-thread opt-out is cheap
// insurance for the overlay thread, not a load-bearing fix.
void DisableOverlayThreadThrottling()
{
	THREAD_POWER_THROTTLING_STATE state = {};
	state.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
	state.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
	state.StateMask = 0; // 0 = always run at normal speed
	SetThreadInformation(GetCurrentThread(), ThreadPowerThrottling, &state,
	                     sizeof(state));
}

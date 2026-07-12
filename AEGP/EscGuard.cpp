// Esc must never be observable by After Effects while the popup is up: AE
// aborts any running ExtendScript when it sees Esc - both as a keyboard
// event reaching its queue (latched, so it kills even a script that starts
// *after* the press, e.g. our cancelEase) and via the live key state polled
// mid-script (killing an in-flight preview applyEase) - with "Unable to
// execute script at line N. Execution halted", N being wherever the abort
// landed, hence the apparently random line numbers. Esc is also the popup's
// cancel key, and the cancel path drives a script in AE.
//
// A WH_KEYBOARD_LL hook that eats the key closes every path at once: a
// low-level event blocked by the hook never reaches any window's message
// queue *and* never enters GetAsyncKeyState, so AE cannot see the press by
// any mechanism. The popup reads the press from the hook instead
// (EscapeGuardTriggered) - raylib itself no longer sees Esc while the guard
// is up.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "win32_popup_bridge.h"

namespace {
	HHOOK g_escHook = NULL;
	// Hook callbacks arrive on the installing thread's message pump (raylib's
	// EndDrawing polls events every frame), so these are only ever touched on
	// the overlay thread - no locking needed.
	bool g_escPressed = false;
	bool g_escDown = false;

	LRESULT CALLBACK EscHookProc(int code, WPARAM wParam, LPARAM lParam)
	{
		if (code == HC_ACTION) {
			const KBDLLHOOKSTRUCT* k = (const KBDLLHOOKSTRUCT*)lParam;
			if (k->vkCode == VK_ESCAPE) {
				if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
					g_escPressed = true;
					g_escDown = true;
				} else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
					g_escDown = false;
				}
				return 1; // eat it: no message, no async-key-state update
			}
		}
		return CallNextHookEx(g_escHook, code, wParam, lParam);
	}
}

void BeginEscapeGuard()
{
	g_escPressed = false;
	g_escDown = false;
	if (!g_escHook)
		g_escHook = SetWindowsHookExW(WH_KEYBOARD_LL, EscHookProc, GetModuleHandleW(NULL), 0);
}

void EndEscapeGuard()
{
	if (g_escHook) {
		UnhookWindowsHookEx(g_escHook);
		g_escHook = NULL;
	}
	g_escPressed = false;
	g_escDown = false;
}

bool EscapeGuardTriggered()
{
	return g_escPressed;
}

bool EscapeGuardKeyStillDown()
{
	return g_escDown;
}

void WaitForEscapeReleased()
{
	// Fallback for when the hook isn't active (SetWindowsHookEx failed, or
	// Windows silently dropped a hook that was too slow to respond): raylib
	// then saw the Esc keydown itself, and so did the async key state. The
	// high bit of GetAsyncKeyState is the current physical state - wait it
	// out before letting the caller drive a script. Bounded so a genuinely
	// held Esc can't wedge the overlay thread; if it's still down past ~1s we
	// send cancel anyway and accept the rare abort over hanging. When the
	// guard *was* active it ate the key, GetAsyncKeyState never saw it, and
	// this returns immediately.
	for (int i = 0; i < 200 && (GetAsyncKeyState(VK_ESCAPE) & 0x8000); i++) {
		Sleep(5);
	}
}

// ---------------------------------------------------------------------------
// Mouse guard - the popup window is only panel-sized (a monitor-covering
// transparent window is impossible on hybrid-GPU machines: DWM drops the
// alpha of GL framebuffers rendered by the discrete GPU, see the window
// comment in HotkeyOverlay_Popup.cpp), so mouse input can't come from the
// window anymore. Same WH_MOUSE_LL trick as the Esc guard above: both
// buttons are read globally AND eaten while the guard is up, so the
// commit/cancel click never reaches AE - the old fullscreen window used to
// swallow it by covering the screen. Cursor position (OverlayCursorPos) and
// modifiers (Overlay*Down) need no hook: GetCursorPos/GetAsyncKeyState work
// globally, and unlike raylib's events they don't need the popup window
// focused (Windows' foreground-lock can silently deny it focus).
//
// Same threading contract as the Esc guard: hook callbacks arrive on the
// installing thread's message pump (raylib's EndDrawing polls events every
// frame), so the state needs no locking. Press/release latches stay set
// until consumed, so a press-and-release that happens inside one 60fps
// frame can't slip between two polls.

namespace {
	HHOOK g_mouseHook = NULL;
	bool g_mouseLeftPressed = false;  // latched until consumed
	bool g_mouseLeftReleased = false; // latched until consumed
	bool g_mouseRightPressed = false; // latched until consumed

	LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam)
	{
		if (code == HC_ACTION) {
			switch (wParam) {
			case WM_LBUTTONDOWN: g_mouseLeftPressed = true;  return 1;
			case WM_LBUTTONUP:   g_mouseLeftReleased = true; return 1;
			case WM_RBUTTONDOWN: g_mouseRightPressed = true; return 1;
			case WM_RBUTTONUP:   return 1;
			}
		}
		return CallNextHookEx(g_mouseHook, code, wParam, lParam);
	}
}

void BeginMouseGuard()
{
	g_mouseLeftPressed = false;
	g_mouseLeftReleased = false;
	g_mouseRightPressed = false;
	if (!g_mouseHook)
		g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, GetModuleHandleW(NULL), 0);
}

void EndMouseGuard()
{
	if (g_mouseHook) {
		UnhookWindowsHookEx(g_mouseHook);
		g_mouseHook = NULL;
	}
	g_mouseLeftPressed = false;
	g_mouseLeftReleased = false;
	g_mouseRightPressed = false;
}

bool MouseGuardInstalled()
{
	return g_mouseHook != NULL;
}

bool MouseGuardConsumeLeftPress()
{
	bool v = g_mouseLeftPressed;
	g_mouseLeftPressed = false;
	return v;
}

bool MouseGuardConsumeLeftRelease()
{
	bool v = g_mouseLeftReleased;
	g_mouseLeftReleased = false;
	return v;
}

bool MouseGuardConsumeRightPress()
{
	bool v = g_mouseRightPressed;
	g_mouseRightPressed = false;
	return v;
}

void OverlayCursorPos(int* x, int* y)
{
	POINT p;
	GetCursorPos(&p);
	*x = p.x;
	*y = p.y;
}

bool OverlayAltDown()   { return (GetAsyncKeyState(VK_MENU) & 0x8000) != 0; }
bool OverlayShiftDown() { return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0; }
bool OverlayCtrlDown()  { return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0; }

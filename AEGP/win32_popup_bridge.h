#pragma once

// Bridges the windows.h-only side (HotkeyOverlay_Win32.cpp) and the
// raylib.h-only side (HotkeyOverlay_Popup.cpp). windows.h and raylib.h both
// redefine CloseWindow/ShowCursor/etc, so they can never share a translation
// unit - this header carries no types from either.
//
// mouseX/mouseY are global (virtual-desktop) cursor coordinates. monX/monY/
// monW/monH is the rect of the monitor under the cursor (MonitorFromPoint on
// the windows.h side) - the panel-sized popup window is centered on the
// cursor and clamped fully onto that monitor, so the popup follows the
// cursor onto secondary displays instead of being pinned to the primary.
void RunPopupAtCursor(int mouseX, int mouseY, int monX, int monY, int monW, int monH);

// Strips the taskbar/Alt-Tab entry from the overlay window
// (WS_EX_TOOLWINDOW instead of WS_EX_APPWINDOW). Takes the HWND as void* -
// see the header comment above for why windows.h can't be included here -
// pass raylib's GetWindowHandle() right after InitWindow, while the window
// is still hidden (an ex-style change doesn't reach an already-shown
// window's taskbar button without a hide/show cycle).
void ApplyPopupWindowStyle(void* hwnd);

// Opts the calling thread out of Windows 11 execution-speed throttling
// (EcoQoS). Cheap insurance, not the load-bearing fix - see
// PopupWindowStyle.cpp and PopupExe/popup_main.cpp (where the process-level
// opt-out lives) for the full story of the foreground-less-process freeze.
// Called at every OpenOverlayWindow; idempotent.
void DisableOverlayThreadThrottling();

// Sink for the raylib trace log so it's readable inside AfterFX.exe (no
// console there): each line is appended, timestamped, to
// %TEMP%\barbar-popup.log and echoed to OutputDebugString ("barbar-popup:"
// prefix, for DebugView). Reset truncates the file - called at every
// OpenOverlayWindow, so the file always holds exactly the last popup's
// lifetime. (PopupWindowStyle.cpp - the windows.h side.)
void PopupDebugLogReset();
void PopupDebugLog(const char* line);

// Auto-dismissing toast (no click needed) for when the Easing shortcut
// fires with nothing selected - fades out over ~1s at the cursor.
void RunNoSelectionToast(int mouseX, int mouseY, int monX, int monY, int monW, int monH);

// While the popup is up, a WH_KEYBOARD_LL hook eats the Escape key system-
// wide so After Effects can never observe it: AE aborts any running
// ExtendScript when it sees Esc, and Esc is also the popup's cancel key,
// whose handling drives a script in AE (see EscGuard.cpp for the full
// story). The raylib side therefore reads Esc from the guard, not from the
// keyboard: Triggered() latches on the first Esc-down after Begin;
// KeyStillDown() tracks the physical state so the popup window can outlive
// the press and keep eating auto-repeats/keyup. Begin degrades to a no-op if
// the hook can't install - raylib then sees Esc normally (its default exit
// key) and WaitForEscapeReleased covers the gap.
void BeginEscapeGuard();
void EndEscapeGuard();
bool EscapeGuardTriggered();
bool EscapeGuardKeyStillDown();

// The mouse guard (EscGuard.cpp): a WH_MOUSE_LL hook that reads both mouse
// buttons globally AND eats them for the popup's lifetime - the popup
// window is only panel-sized, so raylib's own mouse events stop mattering
// the instant the cursor leaves it, and the commit/cancel click must never
// reach AE. Press/release latches stay set until consumed so a fast click
// can't fall between two frames. Begin degrades to a no-op if the hook
// can't install (MouseGuardInstalled() = false) - callers then fall back
// to raylib's window-local mouse events.
void BeginMouseGuard();
void EndMouseGuard();
bool MouseGuardInstalled();
bool MouseGuardConsumeLeftPress();
bool MouseGuardConsumeLeftRelease();
bool MouseGuardConsumeRightPress();

// Hook-free global input reads for the popup loop: cursor position in
// virtual-desktop coordinates (GetCursorPos) and modifier state
// (GetAsyncKeyState) - unlike raylib's events, neither needs the popup
// window to be focused, which Windows' foreground-lock may silently deny.
void OverlayCursorPos(int* x, int* y);
bool OverlayAltDown();
bool OverlayShiftDown();
bool OverlayCtrlDown();

// Fallback for the hook-failed path: blocks (bounded, ~1s) until Escape is
// physically up (GetAsyncKeyState), so the cancel script isn't started while
// AE can still see the key down. Returns immediately when the guard ate the
// key (the async state never saw it then).
void WaitForEscapeReleased();

// Absolute path to the popup font, resolved from this plugin DLL's own
// module path (HotkeyOverlay_Win32.cpp - the windows.h side) since
// LoadFontEx (the raylib.h side) needs an absolute path either way.
const char* GetFontPath();

// Absolute path to a monospace font for the slider's numeric readout -
// Consolas from the system fonts folder (present on every Windows), so no
// second font ships with the plugin. Callers fall back to the main font if
// the load fails.
const char* GetMonoFontPath();

// Same module-relative resolution as GetFontPath(), but under Icons/ -
// nanosvg (the raylib.h side) needs an absolute path to the .svg file too.
const char* GetIconPath(const char* filename);

// Module-relative path to barbar-jsx.js, the compiled src/jsx bundle that
// build.ps1 ships next to the .aex - the ScriptRunner $.evalFile's it into
// AE's ExtendScript engine on the first jsx call of the session. Not a popup
// concern, but it lives with the other module-relative lookups (FontPath.cpp).
const char* GetJsxBundlePath();

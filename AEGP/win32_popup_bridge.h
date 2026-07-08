#pragma once

// Bridges the windows.h-only side (HotkeyOverlay_Win32.cpp) and the
// raylib.h-only side (HotkeyOverlay_Popup.cpp). windows.h and raylib.h both
// redefine CloseWindow/ShowCursor/etc, so they can never share a translation
// unit - this header carries no types from either.
//
// mouseX/mouseY are global (virtual-desktop) cursor coordinates. monX/monY/
// monW/monH is the rect of the monitor under the cursor (MonitorFromPoint on
// the windows.h side) - the overlay window covers exactly that monitor, so
// the popup follows the cursor onto secondary displays instead of being
// pinned to the primary.
void RunPopupAtCursor(int mouseX, int mouseY, int monX, int monY, int monW, int monH);

// Auto-dismissing toast (no click needed) for when the Easing shortcut
// fires with nothing selected - fades out over ~1s at the cursor.
void RunNoSelectionToast(int mouseX, int mouseY, int monX, int monY, int monW, int monH);

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

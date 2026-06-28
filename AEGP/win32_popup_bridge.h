#pragma once

// Bridges the windows.h-only side (HotkeyOverlay_Win32.cpp) and the
// raylib.h-only side (HotkeyOverlay_Popup.cpp). windows.h and raylib.h both
// redefine CloseWindow/ShowCursor/etc, so they can never share a translation
// unit - this header carries no types from either.
void RunPopupAtCursor(int mouseX, int mouseY, int screenW, int screenH);

// Auto-dismissing toast (no click needed) for when the Easing shortcut
// fires with nothing selected - fades out over ~1s at the cursor.
void RunNoSelectionToast(int mouseX, int mouseY, int screenW, int screenH);

void ClosePopupWindow();

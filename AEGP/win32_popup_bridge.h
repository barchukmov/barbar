#pragma once

// Bridges the windows.h-only side (HotkeyOverlay_Win32.cpp) and the
// raylib.h-only side (HotkeyOverlay_Popup.cpp). windows.h and raylib.h both
// redefine CloseWindow/ShowCursor/etc, so they can never share a translation
// unit - this header carries no types from either.
void RunPopupAtCursor(int mouseX, int mouseY);
void ForceForeground(void* hwnd);

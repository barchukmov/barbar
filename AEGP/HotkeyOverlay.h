#pragma once

// Runs the ctrl+h popup overlay on a background thread inside this DLL.
// No Windows or raylib types here so AegpDemo.cpp can call these without
// pulling in either header (they clash with each other - see HotkeyOverlay_Win32.cpp).
void StartHotkeyOverlay();
void StopHotkeyOverlay();

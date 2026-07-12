#pragma once

// Runs the configurable-hotkey popup overlay on a background thread inside
// this DLL. No Windows or raylib types here so AegpDemo.cpp can call these
// without pulling in either header (they clash with each other - see
// HotkeyOverlay_Win32.cpp).
//
// The hotkey table (%APPDATA%\barbar-hotkeys.json, written only by CEP) is
// read at overlay startup - hotkeys are live from AE launch with no CEP
// running - and re-read whenever the overlay thread's directory watch sees
// the file change. Nothing needs to ping this side about edits.
void StartHotkeyOverlay();
void StopHotkeyOverlay();

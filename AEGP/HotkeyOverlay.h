#pragma once
#include <string>

// Runs the configurable-hotkey popup overlay on a background thread inside
// this DLL. No Windows or raylib types here so AegpDemo.cpp can call these
// without pulling in either header (they clash with each other - see
// HotkeyOverlay_Win32.cpp).
void StartHotkeyOverlay();
void StopHotkeyOverlay();

// Re-reads %APPDATA%\barbar-hotkeys.json and replaces the whole hotkey
// registration with it. The file is the single source of truth for the
// table: CEP is its only writer (ws-server.ts), this side only reads - at
// overlay startup (so hotkeys are live from AE launch, no CEP needed) and
// whenever CEP pings "hotkeysChanged" after an edit. Safe to call from any
// thread - hands off to the overlay thread internally, since
// RegisterHotKey/UnregisterHotKey must run on the thread owning the message
// loop that receives WM_HOTKEY.
void ReloadHotkeyTableFromFile();

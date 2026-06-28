#pragma once
#include <string>

// Runs the configurable-hotkey popup overlay on a background thread inside
// this DLL. No Windows or raylib types here so AegpDemo.cpp can call these
// without pulling in either header (they clash with each other - see
// HotkeyOverlay_Win32.cpp).
void StartHotkeyOverlay();
void StopHotkeyOverlay();

// Replaces the whole hotkey registration with the table CEP just sent.
// payload is "id,vkey,mods,fn;..." (see ws-server.ts's encodeHotkeyPayload).
// Safe to call from any thread - hands off to the overlay thread internally,
// since RegisterHotKey/UnregisterHotKey must run on the thread owning the
// message loop that receives WM_HOTKEY.
void UpdateHotkeyTable(const std::string& payload);

// Standalone runner for the real popup UI (HotkeyOverlay_Popup.cpp, compiled
// directly into this exe) - lets the UI be iterated on by running an exe
// instead of rebuilding the .aex and reloading it inside AE.
//
// argv: mouseX mouseY screenW screenH [toast|recreate]
#include "win32_popup_bridge.h"
#include <stdlib.h>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int main(int argc, char** argv)
{
	// Opt the whole process out of Win11 power throttling (EcoQoS + timer-
	// resolution coarsening). This exe never owns the foreground window (the
	// popup is a no-activate toolwindow), and Windows 11 throttles the timers
	// of foreground-less processes so hard that the Sleep inside raylib's
	// SetTargetFPS frame pacing stretches to *seconds* - the popup loop
	// visibly froze ~2s after open and woke on mouse input. Process-level
	// because the timer-resolution control has no per-thread equivalent;
	// inside AfterFX.exe this isn't needed (AE must be foreground for a
	// hotkey to fire at all, and a foreground process isn't throttled).
	PROCESS_POWER_THROTTLING_STATE pts = {};
	pts.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
	pts.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED |
	                  PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
	pts.StateMask = 0;
	SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &pts,
	                      sizeof(pts));
	int mouseX  = argc > 1 ? atoi(argv[1]) : 400;
	int mouseY  = argc > 2 ? atoi(argv[2]) : 300;
	int screenW = argc > 3 ? atoi(argv[3]) : 1920;
	int screenH = argc > 4 ? atoi(argv[4]) : 1080;

	// The exe stands in for the windows.h side's MonitorFromPoint lookup with
	// a monitor at the origin sized by argv - good enough for UI iteration.
	// POPUP_MONX/POPUP_MONY move that fake monitor's origin, e.g. onto a
	// secondary display (the mon rect is what the panel window gets clamped
	// into).
	int monX = getenv("POPUP_MONX") ? atoi(getenv("POPUP_MONX")) : 0;
	int monY = getenv("POPUP_MONY") ? atoi(getenv("POPUP_MONY")) : 0;
	std::string mode = argc > 5 ? argv[5] : "";
	if (mode == "toast") {
		RunNoSelectionToast(mouseX, mouseY, monX, monY, screenW, screenH);
	} else if (mode == "recreate") {
		// Two full open->close window cycles in one process - exercises the
		// per-popup teardown/recreate path without needing AE. The second
		// toast must come up just as transparent as the first.
		RunNoSelectionToast(mouseX, mouseY, monX, monY, screenW, screenH);
		RunNoSelectionToast(mouseX, mouseY, monX, monY, screenW, screenH);
	} else {
		RunPopupAtCursor(mouseX, mouseY, monX, monY, screenW, screenH);
	}

	return 0;
}

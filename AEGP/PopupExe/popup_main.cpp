// Standalone runner for the real popup UI (HotkeyOverlay_Popup.cpp, compiled
// directly into this exe) - lets the UI be iterated on by running an exe
// instead of rebuilding the .aex and reloading it inside AE.
//
// argv: mouseX mouseY screenW screenH [toast|recreate]
#include "win32_popup_bridge.h"
#include <stdlib.h>
#include <string>

int main(int argc, char** argv)
{
	int mouseX  = argc > 1 ? atoi(argv[1]) : 400;
	int mouseY  = argc > 2 ? atoi(argv[2]) : 300;
	int screenW = argc > 3 ? atoi(argv[3]) : 1920;
	int screenH = argc > 4 ? atoi(argv[4]) : 1080;

	// The exe stands in for the windows.h side's MonitorFromPoint lookup with
	// a monitor at the origin sized by argv - good enough for UI iteration.
	std::string mode = argc > 5 ? argv[5] : "";
	if (mode == "toast") {
		RunNoSelectionToast(mouseX, mouseY, 0, 0, screenW, screenH);
	} else if (mode == "recreate") {
		// Two full open->close window cycles in one process - exercises the
		// per-popup teardown/recreate path without needing AE. The second
		// toast must come up just as transparent as the first.
		RunNoSelectionToast(mouseX, mouseY, 0, 0, screenW, screenH);
		RunNoSelectionToast(mouseX, mouseY, 0, 0, screenW, screenH);
	} else {
		RunPopupAtCursor(mouseX, mouseY, 0, 0, screenW, screenH);
	}

	return 0;
}

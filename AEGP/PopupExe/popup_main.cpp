// Standalone runner for the real popup UI (HotkeyOverlay_Popup.cpp, compiled
// directly into this exe) - lets the UI be iterated on by running an exe
// instead of rebuilding the .aex and reloading it inside AE.
//
// argv: mouseX mouseY screenW screenH [toast]
#include "win32_popup_bridge.h"
#include <stdlib.h>
#include <string>

int main(int argc, char** argv)
{
	int mouseX  = argc > 1 ? atoi(argv[1]) : 400;
	int mouseY  = argc > 2 ? atoi(argv[2]) : 300;
	int screenW = argc > 3 ? atoi(argv[3]) : 1920;
	int screenH = argc > 4 ? atoi(argv[4]) : 1080;

	if (argc > 5 && std::string(argv[5]) == "toast") {
		RunNoSelectionToast(mouseX, mouseY, screenW, screenH);
	} else {
		RunPopupAtCursor(mouseX, mouseY, screenW, screenH);
	}

	ClosePopupWindow();
	return 0;
}

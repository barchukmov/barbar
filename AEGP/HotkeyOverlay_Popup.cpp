#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "win32_popup_bridge.h"

void RunPopupAtCursor(int mouseX, int mouseY)
{
	SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST);
	InitWindow(200, 200, "popup");
	SetWindowPosition(mouseX - 100, mouseY - 100);
	SetTargetFPS(60);
	ForceForeground(GetWindowHandle()); // a thread inside AE's process doesn't get focus for free either

	int framesAlive = 0;
	while (!WindowShouldClose() && (framesAlive++ < 10 || IsWindowFocused())) {
		BeginDrawing();
		ClearBackground(RAYWHITE);
		DrawText("Ctrl+H pressed!", 10, 20, 18, BLACK);
		Rectangle closeBtn = { 50, 140, 100, 30 };
		if (GuiButton(closeBtn, "Close")) break;
		EndDrawing();
	}

	CloseWindow();
}

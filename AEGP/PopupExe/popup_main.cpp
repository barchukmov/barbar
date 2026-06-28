// Standalone transparent slider popup. One process per popup: spawned fresh,
// runs once, prints the chosen value to stdout, exits. A fresh process means
// raylib's global state is always pristine - the in-process version corrupted
// it on reshow (opaque on 2nd popup, gone by the 3rd).
//
// argv: mouseX mouseY screenW screenH   (the AEGP side has windows.h; we don't)
// stdout: the slider value (0-100) if the user clicked; nothing if dismissed.
// exit code: 0 = clicked, 1 = dismissed (Esc / no click).
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char** argv)
{
	int mouseX  = argc > 1 ? atoi(argv[1]) : 400;
	int mouseY  = argc > 2 ? atoi(argv[2]) : 300;
	int screenW = argc > 3 ? atoi(argv[3]) : 1920;
	int screenH = argc > 4 ? atoi(argv[4]) : 1080;

	const Color kBorder = { 0x00, 0x00, 0x00, 255 };
	const Color kBg     = { 0x1d, 0x1d, 0x1d, 255 };
	const Color kText   = { 0xd0, 0xd0, 0xd0, 255 };

	SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST | FLAG_WINDOW_TRANSPARENT);
	// Overscan 1px on every side. A borderless window whose client area exactly
	// matches the monitor triggers Windows' fullscreen optimization, which drops
	// DWM compositing - and without DWM the transparent framebuffer renders
	// opaque (black). Spilling a pixel past each edge keeps it an ordinary
	// composited window, so transparency survives. ponytail: 1px overscan is the
	// known fix; if a multi-monitor setup ever needs exact bounds, revisit.
	InitWindow(screenW + 2, screenH + 2, "popup");
	SetWindowPosition(-1, -1);
	SetTargetFPS(60);

	GuiSetStyle(SLIDER, BORDER_COLOR_NORMAL,  ColorToInt(kBorder));
	GuiSetStyle(SLIDER, BORDER_COLOR_FOCUSED, ColorToInt(kBorder));
	GuiSetStyle(SLIDER, BORDER_COLOR_PRESSED, ColorToInt(kBorder));
	GuiSetStyle(SLIDER, BASE_COLOR_NORMAL,    ColorToInt(kBg));
	GuiSetStyle(SLIDER, TEXT_COLOR_NORMAL,    ColorToInt(kText));
	GuiSetStyle(SLIDER, TEXT_COLOR_FOCUSED,   ColorToInt(kText));
	GuiSetStyle(SLIDER, TEXT_COLOR_PRESSED,   ColorToInt(kText));

	const float kPanelW = 220, kPanelH = 50, kPadding = 15, kRadiusPx = 5;
	const float kRoundness = (2 * kRadiusPx) / (kPanelW < kPanelH ? kPanelW : kPanelH);
	Rectangle panel = { (float)(mouseX - kPanelW / 2), (float)(mouseY - kPanelH / 2), kPanelW, kPanelH };
	Rectangle sliderBounds = { panel.x + kPadding, panel.y + kPadding, kPanelW - 2 * kPadding, kPanelH - 2 * kPadding };
	float sliderValue = 0.0f;
	bool clicked = false;

	while (!WindowShouldClose()) {
		BeginDrawing();
		ClearBackground(BLANK);
		DrawRectangleRounded(panel, kRoundness, 0, kBg);
		DrawRectangleRoundedLinesEx(panel, kRoundness, 0, 2, kBorder);
		GuiSliderPro(sliderBounds, NULL, NULL, &sliderValue, 0.0f, 100.0f, GuiGetStyle(SLIDER, SLIDER_WIDTH), true);
		bool clickedThisFrame = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
		EndDrawing();
		if (clickedThisFrame) { clicked = true; break; }
	}

	CloseWindow();
	if (clicked) printf("%d\n", (int)sliderValue);
	return clicked ? 0 : 1;
}

#include "raylib.h"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996 4267) // raygui.h: plain sprintf/fopen/sscanf, size_t->int narrowing - vendored, not ours to fix
#endif
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include "win32_popup_bridge.h"
#include "WsClient.h"
#include <string>

namespace {
	const Color kBorder = { 0x00, 0x00, 0x00, 255 };
	const Color kBg = { 0x1d, 0x1d, 0x1d, 255 };
	const Color kText = { 0xd0, 0xd0, 0xd0, 255 };
}

namespace {
	bool g_windowReady = false;
}

void RunPopupAtCursor(int mouseX, int mouseY, int screenW, int screenH)
{
	// Fullscreen + transparent: a normal small window stops getting mouse-move
	// events the instant the cursor leaves its client area (no capture), which
	// froze the slider at the window edge. Covering the whole screen means the
	// cursor is always "inside" - the visible panel below is just a drawn
	// shape, not the window bounds.
	//
	// The window is created once and hidden/shown after that rather than
	// Init/CloseWindow per popup - raylib's GLFW backend doesn't reliably
	// reapply some window flags (transparency was the visible symptom) when a
	// window is recreated in the same process.
	if (!g_windowReady) {
		SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST | FLAG_WINDOW_TRANSPARENT);
		// Create at final size - GLFW wires up the transparent framebuffer/DWM
		// surface at creation, and resizing afterward (the old 1x1 -> SetWindowSize)
		// doesn't re-establish it, so the window came up opaque.
		// Overscan 1px on every side: a borderless window whose client area
		// exactly matches the monitor triggers Windows' fullscreen optimization,
		// dropping DWM compositing and rendering the transparent framebuffer
		// opaque. ponytail: 1px overscan is the fix.
		InitWindow(screenW + 2, screenH + 2, "popup");
		SetWindowPosition(-1, -1);
		SetTargetFPS(60);

		GuiSetStyle(SLIDER, BORDER_COLOR_NORMAL, ColorToInt(kBorder));
		GuiSetStyle(SLIDER, BORDER_COLOR_FOCUSED, ColorToInt(kBorder));
		GuiSetStyle(SLIDER, BORDER_COLOR_PRESSED, ColorToInt(kBorder));
		GuiSetStyle(SLIDER, BASE_COLOR_NORMAL, ColorToInt(kBg));
		GuiSetStyle(SLIDER, TEXT_COLOR_NORMAL, ColorToInt(kText));
		GuiSetStyle(SLIDER, TEXT_COLOR_FOCUSED, ColorToInt(kText));
		GuiSetStyle(SLIDER, TEXT_COLOR_PRESSED, ColorToInt(kText)); // slider handle color while forceDragging (always STATE_PRESSED)

		g_windowReady = true;
	} else {
		ClearWindowState(FLAG_WINDOW_HIDDEN);
	}
	const float kPanelW = 220, kPanelH = 50, kPadding = 15, kRadiusPx = 5;
	const float kRoundness = (2 * kRadiusPx) / (kPanelW < kPanelH ? kPanelW : kPanelH);

	Rectangle panel = { (float)(mouseX - kPanelW / 2), (float)(mouseY - kPanelH / 2), kPanelW, kPanelH };
	Rectangle sliderBounds = { panel.x + kPadding, panel.y + kPadding, kPanelW - 2 * kPadding, kPanelH - 2 * kPadding };
	float sliderValue = 0.0f;
	bool clicked = false;

	// Dismissal is explicit (click below, or Esc via WindowShouldClose) - not
	// focus-based. A background thread re-showing an already-existing window
	// can get silently denied by Windows' foreground-lock policy (unlike a
	// freshly created one), which made IsWindowFocused() go false almost
	// immediately and hide the popup ~10 frames in regardless of the user.
	while (!WindowShouldClose()) {
		BeginDrawing();
		ClearBackground(BLANK);
		DrawRectangleRounded(panel, kRoundness, 0, kBg);
		DrawRectangleRoundedLinesEx(panel, kRoundness, 0, 2, kBorder);
		// forceDragging=true: tracks the cursor every frame, no click/hold needed
		GuiSliderPro(sliderBounds, NULL, NULL, &sliderValue, 0.0f, 100.0f, GuiGetStyle(SLIDER, SLIDER_WIDTH), true);
		bool clickedThisFrame = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
		EndDrawing();

		if (clickedThisFrame) {
			clicked = true;
			break;
		}
	}

	SetWindowState(FLAG_WINDOW_HIDDEN);

	if (clicked) {
		WsSend("{\"type\":\"slider\",\"value\":" + std::to_string((int)sliderValue) + "}");
	}
}

void ClosePopupWindow()
{
	// Must run on the overlay thread (the one that called InitWindow) - GLFW
	// windows aren't safe to tear down cross-thread.
	if (g_windowReady) {
		CloseWindow();
		g_windowReady = false;
	}
}

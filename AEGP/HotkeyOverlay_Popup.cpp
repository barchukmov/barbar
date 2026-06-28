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

	// Fullscreen + transparent: a normal small window stops getting mouse-move
	// events the instant the cursor leaves its client area (no capture), which
	// froze the slider at the window edge. Covering the whole screen means the
	// cursor is always "inside" - the visible panel below is just a drawn
	// shape, not the window bounds.
	//
	// The window is created once and hidden/shown after that rather than
	// Init/CloseWindow per popup - raylib's GLFW backend doesn't reliably
	// reapply some window flags (transparency was the visible symptom) when a
	// window is recreated in the same process. Shared by every popup/toast
	// variant below.
	void EnsureWindowReady(int screenW, int screenH)
	{
		if (g_windowReady) {
			ClearWindowState(FLAG_WINDOW_HIDDEN);
			return;
		}

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
	}
}

void RunPopupAtCursor(int mouseX, int mouseY, int screenW, int screenH)
{
	EnsureWindowReady(screenW, screenH);

	const float kPanelW = 220, kPanelH = 84, kPadding = 15, kRadiusPx = 5;
	const float kRoundness = (2 * kRadiusPx) / (kPanelW < kPanelH ? kPanelW : kPanelH);

	Rectangle panel = { (float)(mouseX - kPanelW / 2), (float)(mouseY - kPanelH / 2), kPanelW, kPanelH };
	Rectangle sliderBounds = { panel.x + kPadding, panel.y + kPadding, kPanelW - 2 * kPadding, 20 };
	Rectangle holdButtonBounds = { panel.x + kPadding, sliderBounds.y + sliderBounds.height + 10, kPanelW - 2 * kPadding, 24 };
	float sliderValue = 0.0f;
	bool clicked = false;
	bool holdOutgoing = false;
	const char* mode = "both";

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

		// GuiButton fires on mouse-release-while-hovering, not on press - so
		// a press that lands on the button must be excluded from the generic
		// "any click dismisses" check below, or the popup closes on the press
		// frame before the button ever gets its own release-frame click.
		bool overButton = CheckCollisionPointRec(GetMousePosition(), holdButtonBounds);
		bool holdButtonClicked = GuiButton(holdButtonBounds, "Hold Out");
		bool dismissClick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !overButton;

		// Read every frame, not just at commit, so the held modifier always
		// matches what's drawn.
		bool ctrlHeld = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
		bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
		mode = ctrlHeld ? "in" : (shiftHeld ? "out" : "both");
		if (mode[0] != 'b') { // "in" or "out" - "both" needs no label
			DrawText(mode[0] == 'i' ? "In only" : "Out only", (int)panel.x, (int)panel.y - 18, 14, kText);
		}
		EndDrawing();

		if (holdButtonClicked) {
			holdOutgoing = true;
			break;
		}
		if (dismissClick) {
			clicked = true;
			break;
		}
	}

	SetWindowState(FLAG_WINDOW_HIDDEN);

	if (holdOutgoing) {
		WsSend(R"({"type":"holdOutgoing"})");
	} else if (clicked) {
		WsSend(std::string(R"({"type":"slider","value":)") + std::to_string((int)sliderValue) + R"(,"mode":")" + mode + "\"}");
	}
}

void RunNoSelectionToast(int mouseX, int mouseY, int screenW, int screenH)
{
	EnsureWindowReady(screenW, screenH);

	const float kPanelW = 240, kPanelH = 40, kRadiusPx = 5;
	const float kRoundness = (2 * kRadiusPx) / (kPanelW < kPanelH ? kPanelW : kPanelH);
	Rectangle panel = { (float)(mouseX - kPanelW / 2), (float)(mouseY - kPanelH / 2), kPanelW, kPanelH };

	const float kLifetimeSec = 1.0f;
	float elapsed = 0.0f;
	while (!WindowShouldClose() && elapsed < kLifetimeSec) {
		float alpha = 1.0f - (elapsed / kLifetimeSec);
		BeginDrawing();
		ClearBackground(BLANK);
		DrawRectangleRounded(panel, kRoundness, 0, Fade(kBg, alpha));
		DrawRectangleRoundedLinesEx(panel, kRoundness, 0, 2, Fade(kBorder, alpha));
		DrawText("No keyframes are selected", (int)panel.x + 12, (int)(panel.y + panel.height / 2 - 5), 10, Fade(kText, alpha));
		EndDrawing();

		if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) break; // explicit dismiss still works
		elapsed += GetFrameTime();
	}

	SetWindowState(FLAG_WINDOW_HIDDEN);
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

#include "raylib.h"
#include "win32_popup_bridge.h"
#include "WsClient.h"
#include <string>

namespace {
	const Color kBorder = { 0x00, 0x00, 0x00, 255 };
	const Color kBg = { 0x1d, 0x1d, 0x1d, 255 };
	const Color kText = { 0xd0, 0xd0, 0xd0, 255 };
	const Color kAccent = { 0x3b, 0x82, 0xf6, 255 }; // slider handle dot
	const float kRadiusPx = 5; // corner radius shared by every drawn box

	float Roundness(Rectangle r)
	{
		return (2 * kRadiusPx) / (r.width < r.height ? r.width : r.height);
	}
}

namespace {
	bool g_windowReady = false;
	Font g_font = { 0 };

	// Small icon button ("Hold Out"): a square split vertically into two
	// tones, evoking the in/out halves the action holds onto.
	bool DrawButton(Rectangle bounds)
	{
		bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
		DrawRectangleRounded(bounds, Roundness(bounds), 0, kBg);
		DrawRectangleRoundedLinesEx(bounds, Roundness(bounds), 0, 2, kBorder);

		const float kIconSize = 12;
		Rectangle icon = { bounds.x + (bounds.width - kIconSize) / 2, bounds.y + (bounds.height - kIconSize) / 2, kIconSize, kIconSize };
		DrawRectangleRec({ icon.x, icon.y, icon.width / 2, icon.height }, kText);
		DrawRectangleRec({ icon.x + icon.width / 2, icon.y, icon.width / 2, icon.height }, kBorder);
		DrawRectangleLinesEx(icon, 1, kBorder);

		// GuiButton's old behavior: fires on release-while-hovering, not on
		// press - so a press that lands on the button doesn't also trigger a
		// generic "any click dismisses" check elsewhere.
		return hovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
	}

	// AE's Bezier/Easy Ease keyframe glyph: an hourglass, two triangles
	// meeting at a point. Split vertically into a duotone left=in/right=out
	// half; whichever side the held modifier names brightens, the other dims.
	void DrawEasingIcon(Rectangle bounds, const char* mode)
	{
		const Color kBaseIn = { 0x80, 0x80, 0x80, 255 };
		const Color kBaseOut = { 0x55, 0x55, 0x55, 255 };
		Color inColor = kBaseIn, outColor = kBaseOut;
		if (mode[0] == 'i') { inColor = kText; outColor = Fade(kBaseOut, 0.5f); }
		else if (mode[0] == 'o') { outColor = kText; inColor = Fade(kBaseIn, 0.5f); }

		float midX = bounds.x + bounds.width / 2;
		Vector2 topL = { bounds.x, bounds.y };
		Vector2 topM = { midX, bounds.y };
		Vector2 topR = { bounds.x + bounds.width, bounds.y };
		Vector2 botL = { bounds.x, bounds.y + bounds.height };
		Vector2 botM = { midX, bounds.y + bounds.height };
		Vector2 botR = { bounds.x + bounds.width, bounds.y + bounds.height };
		Vector2 center = { midX, bounds.y + bounds.height / 2 };

		DrawTriangle(topL, center, topM, inColor);
		DrawTriangle(botL, botM, center, inColor);
		DrawTriangle(topM, center, topR, outColor);
		DrawTriangle(botM, botR, center, outColor);
	}

	// Single shape for every slider update sent to AE - the 250ms-polled
	// in-drag preview and the commit-on-dismiss message carry the same
	// value/mode, just under a different "type" so CEP can tell a live
	// preview ("polling") from the final commit ("accept") apart.
	void SendSliderUpdate(float value, const char* mode, bool polling)
	{
		WsSend(std::string(R"({"type":")") + (polling ? "polling" : "accept") + R"(","value":)"
			+ std::to_string((int)value) + R"(,"mode":")" + mode + "\"}");
	}

	// Thin round-capped track with a blue dot handle at position t (0-1).
	void DrawSliderTrack(Rectangle bounds, float t)
	{
		const float kThickness = 3;
		float y = bounds.y + bounds.height / 2;
		Vector2 left = { bounds.x + kThickness / 2, y };
		Vector2 right = { bounds.x + bounds.width - kThickness / 2, y };
		DrawLineEx(left, right, kThickness, kText);
		DrawCircleV(left, kThickness / 2, kText);
		DrawCircleV(right, kThickness / 2, kText);
		DrawCircleV({ bounds.x + t * bounds.width, y }, 6, kAccent);
	}

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
		// Esc must not drive WindowShouldClose(): that flag latches true
		// permanently once set and raylib exposes no way to clear it, so since
		// this window is reused across popups instead of recreated, the very
		// next popup's loop would see it pre-closed and exit on frame 0. Esc is
		// handled manually below instead.
		SetExitKey(KEY_NULL);

		// ponytail: load at one fixed size (14px) rather than a re-bakeable
		// size table - bump kFontSize/reload if the popup ever needs another size.
		const int kFontSize = 14;
		g_font = LoadFontEx(GetFontPath(), kFontSize, NULL, 0);
		if (g_font.texture.id != 0) {
			// Bilinear, not point (raygui/raylib default): the manual DrawTextEx
			// calls below scale this font off its baked size (e.g. 10px toast vs
			// 14px bake), and point-sampling a bitmap font off-scale drops whole
			// glyph strokes instead of just looking jagged.
			SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);
		}

		g_windowReady = true;
	}
}

void RunPopupAtCursor(int mouseX, int mouseY, int screenW, int screenH)
{
	EnsureWindowReady(screenW, screenH);

	const float kPanelW = 220, kPanelH = 40, kPadding = 10, kGap = 10, kButtonSize = 20, kIconSize = 16;

	Rectangle panel = { (float)(mouseX - kPanelW / 2), (float)(mouseY - kPanelH / 2), kPanelW, kPanelH };
	Rectangle holdButtonBounds = { panel.x + kPanelW - kPadding - kButtonSize, panel.y + (kPanelH - kButtonSize) / 2, kButtonSize, kButtonSize };
	Rectangle easingIconBounds = { panel.x + kPadding, panel.y + (kPanelH - kIconSize) / 2, kIconSize, kIconSize };
	Rectangle sliderBounds = { easingIconBounds.x + kIconSize + kGap, panel.y + (kPanelH - 16) / 2, holdButtonBounds.x - kGap - (easingIconBounds.x + kIconSize + kGap), 16 };
	float sliderValue = 0.0f;
	float lastSentValue = -1.0f; // unreachable slider value, forces the first poll tick to send
	float sendTimer = 0.0f;
	bool clicked = false;
	bool holdOutgoing = false;
	const char* mode = "both";

	// Alt = precision drag: freeze the value/mouseX pair at the moment Alt
	// goes down, then move at 1/kAltSlowdown speed relative to that anchor instead of
	// tracking the cursor directly. Releasing Alt snaps straight back to
	// normal (direct cursor tracking) - no re-anchoring on release.
	bool altWasDown = false;
	float altAnchorValue = 0.0f, altAnchorMouseX = 0.0f;

	// Dismissal is explicit (click below, or Esc) - not focus-based. A
	// background thread re-showing an already-existing window can get
	// silently denied by Windows' foreground-lock policy (unlike a freshly
	// created one), which made IsWindowFocused() go false almost immediately
	// and hide the popup ~10 frames in regardless of the user.
	bool escPressed = false;
	while (!WindowShouldClose() && !escPressed) {
		BeginDrawing();
		ClearBackground(BLANK);
		DrawRectangleRounded(panel, Roundness(panel), 0, kBg);
		DrawRectangleRoundedLinesEx(panel, Roundness(panel), 0, 2, kBorder);

		// Read every frame, not just at commit, so the held modifier always
		// matches what's drawn.
		bool ctrlHeld = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
		bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
		mode = ctrlHeld ? "in" : (shiftHeld ? "out" : "both");
		DrawEasingIcon(easingIconBounds, mode);

		bool altDown = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
		float mouseX = GetMousePosition().x;
		if (altDown && !altWasDown) {
			altAnchorValue = sliderValue;
			altAnchorMouseX = mouseX;
		}
		altWasDown = altDown;

		const float kAltSlowdown = 30.0f;
		float t = altDown
			? altAnchorValue / 100.0f + (mouseX - altAnchorMouseX) / kAltSlowdown / sliderBounds.width
			: (mouseX - sliderBounds.x) / sliderBounds.width;
		t = t < 0 ? 0 : (t > 1 ? 1 : t);
		sliderValue = t * 100.0f;
		DrawSliderTrack(sliderBounds, t);

		// Live preview to AE while dragging, separate from the commit-on-dismiss
		// message below - polled rather than sent every frame so a fast drag
		// doesn't flood the socket.
		sendTimer += GetFrameTime();
		if (sendTimer >= 0.25f) {
			sendTimer = 0.0f;
			if (sliderValue != lastSentValue) {
				SendSliderUpdate(sliderValue, mode, true);
				lastSentValue = sliderValue;
			}
		}

		// DrawButton fires on mouse-release-while-hovering, not on press - so
		// a press that lands on the button must be excluded from the generic
		// "any click dismisses" check below, or the popup closes on the press
		// frame before the button ever gets its own release-frame click.
		bool overButton = CheckCollisionPointRec(GetMousePosition(), holdButtonBounds);
		bool holdButtonClicked = DrawButton(holdButtonBounds);
		bool dismissClick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !overButton;
		EndDrawing();

		if (holdButtonClicked) {
			holdOutgoing = true;
			break;
		}
		if (dismissClick) {
			clicked = true;
			break;
		}
		if (IsKeyPressed(KEY_ESCAPE)) {
			escPressed = true;
		}
	}

	SetWindowState(FLAG_WINDOW_HIDDEN);

	if (holdOutgoing) {
		WsSend(R"({"type":"holdOutgoing"})");
	} else if (clicked) {
		SendSliderUpdate(sliderValue, mode, false);
	} else {
		// Loop exits without holdOutgoing/clicked via Esc - tell CEP to put the
		// keyframes back the way it found them (it kept the pre-drag snapshot
		// from the preview ticks).
		WsSend(R"({"type":"cancel"})");
	}
}

void RunNoSelectionToast(int mouseX, int mouseY, int screenW, int screenH)
{
	EnsureWindowReady(screenW, screenH);

	const float kPanelW = 240, kPanelH = 40;
	Rectangle panel = { (float)(mouseX - kPanelW / 2), (float)(mouseY - kPanelH / 2), kPanelW, kPanelH };

	const float kLifetimeSec = 1.0f;
	float elapsed = 0.0f;
	while (elapsed < kLifetimeSec) {
		float alpha = 1.0f - (elapsed / kLifetimeSec);
		BeginDrawing();
		ClearBackground(BLANK);
		DrawRectangleRounded(panel, Roundness(panel), 0, Fade(kBg, alpha));
		DrawRectangleRoundedLinesEx(panel, Roundness(panel), 0, 2, Fade(kBorder, alpha));
		DrawTextEx(g_font, "No keyframes are selected", { panel.x + 12, panel.y + panel.height / 2 - 5 }, 10, 0, Fade(kText, alpha));
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

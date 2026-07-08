#include "WsClient.h"
#include "raylib.h"
#include "win32_popup_bridge.h"
#include <cstdlib>
#include <string>

// nanosvg/nanosvgrast - raylib itself shipped an SVG loader (LoadImageSvg)
// built on this same pair before removing it in 6.0 ("moved to
// raylib-extras"). Vendored directly here instead of chasing that repo,
// since it's exactly this header pair with no other dependencies.
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

namespace {
const Color kBorder = {0x00, 0x00, 0x00, 255};
const Color kBg = {0x1d, 0x1d, 0x1d, 255};
const Color kBgHover = {0x32, 0x32, 0x32, 255};
const Color kText = {0xd0, 0xd0, 0xd0, 255};
const Color kAccent = {0x3b, 0x82, 0xf6, 255}; // slider handle dot
const float kRadiusPx = 5;  // corner radius shared by every drawn box
const float kBorderPx = 2;  // border width shared by every drawn box

float Roundness(Rectangle r, float radiusPx) {
  return (2 * radiusPx) / (r.width < r.height ? r.width : r.height);
}

// Bordered rounded box drawn as two filled rounded rects - the border as
// the outer fill, the background inset by the border width on top. A fill
// plus DrawRectangleRoundedLinesEx never tessellate their corner arcs
// identically, which left the backdrop showing through the rounding.
void DrawPanelBox(Rectangle r, Color bg, Color border) {
  DrawRectangleRounded(r, Roundness(r, kRadiusPx), 0, border);
  Rectangle inner = {r.x + kBorderPx, r.y + kBorderPx, r.width - 2 * kBorderPx,
                     r.height - 2 * kBorderPx};
  DrawRectangleRounded(inner, Roundness(inner, kRadiusPx - kBorderPx), 0, bg);
}
} // namespace

namespace {
Font g_font = {0};
Font g_monoFont = {0}; // slider numeric readout; falls back to g_font
// Each icon is a whole, already-colored SVG (e.g. In.svg highlights its
// left/blue half itself) rather than a shape this code tints at runtime -
// the easing icon just swaps which of the textures it draws as the held
// modifier (or a zero slider value) changes.
Texture2D g_holdButtonIconTex = {0};
Texture2D g_easingInTex = {0};
Texture2D g_easingOutTex = {0};
Texture2D g_easingBothTex = {0};
Texture2D g_easingLinearTex = {0};

// Rasterizes an .svg (resolved via GetIconPath, module-relative) to a
// sizePx x sizePx RGBA texture. Returns a zeroed Texture2D (id 0) on any
// load failure - callers just skip drawing it.
Texture2D LoadSvgIconTexture(const char *filename, int sizePx) {
  NSVGimage *image = nsvgParseFromFile(GetIconPath(filename), "px", 96.0f);
  if (!image)
    return {0};

  NSVGrasterizer *rast = nsvgCreateRasterizer();
  float longestSide =
      image->width > image->height ? image->width : image->height;
  float scale = longestSide > 0 ? sizePx / longestSide : 1.0f;
  unsigned char *pixels = (unsigned char *)malloc((size_t)sizePx * sizePx * 4);
  nsvgRasterize(rast, image, 0, 0, scale, pixels, sizePx, sizePx, sizePx * 4);
  nsvgDeleteRasterizer(rast);
  nsvgDelete(image);

  Image img = {pixels, sizePx, sizePx, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  Texture2D tex = LoadTextureFromImage(img); // copies into GPU memory
  free(pixels);
  if (tex.id != 0)
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
  return tex;
}

void LoadIcons() {
  g_holdButtonIconTex = LoadSvgIconTexture("Block.svg", 32);
  g_easingInTex = LoadSvgIconTexture("In.svg", 32);
  g_easingOutTex = LoadSvgIconTexture("Out.svg", 32);
  g_easingBothTex = LoadSvgIconTexture("Nutral.svg", 32);
  g_easingLinearTex = LoadSvgIconTexture("Linear.svg", 32);
}

void DrawIconTexture(Texture2D tex, Rectangle bounds) {
  if (tex.id == 0)
    return;
  Rectangle src = {0, 0, (float)tex.width, (float)tex.height};
  DrawTexturePro(tex, src, bounds, {0, 0}, 0, WHITE);
}

// Small icon button ("Hold Out"): a square split vertically into two
// tones, evoking the in/out halves the action holds onto. Lightens its
// fill while hovered.
bool DrawButton(Rectangle bounds) {
  bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
  DrawPanelBox(bounds, hovered ? kBgHover : kBg, kBorder);

  // Same draw size as the easing icon on the panel's left, so the two
  // read as one icon family.
  const float kIconSize = 16;
  Rectangle icon = {bounds.x + (bounds.width - kIconSize) / 2,
                    bounds.y + (bounds.height - kIconSize) / 2, kIconSize,
                    kIconSize};
  DrawIconTexture(g_holdButtonIconTex, icon);

  // GuiButton's old behavior: fires on release-while-hovering, not on
  // press - so a press that lands on the button doesn't also trigger a
  // generic "any click dismisses" check elsewhere.
  return hovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
}

// AE's Bezier/Easy Ease keyframe glyph: an hourglass split blue/dark by
// which side the held modifier names, or the Linear diamond while the
// slider sits at 0 (no ease applied yet). Each state is its own
// pre-colored SVG rather than one shape tinted at runtime.
void DrawEasingIcon(Rectangle bounds, const char *mode, float sliderValue) {
  Texture2D tex = sliderValue <= 0.0f ? g_easingLinearTex
                  : mode[0] == 'i'    ? g_easingInTex
                  : mode[0] == 'o'    ? g_easingOutTex
                                      : g_easingBothTex;
  DrawIconTexture(tex, bounds);
}

// Single shape for every slider update sent to AE - the 250ms-polled
// in-drag preview and the commit-on-dismiss message carry the same
// value/mode, just under a different "type" so CEP can tell a live
// preview ("polling") from the final commit ("accept") apart.
void SendSliderUpdate(float value, const char *mode, bool polling) {
  WsSend(std::string(R"({"type":")") + (polling ? "polling" : "accept") +
         R"(","value":)" + std::to_string((int)value) + R"(,"mode":")" + mode +
         "\"}");
}

// Thin round-capped track with a blue dot handle at position t (0-1).
void DrawSliderTrack(Rectangle bounds, float t) {
  const float kThickness = 3;
  float y = bounds.y + bounds.height / 2;
  Vector2 left = {bounds.x + kThickness / 2, y};
  Vector2 right = {bounds.x + bounds.width - kThickness / 2, y};
  DrawLineEx(left, right, kThickness, kText);
  DrawCircleV(left, kThickness / 2, kText);
  DrawCircleV(right, kThickness / 2, kText);
  DrawCircleV({bounds.x + t * bounds.width, y}, 6, kAccent);
}

// Fullscreen + transparent: a normal small window stops getting mouse-move
// events the instant the cursor leaves its client area (no capture), which
// froze the slider at the window edge. Covering the whole monitor the cursor
// is on (monX/monY/monW/monH, resolved by the windows.h side) means the
// cursor is always "inside" - the visible panel below is just a drawn
// shape, not the window bounds.
//
// One window per popup: created here, fully torn down by CloseOverlayWindow
// when the popup dismisses, so every invocation starts raylib from the same
// blank state. This used to be a create-once/hide-show singleton, adopted
// when a recreated window came up opaque - but the actual culprits were the
// two creation-time details below (1x1-then-resize, exact-fit fullscreen
// optimization), not recreation itself. The singleton then needed hacks of
// its own (WindowShouldClose() latching true forever after one Esc, DWM
// transparency not reapplied on re-show, Windows' foreground-lock denying
// focus to a re-shown-but-not-fresh window) - all moot now that the window
// never outlives a single popup. Shared by every popup/toast variant below.
void OpenOverlayWindow(int monX, int monY, int monW, int monH) {
  SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST |
                 FLAG_WINDOW_TRANSPARENT);
  // Create at final size - GLFW wires up the transparent framebuffer/DWM
  // surface at creation, and resizing afterward (the old 1x1 -> SetWindowSize)
  // doesn't re-establish it, so the window came up opaque.
  // Overscan 1px on every side: a borderless window whose client area
  // exactly matches the monitor triggers Windows' fullscreen optimization,
  // dropping DWM compositing and rendering the transparent framebuffer
  // opaque. ponytail: 1px overscan is the fix.
  InitWindow(monW + 2, monH + 2, "popup");
  SetWindowPosition(monX - 1, monY - 1);
  SetTargetFPS(60);

  // ponytail: load at one fixed size (14px) rather than a re-bakeable
  // size table - bump kFontSize/reload if the popup ever needs another size.
  const int kFontSize = 14;
  g_font = LoadFontEx(GetFontPath(), kFontSize, NULL, 0);
  if (g_font.texture.id != 0) {
    // Bilinear, not point (raylib default): the manual DrawTextEx
    // calls below scale this font off its baked size (e.g. 10px toast vs
    // 14px bake), and point-sampling a bitmap font off-scale drops whole
    // glyph strokes instead of just looking jagged.
    SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);
  }
  // Only ever drawn at its baked size, so it keeps raylib's default point
  // filtering - crispest for a small numeric readout.
  g_monoFont = LoadFontEx(GetMonoFontPath(), kFontSize, NULL, 0);

  LoadIcons();
}

// The slider's numeric readout font - Consolas if the system load worked,
// otherwise the main (non-mono) font so the readout never just vanishes.
const Font &NumberFont() {
  return g_monoFont.texture.id != 0 ? g_monoFont : g_font;
}

// GPU handles (textures, font atlas) die with the GL context, but unload
// them first anyway: UnloadFont also frees the CPU-side glyph data, and
// zeroed handles keep the next OpenOverlayWindow's state honest.
void CloseOverlayWindow() {
  UnloadTexture(g_holdButtonIconTex);
  g_holdButtonIconTex = {0};
  UnloadTexture(g_easingInTex);
  g_easingInTex = {0};
  UnloadTexture(g_easingOutTex);
  g_easingOutTex = {0};
  UnloadTexture(g_easingBothTex);
  g_easingBothTex = {0};
  UnloadTexture(g_easingLinearTex);
  g_easingLinearTex = {0};
  if (g_font.texture.id != 0)
    UnloadFont(g_font);
  g_font = {0};
  if (g_monoFont.texture.id != 0)
    UnloadFont(g_monoFont);
  g_monoFont = {0};
  CloseWindow();
}
} // namespace

void RunPopupAtCursor(int mouseX, int mouseY, int monX, int monY, int monW,
                      int monH) {
  OpenOverlayWindow(monX, monY, monW, monH);

  // The cursor arrives in global (virtual-desktop) coordinates; everything
  // drawn (and GetMousePosition in the loop) is window-local, with the
  // window's origin at the monitor's corner minus the 1px overscan.
  float localX = (float)(mouseX - (monX - 1));
  float localY = (float)(mouseY - (monY - 1));

  const float kPanelW = 260, kPanelH = 40, kPadding = 10, kGap = 10,
              kButtonSize = 24, kIconSize = 16, kNumberFontSize = 14;

  Rectangle panel = {localX - kPanelW / 2, localY - kPanelH / 2, kPanelW,
                     kPanelH};
  Rectangle holdButtonBounds = {panel.x + kPanelW - kPadding - kButtonSize,
                                panel.y + (kPanelH - kButtonSize) / 2,
                                kButtonSize, kButtonSize};
  // Fixed column sized for the widest value ("100"), right edge pinned -
  // the readout is right-justified into it so digits tick over without the
  // number wandering.
  float numberW = MeasureTextEx(NumberFont(), "100", kNumberFontSize, 0).x;
  float numberRight = holdButtonBounds.x - kGap;
  Rectangle easingIconBounds = {panel.x + kPadding,
                                panel.y + (kPanelH - kIconSize) / 2, kIconSize,
                                kIconSize};
  Rectangle sliderBounds = {
      easingIconBounds.x + kIconSize + kGap, panel.y + (kPanelH - 16) / 2,
      numberRight - numberW - kGap - (easingIconBounds.x + kIconSize + kGap),
      16};
  float sliderValue = 0.0f;
  float lastSentValue =
      -1.0f; // unreachable slider value, forces the first poll tick to send
  float sendTimer = 0.0f;
  bool clicked = false;
  bool holdOutgoing = false;
  const char *mode = "both";

  // Alt = precision drag: freeze the value/mouseX pair at the moment Alt
  // goes down, then move at 1/kAltSlowdown speed relative to that anchor
  // instead of tracking the cursor directly. Releasing Alt snaps straight back
  // to normal (direct cursor tracking) - no re-anchoring on release.
  bool altWasDown = false;
  float altAnchorValue = 0.0f, altAnchorMouseX = 0.0f;

  // Dismissal is explicit (click below, or Esc as raylib's default exit key
  // driving WindowShouldClose) - never focus-based: IsWindowFocused() can go
  // false through no action of the user's (Windows' foreground-lock policy),
  // which once had the popup dismissing itself ~10 frames in.
  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLANK);
    DrawPanelBox(panel, kBg, kBorder);

    // Read every frame, not just at commit, so the held modifier always
    // matches what's drawn.
    bool ctrlHeld = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    mode = ctrlHeld ? "in" : (shiftHeld ? "out" : "both");

    bool altDown = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    float mouseX = GetMousePosition().x;
    if (altDown && !altWasDown) {
      altAnchorValue = sliderValue;
      altAnchorMouseX = mouseX;
    }
    altWasDown = altDown;

    const float kAltSlowdown = 30.0f;
    float t = altDown ? altAnchorValue / 100.0f + (mouseX - altAnchorMouseX) /
                                                      kAltSlowdown /
                                                      sliderBounds.width
                      : (mouseX - sliderBounds.x) / sliderBounds.width;
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    sliderValue = t * 100.0f;

    // Icon drawn after the slider math so a value that just hit 0 swaps to
    // the Linear glyph on the same frame, not the next one.
    DrawEasingIcon(easingIconBounds, mode, sliderValue);
    DrawSliderTrack(sliderBounds, t);

    const char *valueText = TextFormat("%d", (int)sliderValue);
    float valueW = MeasureTextEx(NumberFont(), valueText, kNumberFontSize, 0).x;
    DrawTextEx(NumberFont(), valueText,
               {numberRight - valueW, panel.y + (kPanelH - kNumberFontSize) / 2},
               kNumberFontSize, 0, kText);

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
    bool overButton =
        CheckCollisionPointRec(GetMousePosition(), holdButtonBounds);
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
  }

  CloseOverlayWindow();

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

void RunNoSelectionToast(int mouseX, int mouseY, int monX, int monY, int monW,
                         int monH) {
  OpenOverlayWindow(monX, monY, monW, monH);

  float localX = (float)(mouseX - (monX - 1));
  float localY = (float)(mouseY - (monY - 1));

  const float kPanelW = 240, kPanelH = 40;
  Rectangle panel = {localX - kPanelW / 2, localY - kPanelH / 2, kPanelW,
                     kPanelH};

  const float kLifetimeSec = 1.0f;
  float elapsed = 0.0f;
  while (elapsed < kLifetimeSec) {
    float alpha = 1.0f - (elapsed / kLifetimeSec);
    BeginDrawing();
    ClearBackground(BLANK);
    DrawPanelBox(panel, Fade(kBg, alpha), Fade(kBorder, alpha));
    DrawTextEx(g_font, "No keyframes are selected",
               {panel.x + 12, panel.y + panel.height / 2 - 5}, 10, 0,
               Fade(kText, alpha));
    EndDrawing();

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || WindowShouldClose())
      break; // explicit dismiss still works
    elapsed += GetFrameTime();
  }

  CloseOverlayWindow();
}

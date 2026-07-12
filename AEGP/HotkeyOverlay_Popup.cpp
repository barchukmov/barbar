#include "PopupActions.h"
#include "raylib.h"
#include "win32_popup_bridge.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
const float kBorderPx = 1;  // border width shared by every drawn box

float Roundness(Rectangle r, float radiusPx) {
  return (2 * radiusPx) / (r.width < r.height ? r.width : r.height);
}

// Segments per corner arc, passed explicitly instead of 0 ("auto") - at our
// small radii (~5px) raylib's auto segment-count formula comes out to only
// 2-3 segments per 90 degrees, which draws as a single facet (a chamfered
// corner) rather than a curve.
const int kCornerSegments = 8;

// Bordered rounded box drawn as two filled rounded rects - the border as
// the outer fill, the background inset by the border width on top. A fill
// plus DrawRectangleRoundedLinesEx never tessellate their corner arcs
// identically, which left the backdrop showing through the rounding.
void DrawPanelBox(Rectangle r, Color bg, Color border) {
  DrawRectangleRounded(r, Roundness(r, kRadiusPx), kCornerSegments, border);
  Rectangle inner = {r.x + kBorderPx, r.y + kBorderPx, r.width - 2 * kBorderPx,
                     r.height - 2 * kBorderPx};
  DrawRectangleRounded(inner, Roundness(inner, kRadiusPx - kBorderPx),
                       kCornerSegments, bg);
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

// Mouse input for the popup comes from the mouse guard's low-level hook
// (EscGuard.cpp) - the window is only panel-sized, so raylib's own mouse
// events stop the moment the cursor leaves it. When the hook failed to
// install, degrade to raylib's view: clicks then only register over the
// panel window itself, and un-eaten clicks leak through to AE - imperfect,
// same philosophy as the Esc guard's degrade path.
bool GuardedLeftPressed() {
  if (MouseGuardInstalled()) return MouseGuardConsumeLeftPress();
  return IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}
bool GuardedLeftReleased() {
  if (MouseGuardInstalled()) return MouseGuardConsumeLeftRelease();
  return IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
}
bool GuardedRightPressed() {
  if (MouseGuardInstalled()) return MouseGuardConsumeRightPress();
  return IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
}

// Cursor in window-local coordinates, from the global cursor position (the
// window's origin is winX/winY, fixed for the popup's lifetime).
Vector2 GuardedCursorLocal(int winX, int winY) {
  int x, y;
  OverlayCursorPos(&x, &y);
  return {(float)(x - winX), (float)(y - winY)};
}

int ClampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Small icon button ("Hold Out"): a square split vertically into two
// tones, evoking the in/out halves the action holds onto. Lightens its
// fill while hovered. hovered/leftReleased come from the guarded input
// reads in the caller's frame, not raylib's window events.
bool DrawButton(Rectangle bounds, bool hovered, bool leftReleased) {
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
  return hovered && leftReleased;
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

// Single shape for every slider update applied to AE - the 250ms-polled
// in-drag preview and the commit-on-dismiss carry the same value/mode, just
// with a different isPreview flag so the jsx side knows when to close the
// drag session (see applyEase in src/jsx/aeft/aeft.ts).
void SendSliderUpdate(float value, const char *mode, bool polling) {
  PopupApplyEase((int)value, mode, polling);
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

// Raylib's trace log goes to stdout, which doesn't exist inside AfterFX.exe -
// this callback keeps the console line (for PopupExe) and mirrors every line
// to %TEMP%\barbar-popup.log + OutputDebugString via PopupDebugLog, so the
// display/GL/window-size diagnostics are readable from an AE run too.
void RaylibLogToDebug(int logLevel, const char *text, va_list args) {
  char line[1024];
  int n = 0;
  switch (logLevel) {
  case LOG_WARNING: n = snprintf(line, sizeof(line), "WARNING: "); break;
  case LOG_ERROR:   n = snprintf(line, sizeof(line), "ERROR: ");   break;
  case LOG_FATAL:   n = snprintf(line, sizeof(line), "FATAL: ");   break;
  case LOG_DEBUG:   n = snprintf(line, sizeof(line), "DEBUG: ");   break;
  default:          n = snprintf(line, sizeof(line), "INFO: ");    break;
  }
  vsnprintf(line + n, sizeof(line) - n, text, args);
  printf("%s\n", line);
  PopupDebugLog(line);
}

// Debug knobs for the window setup, read fresh at every popup open from
// %TEMP%\barbar-popup-knobs.txt (key=value lines, e.g. "topmost=0"). Every
// knob defaults to 1 - the full implementation; a missing file or key
// changes nothing. Setting knobs to 0 peels the layers off one by one
// *between popups of a live AE session* (no rebuild, no AE restart) when a
// config-specific window failure needs isolating.
struct PopupKnobs {
  bool hidden = true;     // create hidden, apply ex-styles, then show
  bool toolwindow = true; // WS_EX_TOOLWINDOW (no taskbar/Alt-Tab entry)
  bool topmost = true;    // FLAG_WINDOW_TOPMOST
  bool transparentfb = true; // FLAG_WINDOW_TRANSPARENT (GLFW transparent
                             // framebuffer + DWM blur-behind)
};

PopupKnobs LoadPopupKnobs() {
  PopupKnobs k;
  const char *tmp = getenv("TEMP");
  if (!tmp) return k;
  std::string path = std::string(tmp) + "\\barbar-popup-knobs.txt";
  FILE *f = fopen(path.c_str(), "r");
  if (!f) return k;
  char line[128];
  while (fgets(line, sizeof(line), f)) {
    char key[64];
    int val;
    if (sscanf(line, " %63[a-z] = %d", key, &val) == 2) {
      if (strcmp(key, "hidden") == 0) k.hidden = val != 0;
      if (strcmp(key, "toolwindow") == 0) k.toolwindow = val != 0;
      if (strcmp(key, "topmost") == 0) k.topmost = val != 0;
      if (strcmp(key, "transparentfb") == 0) k.transparentfb = val != 0;
    }
  }
  fclose(f);
  return k;
}

// The overlay window is just the panel: winX/winY/winW/winH is the panel's
// own screen rect, not a monitor. It used to cover the whole monitor so the
// cursor could never leave it (a small window stops getting mouse-move
// events at its edge, no capture) - but a monitor-sized transparent window
// hit an unfixable wall on hybrid-GPU laptops: DWM drops the alpha channel
// of GL framebuffers rendered by the discrete GPU (AfterFX.exe is bound to
// the NVIDIA GPU), so the "transparent" fullscreen window rendered as a
// fullscreen *black* window on the iGPU-driven built-in display - under
// every geometry and window style (1px overscan/undershoot, WS_EX_LAYERED
// at 255 and 254, LWA_COLORKEY, DwmExtendFrameIntoClientArea - all tried,
// all opaque; the same window on the Intel GPU composites perfectly).
// Mouse input therefore no longer comes from the window at all: the mouse
// guard + OverlayCursorPos/Overlay*Down (EscGuard.cpp) read it globally,
// and the window's only job is showing the panel. The alpha bug still
// paints the panel window's own transparent pixels black on that display -
// but those are now just the sub-panel-sized corner notches outside the
// rounding, near-invisible against the dark panel border, while iGPU-driven
// displays keep true rounded corners.
//
// One window per popup: created here, fully torn down by CloseOverlayWindow
// when the popup dismisses, so every invocation starts raylib from the same
// blank state. This used to be a create-once/hide-show singleton, adopted
// when a recreated window came up opaque - the actual culprit was a 1x1
// window resized after creation (GLFW wires the transparent framebuffer/DWM
// surface up at creation only), not recreation itself. The singleton then
// needed hacks of its own (WindowShouldClose() latching true forever after
// one Esc, DWM transparency not reapplied on re-show, Windows'
// foreground-lock denying focus to a re-shown-but-not-fresh window) - all
// moot now that the window never outlives a single popup. Shared by every
// popup/toast variant below.
void OpenOverlayWindow(int winX, int winY, int winW, int winH) {
  PopupDebugLogReset();
  SetTraceLogCallback(RaylibLogToDebug);
  DisableOverlayThreadThrottling();
  PopupKnobs knobs = LoadPopupKnobs();
  unsigned int flags = FLAG_WINDOW_UNDECORATED;
  if (knobs.transparentfb) flags |= FLAG_WINDOW_TRANSPARENT;
  if (knobs.topmost) flags |= FLAG_WINDOW_TOPMOST;
  if (knobs.hidden) flags |= FLAG_WINDOW_HIDDEN;
  SetConfigFlags(flags);
  // Create at final size - GLFW wires up the transparent framebuffer/DWM
  // surface at creation, and resizing afterward (the old 1x1 -> SetWindowSize)
  // doesn't re-establish it, so the window came up opaque.
  InitWindow(winW, winH, "popup");
  SetWindowPosition(winX, winY);
  // SetConfigFlags ORs into raylib's persistent flag state, so a knob turned
  // off after a previous popup ran with it on must be cleared explicitly.
  if (!knobs.topmost) ClearWindowState(FLAG_WINDOW_TOPMOST);
  // The window rect and live knobs - the first things to compare between an
  // AE run and a PopupExe run when the window misbehaves.
  TraceLog(LOG_INFO, "OVERLAY: window (%d,%d %dx%d)", winX, winY, winW, winH);
  TraceLog(LOG_INFO,
           "OVERLAY: knobs hidden=%d toolwindow=%d topmost=%d transparentfb=%d",
           knobs.hidden, knobs.toolwindow, knobs.topmost, knobs.transparentfb);
  // Created hidden (FLAG_WINDOW_HIDDEN above) so the taskbar-hiding ex-style
  // change happens before the window is ever shown - an ex-style change
  // doesn't reach an already-shown window's taskbar button without a
  // hide/show cycle.
  if (knobs.toolwindow) ApplyPopupWindowStyle(GetWindowHandle());
  if (knobs.hidden) ClearWindowState(FLAG_WINDOW_HIDDEN);
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
  PopupSessionBegin(); // re-read the polling setting for this drag

  const float kPanelW = 260, kPanelH = 40, kPadding = 10, kGap = 10,
              kButtonSize = 32, kIconSize = 16, kNumberFontSize = 14;

  // The window is the panel itself: centered on the cursor, nudged fully
  // onto the monitor the cursor is on (the mon rect's only remaining job).
  int winX = ClampInt(mouseX - (int)kPanelW / 2, monX, monX + monW - (int)kPanelW);
  int winY = ClampInt(mouseY - (int)kPanelH / 2, monY, monY + monH - (int)kPanelH);
  OpenOverlayWindow(winX, winY, (int)kPanelW, (int)kPanelH);
  // From here until the guards end: Esc is eaten system-wide so AE can't
  // see it and abort a script we're driving (EscGuard.cpp), and both mouse
  // buttons are read globally and eaten too - the commit/cancel click must
  // never reach AE (the old fullscreen window swallowed it by covering the
  // screen; the panel-sized window can't). The loop reads all input from
  // the guards, not from raylib.
  BeginEscapeGuard();
  BeginMouseGuard();

  Rectangle panel = {0, 0, kPanelW, kPanelH};
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
  // Unreachable sentinel pair - forces the first poll tick to send.
  float lastSentValue = -1.0f;
  std::string lastSentMode;
  float sendTimer = 0.0f;
  bool clicked = false;
  bool rightClickCancel = false;
  bool holdOutgoing = false;
  const char *mode = "both";

  // Ctrl = precision drag: freeze the value/mouseX pair at the moment Ctrl
  // goes down, then move at 1/kPrecisionSlowdown speed relative to that
  // anchor instead of tracking the cursor directly. On release, direct
  // tracking resumes through a persistent X offset chosen so the slider
  // holds the value it had at release instead of jumping to the raw cursor
  // position.
  bool precisionWasDown = false;
  float precisionAnchorValue = 0.0f, precisionAnchorMouseX = 0.0f;
  float trackOffsetX = 0.0f;

  // Dismissal is explicit (left-click commits, right-click cancels, or Esc
  // cancels via the escape guard - raylib never sees the key while the
  // guard's hook is active; WindowShouldClose only fires Esc-wise if the
  // hook failed to install) - never focus-based:
  // IsWindowFocused() can go false through no action of the user's (Windows'
  // foreground-lock policy), which once had the popup dismissing itself ~10
  // frames in.
  while (!WindowShouldClose() && !EscapeGuardTriggered()) {
    BeginDrawing();
    ClearBackground(BLANK);
    DrawPanelBox(panel, kBg, kBorder);

    // Read every frame, not just at commit, so the held modifier always
    // matches what's drawn. Async key state (Overlay*Down), not raylib's
    // key events: those need the popup window focused, and Windows'
    // foreground-lock can silently deny it focus (see the dismissal
    // comment above).
    bool altHeld = OverlayAltDown();
    bool shiftHeld = OverlayShiftDown();
    mode = altHeld ? "in" : (shiftHeld ? "out" : "both");

    bool precisionDown = OverlayCtrlDown();
    Vector2 mouseLocal = GuardedCursorLocal(winX, winY);
    float mouseX = mouseLocal.x;
    if (precisionDown && !precisionWasDown) {
      precisionAnchorValue = sliderValue;
      precisionAnchorMouseX = mouseX;
    }
    if (!precisionDown && precisionWasDown) {
      // Shift the direct mapping so it reads back exactly the value the
      // precision drag released at, given the cursor's current X.
      trackOffsetX = sliderValue / 100.0f * sliderBounds.width +
                     sliderBounds.x - mouseX;
    }
    precisionWasDown = precisionDown;

    const float kPrecisionSlowdown = 30.0f;
    float t = precisionDown
                  ? precisionAnchorValue / 100.0f +
                        (mouseX - precisionAnchorMouseX) / kPrecisionSlowdown /
                            sliderBounds.width
                  : (mouseX + trackOffsetX - sliderBounds.x) /
                        sliderBounds.width;
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
    // doesn't flood the socket. An alt/shift flip counts as a change on its
    // own: the value sits still while the mode goes in/out/both, and gating
    // on the value alone left the preview stuck on the old mode until the
    // mouse moved.
    sendTimer += GetFrameTime();
    if (sendTimer >= 0.25f) {
      sendTimer = 0.0f;
      if (sliderValue != lastSentValue || lastSentMode != mode) {
        SendSliderUpdate(sliderValue, mode, true);
        lastSentValue = sliderValue;
        lastSentMode = mode;
      }
    }

    // DrawButton fires on mouse-release-while-hovering, not on press - so
    // a press that lands on the button must be excluded from the generic
    // "any click dismisses" check below, or the popup closes on the press
    // frame before the button ever gets its own release-frame click.
    bool overButton = CheckCollisionPointRec(mouseLocal, holdButtonBounds);
    bool holdButtonClicked =
        DrawButton(holdButtonBounds, overButton, GuardedLeftReleased());
    bool dismissClick = GuardedLeftPressed() && !overButton;
    // Right-click = cancel, same outcome as Esc but with none of Esc's
    // baggage (AE doesn't abort scripts on mouse buttons) - one hand stays
    // on the mouse for the whole drag-and-bail gesture.
    bool cancelClick = GuardedRightPressed();
    EndDrawing();

    if (holdButtonClicked) {
      holdOutgoing = true;
      break;
    }
    if (cancelClick) {
      rightClickCancel = true;
      break;
    }
    if (dismissClick) {
      clicked = true;
      break;
    }
  }

  // The panel's job is done the moment the loop exits - hide the window
  // before the (possibly up-to-1s) release waits below, which used to be
  // invisible for free when the window was fullscreen-transparent and drew
  // nothing. The waits keep pumping events so the guards' hooks keep firing.
  SetWindowState(FLAG_WINDOW_HIDDEN);

  // Hook-failed fallback only: with the mouse guard up, the right-click was
  // eaten at the low-level hook and AE can never see the button-up. Without
  // it, don't tear the window down while the button is still held: once the
  // window dies the button-up lands in AE's queue, and DefWindowProc turns
  // an orphan WM_RBUTTONUP into WM_CONTEXTMENU - AE would pop a context
  // menu at the cursor. Bounded like the Esc wait below.
  if (rightClickCancel && !MouseGuardInstalled()) {
    double holdStart = GetTime();
    while (IsMouseButtonDown(MOUSE_RIGHT_BUTTON) && GetTime() - holdStart < 1.0) {
      BeginDrawing();
      ClearBackground(BLANK);
      EndDrawing();
    }
  }

  // On an Esc exit, don't tear the window down while the key is still held:
  // the moment the window dies AE is the foreground window again and the
  // remaining auto-repeat keydowns/keyup would land in *its* queue - the
  // guard's hook keeps eating them only while this thread keeps pumping, so
  // idle here (drawing nothing; the window is transparent) until the key is
  // physically up. Bounded so a genuinely held Esc can't wedge the overlay
  // thread.
  if (!holdOutgoing && !clicked && !rightClickCancel) {
    double holdStart = GetTime();
    while (EscapeGuardKeyStillDown() && GetTime() - holdStart < 1.0) {
      BeginDrawing();
      ClearBackground(BLANK);
      EndDrawing();
    }
  }

  CloseOverlayWindow();
  EndMouseGuard();
  EndEscapeGuard();

  if (holdOutgoing) {
    PopupHoldOutgoing();
  } else if (clicked) {
    SendSliderUpdate(sliderValue, mode, false);
  } else if (rightClickCancel) {
    // Same restore as the Esc path, minus the Esc precautions - AE never
    // aborts scripts over a mouse button.
    PopupCancelEase();
  } else {
    // Loop exits without holdOutgoing/clicked via Esc - put the keyframes
    // back the way the drag found them (the jsx side kept the pre-drag
    // snapshot from the preview ticks). The guard has hidden the Esc press
    // from AE entirely, so cancelEase is safe to drive; the release wait
    // below is the hook-failed fallback (AE saw the key then, and aborts any
    // script that runs while it's down - "Unable to execute script at line
    // N. Execution halted").
    WaitForEscapeReleased();
    PopupCancelEase();
  }
}

void RunNoSelectionToast(int mouseX, int mouseY, int monX, int monY, int monW,
                         int monH) {
  const float kPanelW = 240, kPanelH = 40;
  int winX = ClampInt(mouseX - (int)kPanelW / 2, monX, monX + monW - (int)kPanelW);
  int winY = ClampInt(mouseY - (int)kPanelH / 2, monY, monY + monH - (int)kPanelH);
  OpenOverlayWindow(winX, winY, (int)kPanelW, (int)kPanelH);
  // Same guard as the popup: the dismiss click shouldn't leak into AE.
  BeginMouseGuard();

  Rectangle panel = {0, 0, kPanelW, kPanelH};

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

    if (GuardedLeftPressed() || WindowShouldClose())
      break; // explicit dismiss still works
    elapsed += GetFrameTime();
  }

  CloseOverlayWindow();
  EndMouseGuard();
}

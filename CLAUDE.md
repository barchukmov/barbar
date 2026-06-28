# barbar

A CEP panel (Svelte/TS, via bolt-cep) + an AEGP plugin (C++, `AegpDemo.aex`) for After Effects, bridged by a WebSocket.

## Architecture

- **AEGP side** (`AEGP/`): registers a global `Ctrl+H` hotkey ([HotkeyOverlay_Win32.cpp](AEGP/HotkeyOverlay_Win32.cpp)), but only fires if AE is the foreground process (PID check against `GetCurrentProcessId()` - cheap, since the plugin runs inside `AfterFX.exe` itself, no exe-path lookup needed). On trigger, shows a raylib/raygui popup ([HotkeyOverlay_Popup.cpp](AEGP/HotkeyOverlay_Popup.cpp)):
  - Fullscreen transparent window, so the mouse is never "outside" it (a small window stops getting mouse-move events the moment the cursor leaves its client area, with no capture).
  - A rounded slider panel that tracks the cursor continuously with no click/drag needed - via `forceDragging`, a parameter we patched into raygui's `GuiSliderPro` (raygui's own dragging is gated on the mouse button being held; there's no native "always follow" flag).
  - Any click anywhere sends the slider value over the websocket and dismisses the popup.
  - The window is created once and hidden/shown thereafter, never destroyed/recreated - see "Known gotchas" below for why.
- **Transport**: AEGP is the WS *client* (IXWebSocket, TLS+zlib disabled - loopback-only, no need for either). CEP is the *server* (`ws` npm package, port 41420) - CEP's panel lifetime is the more stable end of the two.
- **CEP side** (`src/js/`): three panels -
  - `main` - the visible UI.
  - `floating` - a draggable widget that closes itself on blur (`csi.closeExtension()` on window blur) - not reliable for anything that needs to persist.
  - `background` - 1x1, invisible, `autoVisible: true`, `type: "Custom"`. Exists purely to keep `initBolt()` (which starts the WS server, see [ws-server.ts](src/js/lib/ws-server.ts)) running from AE launch onward, independent of whether the user ever opens a panel.
  - On receiving `{"type":"slider","value":N}`, CEP fires an AE-side `alert(N)` via `evalES` (see `onAegpMessage` wiring in [bolt.ts](src/js/lib/utils/bolt.ts)).

## Build pipeline

- `pnpm build` = `tsc` + `vite build` (CEP bundle; `ws` gets copied into `dist/cep/node_modules` via `installModules` in [cep.config.ts](cep.config.ts), since it's a real npm dependency the CEP node runtime needs at runtime, not something Vite can bundle for a browser target) -> `cd AEGP && build.ps1`.
- [build.ps1](AEGP/build.ps1) does, in order:
  1. CMake-vendors raylib, raygui, and IXWebSocket via `FetchContent` (see [HotkeyOverlay/CMakeLists.txt](AEGP/HotkeyOverlay/CMakeLists.txt)).
  2. **Patches the freshly-fetched `raygui.h`** before copying it into `Vendor/` - line-search based (not a full diff), adding the `forceDragging` param to `GuiSliderPro` specifically, without touching the three *other* controls in that file that share verbatim-identical dragging code (scrollbar/spinner/listview). If raygui's pinned tag (`4.0`) ever changes, this patch step will throw with a clear "upstream layout changed" error rather than silently corrupting the file.
  3. Copies headers/libs into `AEGP/Vendor/raylib/` and `AEGP/Vendor/IXWebSocket/` (both are build *output*, regenerated every build - despite being git-tracked from the initial commit, don't hand-edit anything under `Vendor/`).
  4. MSBuild builds `AegpDemo.vcxproj` straight into AE's plugin folder (`C:\Program Files\Adobe\Adobe After Effects 2026\Support Files\Plug-ins\AEGP\`).
- `pnpm run build:aegp` runs just the AEGP step.
- `pnpm run reload` ([reload-ae.ps1](reload-ae.ps1)): kills `AfterFX.exe` -> `pnpm build` -> relaunches AE with the last-opened project. The "last opened project" lookup is a best-effort single-line regex match against AE's `Prefs.txt` MRU entry - it only works when that path is plain ASCII on one line. AE's prefs format wraps long/non-ASCII paths across multiple lines using an alternating literal-text / hex-encoded-UTF16BE quoted-segment encoding, which is not decoded here (attempted once, decoded garbage) - in that case AE just launches with no file.

**Standing rule: never run `pnpm reload`, `taskkill`, or `Stop-Process` against `AfterFX.exe` automatically. It runs elevated on this machine, so a non-elevated process gets Access Denied trying to kill it - this is a hard Windows privilege boundary, not something to route around. Tell the user a reload is needed and let them run it.**

## Known gotchas (already fixed, kept here so they don't get reintroduced)

- **MSBuild `OutDir`/`IntDir` trailing backslash**: a single trailing `\` right before the closing `"` in a quoted MSBuild arg escapes the quote instead of closing it, silently merging it with the next argument. `build.ps1` doubles any trailing backslash before passing `OutDir`/`IntDir` to MSBuild.
- **Stale CMake cache after moving the repo folder**: `HotkeyOverlay/build/CMakeCache.txt` hardcodes the absolute source path. If the repo directory ever gets moved/renamed, delete `AEGP/HotkeyOverlay/build/` before rebuilding, or CMake will refuse to reconfigure.
- **IXWebSocket needs `USE_ZLIB OFF` too**, not just `USE_TLS OFF` - zlib is for permessage-deflate compression, independent of TLS, and pulls in a `find_package(ZLIB REQUIRED)` that fails on a machine without zlib installed. Both are forced off in `HotkeyOverlay/CMakeLists.txt` since this is a loopback-only connection with no need for either.
- **raylib popup window must be created once, then hidden/shown** - not `InitWindow`/`CloseWindow` per popup. raylib's GLFW backend doesn't reliably reapply some window flags (transparency was the visible symptom) when a window is destroyed and recreated within the same process.
- **Transparency still breaks after a hide/show cycle even with the above fix**: GLFW only calls `updateFramebufferTransparency()` (which calls `DwmEnableBlurBehindWindow`) at window creation or in response to a `WM_DWMCOMPOSITIONCHANGED`/`WM_DWMCOLORIZATIONCOLORCHANGED` message - never on `ShowWindow`. Fixed by having `ForceForeground()` (in [HotkeyOverlay_Win32.cpp](AEGP/HotkeyOverlay_Win32.cpp)) send `WM_DWMCOMPOSITIONCHANGED` to the window every time it's shown, nudging GLFW's own WndProc to redo it.
- **Don't gate popup dismissal on `IsWindowFocused()`**: a background thread re-showing an *already-existing* (previously hidden) window can get silently denied by Windows' foreground-lock policy - unlike a freshly created window, which Windows grants foreground rights to more readily. This made the popup hide itself ~10 frames in (~166ms) regardless of user action. Dismissal is now purely explicit: click anywhere, or Esc (`WindowShouldClose()`).

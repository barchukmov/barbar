# barbar

A CEP panel (Svelte/TS, via bolt-cep) + an AEGP plugin (C++, `AegpDemo.aex`) for After Effects, bridged by a WebSocket.

## Architecture

- **AEGP side** (`AEGP/`): registers global hotkeys from the user's table ([HotkeyOverlay_Win32.cpp](AEGP/HotkeyOverlay_Win32.cpp)); they only fire if AE is the foreground process (PID check against `GetCurrentProcessId()` - cheap, since the plugin runs inside `AfterFX.exe` itself, no exe-path lookup needed). On trigger, shows a raylib popup ([HotkeyOverlay_Popup.cpp](AEGP/HotkeyOverlay_Popup.cpp)), rendered in-process on the hotkey thread (not a separate exe - `PopupExe/` is only a standalone dev runner for iterating on the UI without reloading AE):
  - Fullscreen transparent window, so the mouse is never "outside" it (a small window stops getting mouse-move events the moment the cursor leaves its client area, with no capture).
  - A rounded slider panel that tracks the cursor continuously with no click/drag needed - drawn directly with raylib primitives (raygui is no longer used; an earlier version patched a `forceDragging` param into raygui's `GuiSliderPro`).
  - Any click anywhere sends the slider value over the websocket and dismisses the popup.
  - The window is created fresh for every popup and fully torn down (`CloseWindow`) on dismiss, so raylib starts from a clean slate each call - see "Known gotchas" for the creation-time details that make this work.
- **Transport**: AEGP is the WS *client* (IXWebSocket, TLS+zlib disabled - loopback-only, no need for either). CEP is the *server* (`ws` npm package, port 41420) - CEP's panel lifetime is the more stable end of the two.
- **CEP side** (`src/js/`): three panels -
  - `main` - the visible UI.
  - `floating` - a draggable widget that closes itself on blur (`csi.closeExtension()` on window blur) - not reliable for anything that needs to persist.
  - `background` - 1x1, invisible, `type: "Custom"`. Exists purely to keep `initBolt()` (which starts the WS server, see [ws-server.ts](src/js/lib/ws-server.ts)) running from AE launch onward, independent of whether the user ever opens a panel. CEP never auto-starts Custom-type extensions on its own, so it needs `startOnEvents` (`applicationActivate`) in the manifest to actually load at launch. Panels also `requestOpenExtension` it from `initBolt()` as a fallback.
  - The WS server port (41420) is raced by every panel context (all load the same bundle); losers get `EADDRINUSE` and retry every 3s so a surviving context adopts the port if the owner dies.
- **Hotkey table**: `%APPDATA%/barbar-hotkeys.json` is the single source of truth and the CEP<->AEGP contract (`[{id,vkey,mods,fn},...]`, mods bitmask bit0=ctrl/bit1=shift/bit2=alt). CEP is its only writer; the AEGP only reads it - once at overlay-thread startup (hotkeys work from AE launch with no CEP running) and again on each `hotkeysChanged` ping over the socket. The table itself never crosses the wire. Edits broadcast a `barbar.hotkeysChanged` CSEvent (`notifyHotkeysChanged()`) because the editing panel (`main`) is usually not the context that owns the AEGP socket (`background`) - a direct ping from `main` would silently go nowhere.
  - On receiving popup messages (`polling`/`accept`/`cancel`/`holdOutgoing`), CEP calls typed ExtendScript functions (`applyEase`/`cancelEase`/... in [aeft.ts](src/jsx/aeft/aeft.ts)) via `evalTS` - see `onAegpMessage` wiring in [bolt.ts](src/js/lib/utils/bolt.ts). The ease logic lives in the jsx module; drag-session state (pre-drag keyframe snapshots for in/out-only easing and Esc-cancel restore) is module state there, persisting across `evalTS` calls in AE's shared ExtendScript engine.

## Where things live (file map)

Everything below is the code you actually own; Adobe SDK dirs (`AEGP/Headers/`, `AEGP/Util/`, parts of `Resources/`) and `AEGP/Vendor/{raylib,IXWebSocket}/` are external and gitignored.

**AEGP plugin (C++, `AegpDemo.aex`) — `AEGP/`**
- [AegpDemo.cpp](AEGP/AegpDemo.cpp) - plugin entry (`EntryPointFunc`) and lifetime, stripped to a headless shell. The `BarbarPlugin` ctor registers *only* a death hook and, on Win, calls `StartHotkeyOverlay()` + `StartWsClient("ws://127.0.0.1:41420", ...)`; `DeathHook` stops both. There is deliberately no AE-side UI (no menu command, no panel) - the CEP extension is the only UI, so the Adobe demo-panel sample this was built from (`AegpDemoUI*`, `AegpDemo_Strings.cpp`, the whole `Mac/` tree) has been removed. The binary is still named `AegpDemo.aex` (PiPL/`build.ps1`/plugin-folder path all key off that name); only the sample *code* is gone.
- [HotkeyOverlay_Win32.cpp](AEGP/HotkeyOverlay_Win32.cpp) - the hotkey engine and its dedicated thread (details below).
- [HotkeyOverlay_Popup.cpp](AEGP/HotkeyOverlay_Popup.cpp) - the raylib popup + no-selection toast, all drawing and the in-popup interaction loop.
- [WsClient.cpp](AEGP/WsClient.cpp) - IXWebSocket wrapper: `WsSend` (fire-and-forget), `WsRequest` (blocking request/reply), `WsJsonGetString` (hand-rolled flat-JSON reader shared with the Win32 file).
- [win32_popup_bridge.h](AEGP/win32_popup_bridge.h) / [HotkeyOverlay.h](AEGP/HotkeyOverlay.h) - the two headers wiring the Win32 side to the raylib side and to `AegpDemo.cpp`.
- [FontPath.cpp](AEGP/FontPath.cpp) - resolves the bundled font + mono font by module-relative path (`GetFontPath`/`GetMonoFontPath`/`GetIconPath`).
- `Resources/Icons/*.svg`, `Resources/Fonts/*.otf` - popup assets, rasterized/loaded at popup-open time.
- [PopupExe/](AEGP/PopupExe) - standalone dev runner (`popup_main.cpp` + `StubWs.cpp` stubbing the socket) to iterate the popup outside AE; its `recreate` mode exercises the per-popup window create/destroy path.

**CEP panel (Svelte/TS) — `src/js/`**
- [ws-server.ts](src/js/lib/ws-server.ts) - the WS *server*, the hotkey-file read/write (`barbar-hotkeys.json`), the polling setting (`barbar-settings.json`), and the `notifyHotkeysChanged` cross-context broadcast.
- [bolt.ts](src/js/lib/utils/bolt.ts) - `initBolt()` boots everything (loads jsx, starts the server, opens `background`) and `onAegpMessage` routes incoming popup messages to `evalTS` calls.
- `main/main.svelte`, `floating/floating.svelte` - the two visible panels; the `background` panel loads the same bundle with no UI.

**ExtendScript (runs inside AE) — `src/jsx/`**
- [aeft.ts](src/jsx/aeft/aeft.ts) - the ease implementation (`applyEase`/`cancelEase`/`setOutgoingHandleHold`/`isAnyKeyframeSelected`) and its cross-`evalTS` drag-session state.

## Component internals

### AEGP threading model
Three threads matter. (1) **AE's main thread** runs the plugin ctor/hooks. (2) **The overlay thread** (`ThreadProc` in [HotkeyOverlay_Win32.cpp](AEGP/HotkeyOverlay_Win32.cpp)) owns the hotkey message queue and, while a popup is up, runs the raylib loop inline. `RegisterHotKey`/`UnregisterHotKey` and `WM_HOTKEY` are all thread-affine, so everything hotkey-related is funneled here; other threads hand it work by `PostThreadMessage` (`kHotkeysUpdatedMsg = WM_APP+1` for a new table). `StartHotkeyOverlay` blocks (≤2s) on a `g_queueReady` event so a table push racing in at startup can't be posted to a queue-less thread and dropped. (3) **IXWebSocket's own thread** delivers WS messages (`WsClient`).

Because `RegisterHotKey` grabs a combo *system-wide* the instant it's registered, the table is only registered while AE is foreground: a 250ms `SetTimer` drives `ForegroundPollTick`, which registers/unregisters as focus crosses in/out (`IsAeForeground` = foreground window's PID == our PID; cheap because we *are* `AfterFX.exe`). Hotkeys are live from AE launch because `ThreadProc` seeds the table straight from `barbar-hotkeys.json` (`LoadHotkeyPayloadFromFile`) rather than waiting for CEP. On `WM_HOTKEY` it finds the monitor under the cursor (`MonitorFromPoint`, not the primary) and calls `RunNativeFunction`: `"Easing"` is handled in-process (see below); any other `fn` is forwarded to CEP as `{"type":"hotkeyTriggered","fn":...}` for CEP to handle.

### Popup interaction grammar ([HotkeyOverlay_Popup.cpp](AEGP/HotkeyOverlay_Popup.cpp))
`RunNativeFunction("Easing")` first asks CEP whether anything is selected via a **blocking** `WsRequest({"type":"keyframeSelectionQuery"}, "keyframeSelectionReply", 300ms)`; a timeout/empty reply is treated as "selected" (err toward showing the popup). Selected → `RunPopupAtCursor`; not → `RunNoSelectionToast` (a 1s fading "No keyframes are selected", dismissible early).

Inside `RunPopupAtCursor`'s loop, all state is read every frame so the drawing and the outgoing messages always agree:
- **Slider value** = cursor-X mapped into the track, clamped 0–100.
- **Mode** from held modifiers: Ctrl → `"in"`, Shift → `"out"`, neither → `"both"`. A mode flip counts as a change even when the value is still.
- **Alt = precision drag**: on Alt-down it anchors (value, mouseX); while held it moves at 1/30 speed relative to that anchor; releasing snaps back to direct tracking with no re-anchor.
- **Icon**: value ≤ 0 draws the Linear glyph (no ease yet); otherwise the in/out/both easy-ease glyph for the current mode. Each state is a pre-colored SVG, not a runtime tint.
- **Hold-Out button** (right side, `Block.svg`): fires on release-while-hovered and is excluded from the generic "any click dismisses" check so its own press doesn't dismiss first.

Outgoing messages: a **live preview** is polled every 0.25s (`{"type":"polling",value,mode}`) only when value or mode changed, so a fast drag can't flood the socket. Exit paths: dismiss-click → `{"type":"accept",value,mode}`; Hold-Out → `{"type":"holdOutgoing"}`; Esc (raylib's default exit key via `WindowShouldClose`) → `{"type":"cancel"}`. Dismissal is never focus-gated (see Known gotchas).

### WS message protocol (all flat JSON, keyed by `type`)
| Direction | `type` | Payload | Meaning |
|---|---|---|---|
| AEGP→CEP | `hello` | — | on WS open |
| AEGP→CEP | `keyframeSelectionQuery` | — | blocking; expects `keyframeSelectionReply` |
| AEGP→CEP | `polling` | `value`, `mode` | live in-drag preview (respected only if polling enabled) |
| AEGP→CEP | `accept` | `value`, `mode` | commit on dismiss-click |
| AEGP→CEP | `cancel` | — | Esc: restore pre-drag keyframes |
| AEGP→CEP | `holdOutgoing` | — | Hold-Out button |
| AEGP→CEP | `hotkeyTriggered` | `fn` | non-native hotkey, for CEP to handle |
| CEP→AEGP | `ack` | — | reply to `hello` |
| CEP→AEGP | `hotkeysChanged` | — | re-read the hotkey file |
| CEP→AEGP | `keyframeSelectionReply` | `selected` | answer to the query |

`polling`/`accept` both carry the same value/mode and both drive `applyEase` in CEP; the only difference is the `isPreview` flag ([bolt.ts](src/js/lib/utils/bolt.ts) `onAegpMessage`). Polling can be disabled panel-side (`loadPollingEnabled`), which drops preview ticks but keeps the commit.

### The ease algorithm ([aeft.ts](src/jsx/aeft/aeft.ts))
`applyEase(value, mode, isPreview)` walks every selected property carrying selected keyframes (snapshotted into a plain array first, because `setInterpolationTypeAtKey` can deselect keys mid-walk). Per key:
- **Pre-drag snapshot** (`snapshotKeyOnce`, keyed by `layer|propName|propIndex|keyTime`) captures in/out interp types, temporal eases, and auto-bezier/continuous flags the *first* time a key is touched in a drag. Later polling ticks read from this snapshot, not from the previous tick's own output. `cancelEase` restores it; a commit clears the session so the next drag snapshots fresh. This state is module-level and survives across `evalTS` calls (shared ExtendScript engine).
- **Interior-run detection** (`interiorRunFlags`): a key that a monotonic run passes straight through gets temporal auto-bezier (AE derives its tangents from neighbors — the smooth case) instead of the slider's ease, which belongs on the run's *ends*. Spatial props (motion-path-capable) use the dot product of incoming vs. outgoing motion (>0 = interior); everything else is per-dimension sign agreement with at least one dimension moving.
- **Mode** decides which side(s) of the ease the slider drives: `in` (Ctrl), `out` (Shift), or `both`; the untouched side is read back from the snapshot so in/out-only easing doesn't clobber it.

`isAnyKeyframeSelected` backs the selection query; `setOutgoingHandleHold` is the Hold-Out action.

## Build pipeline

- `pnpm build` = `tsc` + `vite build` (CEP bundle; `ws` gets copied into `dist/cep/node_modules` via `installModules` in [cep.config.ts](cep.config.ts), since it's a real npm dependency the CEP node runtime needs at runtime, not something Vite can bundle for a browser target) -> `cd AEGP && build.ps1`.
- [build.ps1](AEGP/build.ps1) does, in order:
  1. CMake-vendors raylib and IXWebSocket via `FetchContent` (see [HotkeyOverlay/CMakeLists.txt](AEGP/HotkeyOverlay/CMakeLists.txt)).
  2. Copies headers/libs into `AEGP/Vendor/raylib/` and `AEGP/Vendor/IXWebSocket/` (both are build *output*, gitignored and regenerated every build - don't hand-edit them; `AEGP/Vendor/nanosvg/` is the exception: hand-vendored source, tracked).
  3. MSBuild builds `AegpDemo.vcxproj` straight into AE's plugin folder (`C:\Program Files\Adobe\Adobe After Effects 2026\Support Files\Plug-ins\AEGP\`).
- **Adobe SDK is not in the repo** (its EULA forbids redistribution): `AEGP/Headers/`, `AEGP/Util/`, and three files in `AEGP/Resources/` (`AE_General.r`, `Mach-O_prefix.h`, `PiPLtool.exe`) are gitignored and must be copied in from Adobe's free AE SDK download - see README "Adobe SDK". They exist in the local working tree, so local builds just work.
- `pnpm run build:aegp` runs just the AEGP step.
- `pnpm run reload` ([reload-ae.ps1](reload-ae.ps1)): kills `AfterFX.exe` -> `pnpm build` -> relaunches AE with the last-opened project. The "last opened project" lookup is a best-effort single-line regex match against AE's `Prefs.txt` MRU entry - it only works when that path is plain ASCII on one line. AE's prefs format wraps long/non-ASCII paths across multiple lines using an alternating literal-text / hex-encoded-UTF16BE quoted-segment encoding, which is not decoded here (attempted once, decoded garbage) - in that case AE just launches with no file.

**Standing rule: never run `pnpm reload`, `taskkill`, or `Stop-Process` against `AfterFX.exe` automatically. It runs elevated on this machine, so a non-elevated process gets Access Denied trying to kill it - this is a hard Windows privilege boundary, not something to route around. Tell the user a reload is needed and let them run it.**

## Known gotchas (already fixed, kept here so they don't get reintroduced)

- **MSBuild `OutDir`/`IntDir` trailing backslash**: a single trailing `\` right before the closing `"` in a quoted MSBuild arg escapes the quote instead of closing it, silently merging it with the next argument. `build.ps1` doubles any trailing backslash before passing `OutDir`/`IntDir` to MSBuild.
- **Stale CMake cache after moving the repo folder**: `HotkeyOverlay/build/CMakeCache.txt` hardcodes the absolute source path. If the repo directory ever gets moved/renamed, delete `AEGP/HotkeyOverlay/build/` before rebuilding, or CMake will refuse to reconfigure.
- **IXWebSocket needs `USE_ZLIB OFF` too**, not just `USE_TLS OFF` - zlib is for permessage-deflate compression, independent of TLS, and pulls in a `find_package(ZLIB REQUIRED)` that fails on a machine without zlib installed. Both are forced off in `HotkeyOverlay/CMakeLists.txt` since this is a loopback-only connection with no need for either.
- **The transparent popup window must be created at its final size, overscanned 1px past the monitor**: GLFW wires up the transparent framebuffer/DWM surface only at window creation (a 1x1 window resized afterward comes up opaque), and a borderless window whose client area exactly matches the monitor triggers Windows' fullscreen optimization, which drops DWM compositing and also renders it opaque. These two were originally misdiagnosed as "destroying/recreating the window breaks transparency", which led to a create-once/hide-show singleton that then needed workarounds of its own (`WindowShouldClose()`'s Esc-latch, `WM_DWMCOMPOSITIONCHANGED` nudges on re-show, foreground-lock denial of re-shown windows). The window is now `InitWindow`/`CloseWindow` per popup - a clean raylib slate every call, with the two creation-time rules above applied each time - and none of those workarounds exist anymore. Don't reintroduce the singleton. (`PopupExe`'s `recreate` mode runs two full window cycles in one process to test this path outside AE.)
- **Don't gate popup dismissal on `IsWindowFocused()`**: Windows' foreground-lock policy can silently deny focus to a window shown from a background thread, which once made the popup hide itself ~10 frames in (~166ms) regardless of user action. Dismissal is purely explicit: click anywhere, or Esc (raylib's default exit key, via `WindowShouldClose()`).

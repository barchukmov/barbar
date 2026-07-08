# barbar

Keyframe easing for After Effects, at the cursor. Press a hotkey anywhere in
AE and a slider popup appears under your mouse — move the mouse to ease the
selected keyframes (with live preview in the comp), click to commit, Esc to
put everything back exactly as it was.

- **No clicking, no dragging** — the slider tracks the cursor the moment the
  popup opens.
- **Ctrl** = ease in only · **Shift** = ease out only · **Alt** = precision
  (1/30 speed) drag.
- **Smart interiors**: keys inside a monotonic run get temporal auto-bezier
  instead of the ease — for spatial properties (Position, point controls) a
  key counts as "passing through" when its incoming and outgoing motion agree
  within 90° (positive dot product); for everything else, when no dimension
  reverses direction.
- **Hold button** sets just the outgoing handle to Hold.
- Works on any monitor, respects which app is foreground (hotkeys only fire
  while AE is), and the hotkeys are user-configurable from the panel.

## How it's put together

Two halves, bridged by a loopback WebSocket (port 41420):

- **AEGP plugin** (`AEGP/`, C++): registers the global hotkeys and renders
  the popup — a fullscreen transparent [raylib](https://github.com/raysan5/raylib)
  window on the monitor under the cursor, created fresh per popup and fully
  torn down on dismiss. It is the WS *client* ([IXWebSocket](https://github.com/machinezone/IXWebSocket)).
- **CEP panel** (`src/`, Svelte + TypeScript via [bolt-cep](https://github.com/hyperbrew/bolt-cep)):
  hosts the WS *server*, the hotkey-editor UI, and applies the actual
  keyframe changes through typed ExtendScript functions
  ([src/jsx/aeft/aeft.ts](src/jsx/aeft/aeft.ts), called with `evalTS`). An
  invisible `background` panel auto-starts with AE so the server exists even
  when no panel is open.
- **Hotkey table**: `%APPDATA%/barbar-hotkeys.json` is the single source of
  truth. CEP writes it; the AEGP reads it directly at startup (hotkeys work
  from AE launch, no panel needed) and re-reads on a `hotkeysChanged` ping.

[CLAUDE.md](CLAUDE.md) has the deeper architecture notes and a list of
hard-won gotchas (DWM transparency, CEP lifecycle, MSBuild quoting) — worth
reading before touching the popup or the build.

## Building (Windows)

Prerequisites:

- After Effects 2024+ (the default install path in `AEGP/build.ps1` points at
  AE 2026 — override `-OutDir` for other versions)
- Visual Studio 2022 with the C++ desktop workload
- CMake 3.20+
- Node.js 18+ and [pnpm](https://pnpm.io)

### Adobe SDK (one-time setup)

Adobe's SDK license doesn't allow redistributing it, so this repo doesn't.
Download the free **After Effects SDK** from
[developer.adobe.com/after-effects](https://developer.adobe.com/after-effects/)
and copy out of its `Examples/` folder:

| From the SDK | Into this repo |
|---|---|
| `Examples/Headers/` (whole folder, incl. `SP/` and `Win/`) | `AEGP/Headers/` |
| `Examples/Util/` (whole folder) | `AEGP/Util/` |
| `Examples/Resources/AE_General.r`, `Mach-O_prefix.h`, `PiPLtool.exe` | `AEGP/Resources/` |

Those paths are `.gitignore`d — they live in your working tree only.

### Build

```sh
pnpm install
pnpm build          # CEP bundle + AEGP plugin (into AE's Plug-ins folder)
pnpm run build:aegp # just the C++ plugin
pnpm dev            # CEP panel dev server with HMR
```

The first AEGP build fetches and compiles raylib and IXWebSocket via CMake
`FetchContent` into `AEGP/Vendor/` (build output, not tracked).

The default `-OutDir` writes straight into
`C:\Program Files\Adobe\Adobe After Effects 2026\Support Files\Plug-ins\AEGP\`,
which needs an elevated shell; pass another `-OutDir` to build elsewhere.

### Iterating on the popup UI without AE

`AEGP/PopupExe/` builds the popup into a standalone exe (WebSocket calls
stubbed to stdout):

```sh
cd AEGP/PopupExe
cmake -S . -B build && cmake --build build --config Debug
build/Debug/raylib_popup.exe 500 400 1920 1080          # the slider popup
build/Debug/raylib_popup.exe 500 400 1920 1080 toast    # the no-selection toast
build/Debug/raylib_popup.exe 500 400 1920 1080 recreate # two window cycles in one process
```

## License

GPL-3.0-or-later — see [LICENSE](LICENSE). Third-party components (including
the MIT-licensed bolt-cep scaffold this project started from) are listed in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md). The Adobe After Effects SDK
is required to build but is licensed separately by Adobe and not included.

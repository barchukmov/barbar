
# Pre-release tasks

1. [DONE] **Raylib popup startup query: active comp + keyframe selection.** `keyframeSelectionQuery`/`keyframeSelectionReply` round trip already wired in [HotkeyOverlay_Win32.cpp](AEGP/HotkeyOverlay_Win32.cpp) and [bolt.ts](src/js/lib/utils/bolt.ts).

2. [DONE] **Easing popup behavior: modifier-gated in/out, no-selection toast.** `RunNoSelectionToast` in [HotkeyOverlay_Popup.cpp](AEGP/HotkeyOverlay_Popup.cpp) covers this.

3. [DONE] **ExtendScript: monotonic runs -> continuous bezier.** Implemented in [easingScript.ts](src/shared/easingScript.ts) - run membership cached on `$.__easeMemory` once per property per drag.

4. **CEP UI cleanup.** Strip the Panelator/AEGP boilerplate panel. Make the UI minimal and responsive, sized to its smallest acceptable footprint - it's a panel that's rarely looked at, so it should be okay being tiny.

5. **Raylib icons: switch to SVG.** Replace whatever icon approach the Raylib popup currently uses with SVG-based icons.

6. [DONE - already fixed in a415c9b] **Fix interpolation-type bug on in/out.** `setInterpolationTypeAtKey` is reasserted after `setTemporalEaseAtKey` so the untouched side's type sticks.

7. [DONE] **Foreground-app check for the shortcut system.** Hotkeys are now unregistered while AE isn't foreground and re-registered when it regains focus (see [HotkeyOverlay_Win32.cpp](AEGP/HotkeyOverlay_Win32.cpp)), instead of swallowing the keystroke for every app.

8. [DONE] **CEP UI: polling on/off toggle.** "Live preview while dragging" checkbox in [main.svelte](src/js/main/main.svelte), persisted to `barbar-settings.json`.

9. [DONE] **Esc broke the Raylib popup until AE restart.** `WindowShouldClose()` latched true on Esc and never cleared since the window is reused across popups - fixed in [HotkeyOverlay_Popup.cpp](AEGP/HotkeyOverlay_Popup.cpp) by disabling the default exit key and checking Esc manually.

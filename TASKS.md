
# Pre-release tasks

1. [DONE] **Raylib popup startup query: active comp + keyframe selection.** `keyframeSelectionQuery`/`keyframeSelectionReply` round trip already wired in [HotkeyOverlay_Win32.cpp](AEGP/HotkeyOverlay_Win32.cpp) and [bolt.ts](src/js/lib/utils/bolt.ts).

2. [DONE] **Easing popup behavior: modifier-gated in/out, no-selection toast.** `RunNoSelectionToast` in [HotkeyOverlay_Popup.cpp](AEGP/HotkeyOverlay_Popup.cpp) covers this.

3. [DONE] **ExtendScript: monotonic runs -> continuous bezier.** Implemented in [easingScript.ts](src/shared/easingScript.ts) - run membership cached on `$.__easeMemory` once per property per drag.

4. [DONE] **CEP UI cleanup.** Main panel default size dropped from 600x650 to 280x240, CSS tightened, and the unused Bolt-CEP scaffold sample/helloWorld ExtendScript functions deleted ([cep.config.ts](cep.config.ts), [main.svelte](src/js/main/main.svelte), [aeft.ts](src/jsx/aeft/aeft.ts)). Left the "floating" and "background" panels alone - both are load-bearing (background keeps the WS server alive independent of panel visibility; floating is real infra even though its content is currently a placeholder), and removing either is a product call, not a cleanup.

5. **Raylib icons: switch to SVG. Skipped, flagging instead of guessing.** The current icons (split-square "hold" button, hourglass easing glyph) are drawn as native raylib shapes (DrawRectangleRec/DrawTriangle) - already vector, crisp at any size, zero dependencies. Doing this for real needs: vendoring an SVG rasterizer (e.g. nanosvg, not currently in Vendor/ and not network-fetchable from this session), actual .svg icon artwork (a design decision, not a mechanical port), and visual QA in the live popup window that can't be done headlessly. Converting today's shapes 1:1 into SVG paths would only add a rasterize-to-bitmap step for no visual gain. Worth doing once there's real icon artwork to bring in - until then this is a placeholder-icon problem, not an SVG problem.

6. [DONE - already fixed in a415c9b] **Fix interpolation-type bug on in/out.** `setInterpolationTypeAtKey` is reasserted after `setTemporalEaseAtKey` so the untouched side's type sticks.

7. [DONE] **Foreground-app check for the shortcut system.** Hotkeys are now unregistered while AE isn't foreground and re-registered when it regains focus (see [HotkeyOverlay_Win32.cpp](AEGP/HotkeyOverlay_Win32.cpp)), instead of swallowing the keystroke for every app.

8. [DONE] **CEP UI: polling on/off toggle.** "Live preview while dragging" checkbox in [main.svelte](src/js/main/main.svelte), persisted to `barbar-settings.json`.

9. [DONE] **Esc broke the Raylib popup until AE restart.** `WindowShouldClose()` latched true on Esc and never cleared since the window is reused across popups - fixed in [HotkeyOverlay_Popup.cpp](AEGP/HotkeyOverlay_Popup.cpp) by disabling the default exit key and checking Esc manually.

# Next up

1. **Keep CEP alive without a visible panel.** `background` panel (cep.config.ts:46-56) already runs `initBolt()`/ws-server from AE launch via `autoVisible`. Gap: nothing relaunches it if that panel ever gets closed/unloaded. Plan: `WsClient` exposes `IsConnected()`; before showing the popup, AEGP checks it, and if disconnected, fires the panel's Window-menu command (AEGP command suite) to bring CEP back up.

2. **Strip CEP boilerplate.** Gut `src/js/main/main.svelte` / `app.svelte`. Replace with: a list of extension features (just "Easing" for now), each with a user-assignable shortcut. Persist the chosen shortcut to disk.

3. **Parse the AE keymap for clash-checking.** Format in `After Effects Default.txt`: `"CommandName" = "(Ctrl+Alt+Key)"`, ~674 entries. Parser feeds the shortcut picker so it can warn on collision with a stock AE binding. Actual hotkey registration/enforcement still lives in AEGP's `RegisterHotKey` call.

4. **Easing feature end-to-end.**
   - Slider: 0 = linear, 1-100 = ease amount (`HotkeyOverlay_Popup.cpp`).
   - Ctrl held = ease-in only, Shift held = ease-out only, neither = both.
   - On shortcut press, AEGP must ask CEP whether keyframes are selected *before* showing the popup. Current `WsClient`/ws-server is fire-and-forget AEGP→CEP only (`bolt.ts:219` just alerts on `slider`); needs a real request/response round trip.
   - No keyframes selected → skip the slider, show a toast at the cursor ("No keyframes are selected") that fades after ~1s, instead of the popup.

5. **Outgoing-handle-only hold button.** New `GuiButton` in the popup to set hold on the outgoing handle only (not both). Must hit-test the button *before* the existing "any click dismisses" logic (`HotkeyOverlay_Popup.cpp:85`), or clicking it just closes the popup.

6. **Show modifier state in the popup.** Draw "In only" / "Out only" near the slider when Ctrl/Shift is held — check each frame in the draw loop.

7. **ExtendScript: monotonic runs → continuous bezier.** For N≥3 selected keyframes forming an ascending or descending run, the *interior* keyframes of that run (not its endpoints) get continuous-bezier interpolation instead of the ease applied — easing an interior point of a monotonic run doesn't make sense.
   - Open question: confirm the endpoint rule. Example given: values `5,6,7,8,7` at indices 0-4. The run is 0-3 (5,6,7,8); "points 1 and 2" (values 6,7) become continuous-bezier. Need to confirm endpoints of a run keep normal easing in all cases, including when the run is the *whole* selection (no preceding/following keyframe to ease against).

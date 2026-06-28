# Next up

Order: #1 (shared WS plumbing) first, then 2/3/4/5/6/7 in order. Commit after each.

1. **WS plumbing: hotkey table push + dispatch + keyframe-selection query.**
   - CEP holds the only on-disk copy (JSON file under `csi.getSystemPath(SystemPath.USER_DATA)`), written by the shortcut picker (#2). Table rows are `[hotkey, function_name]`.
   - On every AEGP `"hello"` and every CEP-side edit, CEP sends `{"type":"hotkeys","payload":"id,vkey,mods,fn;..."}` — flat delimited string inside a JSON wrapper. AEGP extracts/`strtok`s the payload string itself; no JSON library added.
   - AEGP replaces its whole registration on receipt: `UnregisterHotKey` everything currently held, then `RegisterHotKey` fresh from the new table. No diffing.
   - Dispatch: AEGP keeps a small static map of native function names (just `"Easing"` today) it runs directly. Any `fn` it doesn't recognize, it sends `{"type":"hotkeyTriggered","fn":"..."}` over WS and CEP handles it.
   - Same request/response machinery also carries the keyframe-selection query (AEGP asks CEP "are keyframes selected?", blocks for the reply) needed by #4.

2. **Strip CEP boilerplate, add shortcut picker UI.** Gut `src/js/main/main.svelte` / `app.svelte`. List of extension features (starts with "Easing"), each with a user-assignable shortcut. Writes the on-disk table from #1 and pushes it over WS on every change.

3. **Parse the AE keymap for clash-checking.** `"CommandName" = "(Ctrl+Alt+Key)"` format in `After Effects Default.txt`, ~674 entries. Feeds the picker so it can warn on collision with a stock AE binding.

4. **Easing popup behavior: modifier-gated in/out, no-selection toast.** Uses #1's query: ask CEP if keyframes are selected before showing the popup. None selected -> fading "No keyframes are selected" toast at the cursor (~1s) instead of the slider popup. Slider 0=linear, 1-100=ease amount; Ctrl=in only, Shift=out only, neither=both.

5. **Outgoing-handle-only hold button.** New `GuiButton` in the popup (`HotkeyOverlay_Popup.cpp`) for hold-on-outgoing-handle-only. Must hit-test before the existing "any click dismisses" logic (`HotkeyOverlay_Popup.cpp:85`).

6. **Show modifier state in the popup.** Draw "In only" / "Out only" near the slider when Ctrl/Shift held, checked each frame in the draw loop.

7. **ExtendScript: monotonic runs -> continuous bezier.** For N>=3 selected keyframes forming an ascending or descending run, interior keyframes of that run (not the run's own first/last) get continuous-bezier instead of the ease applied. No special-casing needed for whether a run's endpoint is also the whole selection's first/last keyframe — only interior-of-run position matters.

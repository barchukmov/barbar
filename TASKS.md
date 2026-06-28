
# Pre-release tasks

Items 1-3 below were already noted but are not working yet. Items 4-8 are new.

1. **Raylib popup startup query: active comp + keyframe selection.** On popup open, ask CEP whether there's an active comp and whether keyframes are selected. No ExtendScript `alert()` for "no keyframes selected" etc. - that check has to be reported to the AEGP side and shown as a floating message that closes itself after ~1s, not block via JS alert. If no active comp, no-op or show the same kind of toast.

2. **Easing popup behavior: modifier-gated in/out, no-selection toast.** Uses #1's query. None selected -> fading "No keyframes are selected" toast at the cursor (~1s) instead of the slider popup. Slider 0=linear, 1-100=ease amount; Ctrl=in only, Shift=out only, neither=both.

3. **ExtendScript: monotonic runs -> continuous bezier.** For N>=3 selected keyframes forming an ascending or descending run, interior keyframes of that run (not the run's own first/last) get continuous-bezier instead of the ease applied, only when continuous-bezier interpolation is the chosen mode (otherwise treat normally). Calculate this once when saving to memory, not on every poll.

4. **CEP UI cleanup.** Strip the Panelator/AEGP boilerplate panel. Make the UI minimal and responsive, sized to its smallest acceptable footprint - it's a panel that's rarely looked at, so it should be okay being tiny.

5. **Raylib icons: switch to SVG.** Replace whatever icon approach the Raylib popup currently uses with SVG-based icons.

6. **Fix interpolation-type bug on in/out.** Using the in/out feature in the ExtendScript currently overrides the keyframe's interpolation type. It must preserve the existing type (e.g. a linear keyframe should stay linear) instead of stomping it.

7. **Foreground-app check for the shortcut system.** The global keystroke hook currently intercepts keys system-wide regardless of focused app. It must check whether After Effects is the active/foreground application and do nothing (let the keystroke pass through) when it isn't.

8. **CEP UI: polling on/off toggle.** Add a control in the CEP panel to disable/enable polling.

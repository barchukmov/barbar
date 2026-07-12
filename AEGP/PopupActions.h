#pragma once
#include <string>

// The popup's AE-side actions. In the .aex these drive the typed jsx
// functions ($["barbar"].applyEase/cancelEase/... - see src/jsx/aeft/aeft.ts)
// through the ScriptRunner; PopupExe/StubActions.cpp swaps in printf stubs so
// the popup UI runs standalone. No windows.h or raylib types here - both
// sides of the popup bridge include this.

// Called once at popup open: re-reads the polling setting
// (%APPDATA%\barbar-settings.json) and caches it for the popup's lifetime.
void PopupSessionBegin();

// applyEase(value, mode, isPreview). Preview ticks are dropped while the
// user has polling disabled; the commit (preview=false) always goes through.
// Queued previews coalesce to the newest, and a commit or cancel replaces a
// still-queued preview, so a busy AE main thread applies the latest state
// once instead of replaying the drag history.
void PopupApplyEase(int value, const char* mode, bool preview);

// Esc: restore every touched keyframe from its pre-drag snapshot.
void PopupCancelEase();

// Hold-Out button: set the selected keys' outgoing handles to Hold.
void PopupHoldOutgoing();

// Blocking query backing the popup-vs-toast decision; as a side effect it
// begins the ease session (snapshots every selected key's pre-drag state
// while AE is still foreground - flag reads with the popup up misreport).
// Errs toward "selected" when AE's main thread doesn't answer in time (busy
// rendering) - showing the popup is a UX nicety, not a guarantee.
bool IsAnyKeyframeSelected();

// A hotkey fn with no native handler is treated as the name of a $["barbar"]
// jsx function and called with no arguments - the hotkey table is how new
// jsx tools get bound to shortcuts, no C++ change needed. Unknown names
// no-op (the wrapper's try/catch eats the TypeError).
void RunJsxFunctionByName(const std::string& fn);

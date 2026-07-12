#pragma once
#include <string>

// Runs ExtendScript in AE from any thread. AEGP_ExecuteScript is main-thread
// only, but the callers (the popup, on the hotkey overlay thread) aren't -
// so calls are queued here and an AEGP idle hook, registered at plugin init,
// drains the queue on AE's main thread. AEGP_CauseIdleRoutinesToBeCalled is
// the one documented thread-safe way to poke that hook awake. This is the
// replacement for the old WebSocket hop (AEGP -> ws -> CEP -> evalTS), which
// reached the same main thread through two extra processes' worth of
// plumbing.
//
// The `call` strings are expressions on the plugin's jsx namespace - e.g.
// `applyEase(50,"both",true)` runs `$["barbar"].applyEase(50,"both",true)`.
// The namespace comes from barbar-jsx.js (the compiled src/jsx bundle,
// shipped next to the .aex by build.ps1), lazily $.evalFile'd into AE's main
// ExtendScript engine on the first call of the session.
struct SPBasicSuite;

// Main thread only (the plugin ctor): saves off the suite pointers - the
// acquisition routines aren't thread-safe, so they can't be fetched later
// from the hotkey thread - and registers the idle hook.
void StartScriptRunner(SPBasicSuite* pica_basicP, long pluginID);
void StopScriptRunner();

// Fire-and-forget: queues the call and returns. Entries sharing a non-empty
// coalesceKey collapse to the newest while queued (keeping the oldest's
// queue position), so a busy main thread applies one up-to-date ease per
// drain instead of replaying the whole drag history.
void RunJsxAsync(const std::string& call, const char* coalesceKey = nullptr);

// Blocking: waits for the idle hook to run the call and hand the result
// back (String()-ified by the wrapper). false on timeout - AE's main thread
// was too busy to go idle - in which case a late execution's result is
// discarded rather than satisfying the next request.
bool RunJsxSync(const std::string& call, int timeoutMs, std::string& resultOut);

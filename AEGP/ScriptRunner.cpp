#include "ScriptRunner.h"
#include "win32_popup_bridge.h" // GetJsxBundlePath

#include "AEConfig.h"
#include <windows.h>
#include "entry.h"
#include "AE_GeneralPlug.h"

#include <condition_variable>
#include <deque>
#include <mutex>

namespace {
	SPBasicSuite* g_sp = nullptr;
	AEGP_PluginID g_pluginID = 0;
	// Acquired once at StartScriptRunner and never released: the idle hook
	// can't be unregistered, so releasing these at death would leave a hook
	// that could still fire against dead suite pointers. AEGPs only unload at
	// AE shutdown, where AE reclaims everything anyway - the same lifetime
	// deal the death hook itself relies on.
	AEGP_UtilitySuite6* g_utility = nullptr;
	AEGP_MemorySuite1* g_memory = nullptr;

	std::mutex g_mutex;
	std::condition_variable g_syncCv;
	bool g_started = false;

	struct QueuedScript { std::string script; std::string coalesceKey; };
	std::deque<QueuedScript> g_queue;

	// One blocking request at a time is enough - only the hotkey thread calls
	// RunJsxSync, and only while no popup is up. The generation counter lets
	// a timed-out request's late result be discarded instead of satisfying
	// whatever request comes next.
	long g_syncGeneration = 0;
	bool g_syncPending = false;
	std::string g_syncScript;
	bool g_syncDone = false;
	std::string g_syncResult;

	std::string g_jsxPath; // forward-slashed so it splices into a JS literal

	// Copies out and frees an AEGP_MemHandle in one move - both ExecuteScript
	// out-params are handles the caller must dispose of.
	std::string HandleToString(AEGP_MemHandle h)
	{
		if (!h) return "";
		std::string out;
		void* p = nullptr;
		if (g_memory->AEGP_LockMemHandle(h, &p) == A_Err_NONE && p) {
			out = static_cast<const char*>(p);
			g_memory->AEGP_UnlockMemHandle(h);
		}
		g_memory->AEGP_FreeMemHandle(h);
		return out;
	}

	// Main thread only (called from the idle hook).
	std::string ExecScript(const std::string& script)
	{
		AEGP_MemHandle resultH = nullptr;
		AEGP_MemHandle errorH = nullptr;
		// FALSE = the script (and result/error strings) are UTF-8, not the
		// current application encoding.
		g_utility->AEGP_ExecuteScript(g_pluginID, script.c_str(), FALSE, &resultH, &errorH);
		std::string error = HandleToString(errorH);
		if (!error.empty()) {
			OutputDebugStringA(("[barbar jsx error] " + error + "\n").c_str());
		}
		std::string result = HandleToString(resultH);
		if (result.rfind("ERR:", 0) == 0) {
			OutputDebugStringA(("[barbar jsx] " + result + "\n").c_str());
		}
		return result;
	}

	// Wraps a call on the jsx namespace with the lazy bundle load and a
	// try/catch, so a missing bundle or a throwing function comes back as an
	// "ERR:" result string instead of an unhandled script error. $["barbar"]
	// is the same namespace object CEP's evalTS calls into - the bundle
	// (compiled from src/jsx/aeft/aeft.ts) assigns it on load.
	std::string BuildScript(const std::string& call)
	{
		return
			"(function(){try{"
			"var ns=$[\"barbar\"];"
			"if(typeof ns===\"undefined\"){$.evalFile(\"" + g_jsxPath + "\");ns=$[\"barbar\"];}"
			"if(typeof ns===\"undefined\")return\"ERR:barbar-jsx.js missing or failed to load\";"
			"return String(ns." + call + ");"
			"}catch(e){return\"ERR:\"+e;}})()";
	}

	// AE services idle routines from inside AEGP_ExecuteScript - the same
	// mid-script event pumping that lets Esc abort a running script. When the
	// popup thread queues a call and pokes AEGP_CauseIdleRoutinesToBeCalled
	// while a script is executing, this hook re-enters, and without a guard
	// the next queued script runs *nested between two statements* of the one
	// still running - a preview tick's keyframe writes land in the middle of
	// another tick's snapshot reads, corrupting any state read/decide/write
	// sequence. Main thread only, so a plain flag under the mutex suffices;
	// the re-entered invocation returns and the outer drain loop picks the
	// queue back up after the current script finishes.
	bool g_executing = false;

	A_Err IdleHook(AEGP_GlobalRefcon, AEGP_IdleRefcon, A_long*)
	{
		// Drain everything queued: a sync request first (a caller is blocked
		// on it), then the async queue in order.
		for (;;) {
			std::string script;
			bool isSync = false;
			long generation = 0;
			{
				std::lock_guard<std::mutex> lock(g_mutex);
				if (!g_started || g_executing) return A_Err_NONE;
				if (g_syncPending) {
					script = g_syncScript;
					g_syncPending = false;
					isSync = true;
					generation = g_syncGeneration;
				} else if (!g_queue.empty()) {
					script = g_queue.front().script;
					g_queue.pop_front();
				} else {
					return A_Err_NONE;
				}
				g_executing = true;
			}
			std::string result = ExecScript(script);
			{
				std::lock_guard<std::mutex> lock(g_mutex);
				g_executing = false;
				if (isSync) {
					// A stale generation means the waiter already timed out
					// and moved on - drop the result.
					if (generation == g_syncGeneration) {
						g_syncResult = result;
						g_syncDone = true;
						g_syncCv.notify_all();
					}
				}
			}
		}
	}
}

void StartScriptRunner(SPBasicSuite* pica_basicP, long pluginID)
{
	g_sp = pica_basicP;
	g_pluginID = (AEGP_PluginID)pluginID;

	g_sp->AcquireSuite(kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6, (const void**)&g_utility);
	g_sp->AcquireSuite(kAEGPMemorySuite, kAEGPMemorySuiteVersion1, (const void**)&g_memory);

	bool hookRegistered = false;
	AEGP_RegisterSuite5* registerSuite = nullptr;
	g_sp->AcquireSuite(kAEGPRegisterSuite, kAEGPRegisterSuiteVersion5, (const void**)&registerSuite);
	if (registerSuite) {
		hookRegistered = registerSuite->AEGP_RegisterIdleHook(g_pluginID, IdleHook, nullptr) == A_Err_NONE;
		g_sp->ReleaseSuite(kAEGPRegisterSuite, kAEGPRegisterSuiteVersion5);
	}

	g_jsxPath = GetJsxBundlePath();
	for (auto& c : g_jsxPath) {
		if (c == '\\') c = '/';
	}

	std::lock_guard<std::mutex> lock(g_mutex);
	g_started = g_utility && g_memory && hookRegistered;
}

void StopScriptRunner()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	g_started = false;
	g_queue.clear();
	++g_syncGeneration; // orphan any in-flight sync execution
	g_syncCv.notify_all(); // wake a blocked RunJsxSync so it fails fast
}

void RunJsxAsync(const std::string& call, const char* coalesceKey)
{
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		if (!g_started) return;
		std::string script = BuildScript(call);
		bool coalesced = false;
		if (coalesceKey && *coalesceKey) {
			for (auto& queued : g_queue) {
				if (queued.coalesceKey == coalesceKey) {
					queued.script = script; // newest wins, position kept
					coalesced = true;
					break;
				}
			}
		}
		if (!coalesced) {
			g_queue.push_back({ script, coalesceKey ? coalesceKey : "" });
		}
	}
	g_utility->AEGP_CauseIdleRoutinesToBeCalled();
}

bool RunJsxSync(const std::string& call, int timeoutMs, std::string& resultOut)
{
	std::unique_lock<std::mutex> lock(g_mutex);
	if (!g_started) return false;
	++g_syncGeneration;
	g_syncScript = BuildScript(call);
	g_syncPending = true;
	g_syncDone = false;
	lock.unlock();

	g_utility->AEGP_CauseIdleRoutinesToBeCalled();

	lock.lock();
	bool done = g_syncCv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
		[] { return g_syncDone || !g_started; });
	if (!done || !g_syncDone) {
		// Timed out (or the plugin is shutting down): clear pending in case
		// the idle hook hasn't picked the script up yet, and bump the
		// generation so a pick-up already in flight gets discarded.
		g_syncPending = false;
		++g_syncGeneration;
		return false;
	}
	resultOut = g_syncResult;
	return true;
}

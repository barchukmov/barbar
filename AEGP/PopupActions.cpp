#include "PopupActions.h"
#include "ScriptRunner.h"
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {
	bool g_pollingEnabled = true;

	// Preview, commit, and cancel share one coalesce key: at most one of them
	// is ever queued, and the newest is the only one that matters - a preview
	// still queued behind a busy main thread must not run after (or instead
	// of) the commit/cancel that superseded it.
	const char* kEaseKey = "applyEase";

	// %APPDATA%\barbar-settings.json is written only by CEP
	// (JSON.stringify, compact) - a substring match is enough for a flat
	// format both ends own, same philosophy as the hotkey-file reader.
	bool ReadPollingEnabled()
	{
		const char* appData = std::getenv("APPDATA");
		if (!appData) return true;
		std::ifstream file(std::string(appData) + "\\barbar-settings.json");
		if (!file) return true; // never saved - polling defaults on
		std::stringstream buf;
		buf << file.rdbuf();
		return buf.str().find("\"pollingEnabled\":false") == std::string::npos;
	}
}

void PopupSessionBegin()
{
	g_pollingEnabled = ReadPollingEnabled();
}

void PopupApplyEase(int value, const char* mode, bool preview)
{
	if (preview && !g_pollingEnabled) return;
	RunJsxAsync("applyEase(" + std::to_string(value) + ",\"" + mode + "\","
	            + (preview ? "true" : "false") + ")", kEaseKey);
}

void PopupCancelEase()
{
	RunJsxAsync("cancelEase()", kEaseKey);
}

void PopupHoldOutgoing()
{
	// No coalesce key: a queued preview should still land first (in queue
	// order) so the hold applies on top of the previewed ease, matching what
	// the user saw - hold only touches the outgoing interpolation type.
	RunJsxAsync("setOutgoingHandleHold()");
}

bool IsAnyKeyframeSelected()
{
	std::string result;
	// beginEaseSession answers the selection query and snapshots every
	// selected key's pre-drag state. The snapshot must be taken here -
	// synchronously, before the popup window exists - because keyframe flag
	// reads taken while the popup is up misreport (keyTemporalAutoBezier
	// returns false for hand-made auto-bezier keys); reads at hotkey time,
	// with AE still the foreground window, are reliable. On timeout the
	// popup shows anyway and applyEase's first tick snapshots as a fallback.
	if (!RunJsxSync("beginEaseSession()", 1000, result)) return true;
	return result != "false";
}

void RunJsxFunctionByName(const std::string& fn)
{
	// fn comes from the user-editable hotkey file - keep identifier chars
	// only so it splices into the script as a plain property access.
	std::string safe;
	for (char c : fn) {
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
		    || (c >= '0' && c <= '9') || c == '_' || c == '$') {
			safe += c;
		}
	}
	if (safe.empty()) return;
	RunJsxAsync(safe + "()");
}

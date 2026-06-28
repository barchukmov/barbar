#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "win32_popup_bridge.h"
#include <string>

namespace {
	// Module-relative path is resolved once and cached - it can't change at runtime.
	std::string g_fontPath;
}

const char* GetFontPath()
{
	if (g_fontPath.empty()) {
		HMODULE module = nullptr;
		GetModuleHandleExA(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)&GetFontPath, &module);
		char path[MAX_PATH];
		GetModuleFileNameA(module, path, MAX_PATH);
		std::string dir(path);
		dir = dir.substr(0, dir.find_last_of("\\/"));
		g_fontPath = dir + "\\Fonts\\Mannin-Regular.otf";
	}
	return g_fontPath.c_str();
}

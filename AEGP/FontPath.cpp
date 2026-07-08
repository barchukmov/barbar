#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "win32_popup_bridge.h"
#include <string>
#include <unordered_map>

namespace {
	std::string ModuleDir()
	{
		HMODULE module = nullptr;
		GetModuleHandleExA(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)&ModuleDir, &module);
		char path[MAX_PATH];
		GetModuleFileNameA(module, path, MAX_PATH);
		std::string dir(path);
		return dir.substr(0, dir.find_last_of("\\/"));
	}

	// Module-relative path is resolved once and cached per file - it can't
	// change at runtime.
	std::string g_fontPath;
	std::string g_monoFontPath;
	std::unordered_map<std::string, std::string> g_iconPaths;
}

const char* GetFontPath()
{
	if (g_fontPath.empty()) {
		g_fontPath = ModuleDir() + "\\Fonts\\Mannin-Regular.otf";
	}
	return g_fontPath.c_str();
}

const char* GetMonoFontPath()
{
	if (g_monoFontPath.empty()) {
		// Consolas ships with every Windows since Vista - loading it straight
		// from the system fonts folder avoids bundling a second font just for
		// the slider's numeric readout.
		char winDir[MAX_PATH];
		GetWindowsDirectoryA(winDir, MAX_PATH);
		g_monoFontPath = std::string(winDir) + "\\Fonts\\consola.ttf";
	}
	return g_monoFontPath.c_str();
}

const char* GetIconPath(const char* filename)
{
	auto it = g_iconPaths.find(filename);
	if (it == g_iconPaths.end()) {
		it = g_iconPaths.emplace(filename, ModuleDir() + "\\Icons\\" + filename).first;
	}
	return it->second.c_str();
}

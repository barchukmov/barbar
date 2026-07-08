#pragma once

#include "AEConfig.h"
#ifdef AE_OS_WIN
	#include <windows.h>
#endif

#include "entry.h"
#include "AE_GeneralPlug.h"
#include "AE_Macros.h"
// AEGP_SuiteHandler and the SDK helper headers it leans on want DEBUG defined
// before they're pulled in (the original sample had a hard #error otherwise).
#ifndef DEBUG
	#define DEBUG
#endif
#include "SuiteHelper.h"
#include "SimpleSuiteHelper.h"
#include "AEGP_SuiteHandler.h"
#include "PT_Err.h"

// This entry point is exported through the PiPL (AegpDemo_PiPL.r / .rc).
extern "C" DllExport AEGP_PluginInitFuncPrototype EntryPointFunc;

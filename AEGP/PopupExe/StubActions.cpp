// Stands in for the real AE-side actions (PopupActions.cpp -> ScriptRunner ->
// AEGP_ExecuteScript) when running the popup UI standalone - prints what
// would have run in AE instead.
#include "PopupActions.h"
#include <cstdio>

void PopupSessionBegin()
{
	printf("PopupSessionBegin\n");
}

void PopupApplyEase(int value, const char* mode, bool preview)
{
	printf("applyEase(%d, \"%s\", %s)\n", value, mode, preview ? "true" : "false");
}

void PopupCancelEase()
{
	printf("cancelEase()\n");
}

void PopupHoldOutgoing()
{
	printf("setOutgoingHandleHold()\n");
}

bool IsAnyKeyframeSelected()
{
	return true;
}

void RunJsxFunctionByName(const std::string& fn)
{
	printf("jsx: %s()\n", fn.c_str());
}

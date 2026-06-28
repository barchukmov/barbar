// Stands in for the real CEP WebSocket link when testing the popup UI
// standalone - prints what would have been sent instead of connecting.
#include "WsClient.h"
#include <cstdio>

void WsSend(const std::string& json)
{
	printf("WsSend: %s\n", json.c_str());
}

std::string WsRequest(const std::string&, const std::string&, int)
{
	return ""; // unused by the popup UI; RunNativeFunction (real WS code) isn't part of this build
}

std::string WsJsonGetString(const std::string&, const std::string&)
{
	return "";
}

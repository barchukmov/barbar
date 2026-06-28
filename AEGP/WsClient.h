#pragma once
#include <string>
#include <functional>

// WebSocket client connecting to the CEP-hosted server (see src/js/lib/ws-server.ts).
// CEP is the server; this is the client, since AEGP's process lifetime
// (the .aex inside AfterFX.exe) is the less stable end of the two.
void StartWsClient(const std::string& url, std::function<void(const std::string&)> onMessage);
void StopWsClient();
void WsSend(const std::string& json);

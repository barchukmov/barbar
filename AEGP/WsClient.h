#pragma once
#include <string>
#include <functional>

// WebSocket client connecting to the CEP-hosted server (see src/js/lib/ws-server.ts).
// CEP is the server; this is the client, since AEGP's process lifetime
// (the .aex inside AfterFX.exe) is the less stable end of the two.
void StartWsClient(const std::string& url, std::function<void(const std::string&)> onMessage);
void StopWsClient();
void WsSend(const std::string& json);

// Sends requestJson, then blocks (up to timeoutMs) for the next message
// whose "type" field equals expectedReplyType. Returns the raw reply JSON,
// or "" on timeout - callers treat a timeout as "couldn't confirm", not an
// error, since this is a best-effort UX check, not a transactional protocol.
std::string WsRequest(const std::string& requestJson, const std::string& expectedReplyType, int timeoutMs);

// Extracts the value of a top-level "key":"value" string field. ponytail:
// hand-rolled substring search instead of a JSON library - the wire format
// is small, flat, and fully owned by both ends (loopback-only), so a real
// parser would be solving a problem we don't have.
std::string WsJsonGetString(const std::string& json, const std::string& key);

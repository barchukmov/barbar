#include "WsClient.h"
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXNetSystem.h>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace {
	ix::WebSocket g_ws;
	bool g_netInitialized = false;

	std::mutex g_replyMutex;
	std::condition_variable g_replyCv;
	std::string g_awaitingType; // empty when nobody is waiting
	std::string g_reply;
	bool g_hasReply = false;
}

std::string WsJsonGetString(const std::string& json, const std::string& key)
{
	std::string needle = "\"" + key + "\":\"";
	size_t start = json.find(needle);
	if (start == std::string::npos) return "";
	start += needle.size();
	size_t end = json.find('"', start);
	if (end == std::string::npos) return "";
	return json.substr(start, end - start);
}

void StartWsClient(const std::string& url, std::function<void(const std::string&)> onMessage)
{
	if (!g_netInitialized) {
		ix::initNetSystem();
		g_netInitialized = true;
	}

	g_ws.setUrl(url);
	// ix::WebSocket reconnects automatically by default after the first connect.

	g_ws.setOnMessageCallback([onMessage](const ix::WebSocketMessagePtr& msg) {
		if (msg->type == ix::WebSocketMessageType::Open) {
			g_ws.send(R"({"type":"hello"})");
		} else if (msg->type == ix::WebSocketMessageType::Message) {
			{
				std::lock_guard<std::mutex> lock(g_replyMutex);
				if (!g_awaitingType.empty() && WsJsonGetString(msg->str, "type") == g_awaitingType) {
					g_reply = msg->str;
					g_hasReply = true;
					g_replyCv.notify_one();
				}
			}
			onMessage(msg->str);
		}
	});

	g_ws.start(); // runs on its own background thread
}

void StopWsClient()
{
	g_ws.stop();
	if (g_netInitialized) {
		ix::uninitNetSystem();
		g_netInitialized = false;
	}
}

void WsSend(const std::string& json)
{
	g_ws.send(json);
}

std::string WsRequest(const std::string& requestJson, const std::string& expectedReplyType, int timeoutMs)
{
	std::unique_lock<std::mutex> lock(g_replyMutex);
	g_awaitingType = expectedReplyType;
	g_hasReply = false;
	g_reply.clear();
	lock.unlock();

	g_ws.send(requestJson);

	lock.lock();
	g_replyCv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [] { return g_hasReply; });
	g_awaitingType.clear();
	return g_hasReply ? g_reply : "";
}

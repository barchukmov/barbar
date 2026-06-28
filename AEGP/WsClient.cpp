#include "WsClient.h"
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXNetSystem.h>

namespace {
	ix::WebSocket g_ws;
	bool g_netInitialized = false;
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

// WebSocket server CEP hosts for the AEGP plugin to connect to.
// AEGP is the only client; CEP is the server because the panel's lifetime
// is what's stable here (the .aex can reconnect on AE restarts/reloads).
const WS = (
  typeof window.cep !== "undefined" ? require("ws") : {}
) as typeof import("ws");

const PORT = 41420;

let aegpSocket: import("ws").WebSocket | null = null;
const listeners = new Set<(msg: any) => void>();

export const startWsServer = () => {
  if (!window.cep) return;
  const server = new WS.Server({ port: PORT });

  // ponytail: both the main and floating panels load this same bundle, so
  // whichever loads second hits EADDRINUSE - that's expected, not an error.
  server.on("error", (err: any) => {
    if (err?.code !== "EADDRINUSE") throw err;
  });

  server.on("connection", (socket) => {
    aegpSocket = socket;
    console.log("[ws] AEGP connected");

    socket.on("message", (data) => {
      let msg: any;
      try {
        msg = JSON.parse(data.toString());
      } catch {
        return;
      }
      if (msg?.type === "hello") sendToAegp({ type: "ack" });
      listeners.forEach((fn) => fn(msg));
    });

    socket.on("close", () => {
      if (aegpSocket === socket) aegpSocket = null;
      console.log("[ws] AEGP disconnected");
    });
  });

  return server;
};

export const sendToAegp = (msg: any) => {
  aegpSocket?.send(JSON.stringify(msg));
};

export const onAegpMessage = (fn: (msg: any) => void) => {
  listeners.add(fn);
  return () => listeners.delete(fn);
};

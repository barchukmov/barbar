// WebSocket server CEP hosts for the AEGP plugin to connect to.
// AEGP is the only client; CEP is the server because the panel's lifetime
// is what's stable here (the .aex can reconnect on AE restarts/reloads).
import CSInterface from "./cep/csinterface";
import { fs } from "./cep/node";

const WS = (
  typeof window.cep !== "undefined" ? require("ws") : {}
) as typeof import("ws");

const PORT = 41420;

let aegpSocket: import("ws").WebSocket | null = null;
const listeners = new Set<(msg: any) => void>();

// Hotkey table: CEP is the only reader/writer on disk. AEGP never touches
// the file - it gets the table pushed over the socket on every connect and
// every edit, see sendHotkeyTable(). [id, vkey, mods, fn] per binding; mods
// is a bitmask independent of any OS's MOD_* constants (bit0=ctrl,
// bit1=shift, bit2=alt) so the wire format doesn't leak Win32 specifics.
export type HotkeyBinding = {
  id: number;
  vkey: number;
  mods: number;
  fn: string;
};

const csi = new CSInterface();
const hotkeysFile = () => `${csi.getSystemPath("userData")}/barbar-hotkeys.json`;

// Ctrl+H -> Easing, matching the hotkey that was hardcoded before this table
// existed, so behavior is unchanged until the shortcut picker UI writes a
// real table.
const defaultHotkeyTable = (): HotkeyBinding[] => [
  { id: 1, vkey: 0x48, mods: 1, fn: "Easing" },
];

export const loadHotkeyTable = (): HotkeyBinding[] => {
  if (!window.cep) return [];
  try {
    return JSON.parse(fs.readFileSync(hotkeysFile(), "utf8"));
  } catch {
    const table = defaultHotkeyTable();
    saveHotkeyTable(table);
    return table;
  }
};

export const saveHotkeyTable = (bindings: HotkeyBinding[]) => {
  if (!window.cep) return;
  fs.writeFileSync(hotkeysFile(), JSON.stringify(bindings));
};

const encodeHotkeyPayload = (bindings: HotkeyBinding[]) =>
  bindings.map((b) => `${b.id},${b.vkey},${b.mods},${b.fn}`).join(";");

// Pushes the current table to AEGP. AEGP always replaces its whole
// registration on receipt (unregister everything, register fresh) - no
// diffing - so this is safe to call on every connect and on every edit.
export const sendHotkeyTable = () => {
  sendToAegp({ type: "hotkeys", payload: encodeHotkeyPayload(loadHotkeyTable()) });
};

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
      if (msg?.type === "hello") {
        sendToAegp({ type: "ack" });
        sendHotkeyTable();
      }
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

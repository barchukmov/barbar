// WebSocket server CEP hosts for the AEGP plugin to connect to.
// AEGP is the only client; CEP is the server because the panel's lifetime
// is what's stable here (the .aex can reconnect on AE restarts/reloads).
import CSInterface, { CSEvent } from "./cep/csinterface";
import { fs } from "./cep/node";

const WS = (
  typeof window.cep !== "undefined" ? require("ws") : {}
) as typeof import("ws");

const PORT = 41420;

let aegpSocket: import("ws").WebSocket | null = null;
const listeners = new Set<(msg: any) => void>();

// Hotkey table: the file (%APPDATA%/barbar-hotkeys.json) is the single
// source of truth and the contract with the AEGP. CEP is its only writer;
// the AEGP reads it directly - at AE launch (so hotkeys work before any
// panel exists) and again whenever we ping "hotkeysChanged" over the socket
// (see LoadHotkeyPayloadFromFile in HotkeyOverlay_Win32.cpp). The table
// itself never crosses the wire. mods is a bitmask independent of any OS's
// MOD_* constants (bit0=ctrl, bit1=shift, bit2=alt) so the file format
// doesn't leak Win32 specifics.
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

// "Polling" = the live-preview applyEase tick AEGP sends on every slider-drag
// frame (not just on commit) - some users would rather only see the result
// on release. Stored separately from the hotkey table since it's a CEP-only
// setting AEGP never needs to read.
const settingsFile = () => `${csi.getSystemPath("userData")}/barbar-settings.json`;

export const loadPollingEnabled = (): boolean => {
  if (!window.cep) return true;
  try {
    const settings = JSON.parse(fs.readFileSync(settingsFile(), "utf8"));
    return settings.pollingEnabled !== false;
  } catch {
    return true;
  }
};

export const savePollingEnabled = (enabled: boolean) => {
  if (!window.cep) return;
  fs.writeFileSync(settingsFile(), JSON.stringify({ pollingEnabled: enabled }));
};

// Tells AEGP to re-read the hotkey file. AEGP always replaces its whole
// registration on a re-read (unregister everything, register fresh) - no
// diffing - so this is safe to send on every connect and on every edit.
export const pingHotkeysChanged = () => {
  loadHotkeyTable(); // creates the default file if missing, so AEGP finds one
  sendToAegp({ type: "hotkeysChanged" });
};

// Cross-context "the hotkey file changed" broadcast. The ws server lives in
// whichever panel context won the port (normally `background`), which is
// usually NOT the context where the user edits hotkeys (`main`) - a plain
// pingHotkeysChanged() from main would hit a null aegpSocket and silently go
// nowhere. So the editing panel calls this: ping locally (covers the case
// where it does own the socket) and dispatch a CSEvent so the owning context
// pings too. Double delivery is harmless (see above).
const hotkeysChangedEvent = "barbar.hotkeysChanged";

export const notifyHotkeysChanged = () => {
  pingHotkeysChanged();
  csi.dispatchEvent(new CSEvent(hotkeysChangedEvent, "APPLICATION", undefined, undefined));
};

export const startWsServer = () => {
  if (!window.cep) return;

  // Every context listens; only the one holding aegpSocket actually delivers
  // (sendToAegp is a no-op on the others).
  csi.addEventListener(hotkeysChangedEvent, () => pingHotkeysChanged(), undefined);

  // All three panels (main/floating/background) load this same bundle, so
  // every context but the first hits EADDRINUSE. That's expected - but keep
  // retrying instead of giving up: if the owning context dies (e.g. its panel
  // is closed), a surviving context adopts the port and AEGP's auto-reconnect
  // finds it, instead of the bridge staying dead until AE restarts.
  const listen = () => {
    const server = new WS.Server({ port: PORT });

    server.on("error", (err: any) => {
      if (err?.code !== "EADDRINUSE") throw err;
      server.close();
      setTimeout(listen, 3000);
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
          // Mostly redundant now that AEGP seeds from the file at launch, but
          // it covers the file having been created/changed while AEGP had no
          // server to hear a ping from (e.g. very first run).
          pingHotkeysChanged();
        }
        listeners.forEach((fn) => fn(msg));
      });

      socket.on("close", () => {
        if (aegpSocket === socket) aegpSocket = null;
        console.log("[ws] AEGP disconnected");
      });
    });
  };
  listen();
};

export const sendToAegp = (msg: any) => {
  aegpSocket?.send(JSON.stringify(msg));
};

export const onAegpMessage = (fn: (msg: any) => void) => {
  listeners.add(fn);
  return () => listeners.delete(fn);
};

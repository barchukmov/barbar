// The two prefs files CEP owns, both in %APPDATA% (CEP's "userData" path).
// They're the whole CEP<->AEGP contract now: CEP writes, the AEGP reads -
// the hotkey table at AE launch and whenever its directory watch sees the
// file change (HotkeyOverlay_Win32.cpp), the polling flag at each popup open
// (PopupActions.cpp). The write itself is the change notification; nothing
// else needs to be pinged.
import CSInterface from "./cep/csinterface";
import { fs } from "./cep/node";

// Hotkey table: [{id,vkey,mods,fn},...]. mods is a bitmask independent of
// any OS's MOD_* constants (bit0=ctrl, bit1=shift, bit2=alt) so the file
// format doesn't leak Win32 specifics. fn is either a native AEGP function
// ("Easing" - the popup) or the name of a $["barbar"] jsx function the AEGP
// calls with no arguments.
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
    // First run: write the default so the AEGP finds a table on disk.
    const table = defaultHotkeyTable();
    saveHotkeyTable(table);
    return table;
  }
};

export const saveHotkeyTable = (bindings: HotkeyBinding[]) => {
  if (!window.cep) return;
  fs.writeFileSync(hotkeysFile(), JSON.stringify(bindings));
};

// "Polling" = the live-preview applyEase tick the popup runs on every
// slider-drag frame (not just on commit) - some users would rather only see
// the result on release. The AEGP reads this file compactly (substring
// match, PopupActions.cpp), so keep it flat JSON.stringify output.
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

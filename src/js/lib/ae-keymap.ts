// Parses After Effects' own keyboard shortcut file so the picker in
// main.svelte can warn when a chosen combo collides with a stock AE binding.
import { fs, os, path } from "./cep/node";

// ponytail: version folder hardcoded for the current AE install - point
// this at the right "aeks" folder if/when this needs to track AE upgrades.
const KEYMAP_PATH = () =>
  path.join(
    os.homedir(),
    "AppData",
    "Roaming",
    "Adobe",
    "After Effects",
    "26.3",
    "aeks",
    "After Effects Default.txt"
  );

const MOD_BITS: Record<string, number> = { Ctrl: 1, Shift: 2, Alt: 4 };

// Only the tokens our own shortcut picker can ever produce need a mapping -
// the keymap file also uses names (DoubleQuote, PadUxFFFF hex, etc.) for
// keys main.svelte's picker doesn't support capturing, so those just never
// match and are skipped below.
const NAMED_VKEYS: Record<string, number> = {
  Enter: 0x0d,
  Return: 0x0d,
  Delete: 0x2e,
  Backspace: 0x08,
  Tab: 0x09,
  Esc: 0x1b,
  LeftArrow: 0x25,
  UpArrow: 0x26,
  RightArrow: 0x27,
  DownArrow: 0x28,
  Space: 0x20,
  F1: 0x70,
  F2: 0x71,
  F3: 0x72,
  F4: 0x73,
  F5: 0x74,
  F6: 0x75,
  F7: 0x76,
  F8: 0x77,
  F9: 0x78,
  F10: 0x79,
  F11: 0x7a,
  F12: 0x7b,
};

const tokenToVKey = (token: string): number | null => {
  if (NAMED_VKEYS[token] !== undefined) return NAMED_VKEYS[token];
  if (/^Ux[0-9A-Fa-f]{4}$/.test(token)) return parseInt(token.slice(2), 16);
  if (token.length === 1) {
    const code = token.toUpperCase().charCodeAt(0);
    if ((code >= 0x41 && code <= 0x5a) || (code >= 0x30 && code <= 0x39))
      return code;
  }
  return null;
};

// "mods,vkey" -> AE command name, for O(1) clash lookups.
export const loadAeKeymap = (): Map<string, string> => {
  const map = new Map<string, string>();
  if (!window.cep) return map;
  let text: string;
  try {
    text = fs.readFileSync(KEYMAP_PATH(), "utf8");
  } catch {
    return map;
  }
  const lineRe = /^\s*"([^"]+)"\s*=\s*"\(([^)]+)\)"/;
  for (const line of text.split("\n")) {
    const m = lineRe.exec(line);
    if (!m) continue;
    const parts = m[2].split("+");
    const keyToken = parts.pop()!;
    const vkey = tokenToVKey(keyToken);
    if (vkey === null) continue;
    let mods = 0;
    for (const part of parts) mods |= MOD_BITS[part] ?? 0;
    map.set(`${mods},${vkey}`, m[1]);
  }
  return map;
};

export const findClash = (
  keymap: Map<string, string>,
  mods: number,
  vkey: number
): string | null => keymap.get(`${mods},${vkey}`) ?? null;

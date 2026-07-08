<script lang="ts">
  import { onMount } from "svelte";
  import { subscribeBackgroundColor, evalTS } from "../lib/utils/bolt";
  import {
    loadHotkeyTable,
    saveHotkeyTable,
    notifyHotkeysChanged,
    loadPollingEnabled,
    savePollingEnabled,
    type HotkeyBinding,
  } from "../lib/ws-server";
  import { loadAeKeymap, findClash } from "../lib/ae-keymap";
  import "../index.scss";
  import "./main.scss";

  let backgroundColor: string = $state("#282c34");
  let bindings: HotkeyBinding[] = $state([]);
  let listeningId: number | null = $state(null);
  let clashWarning: string | null = $state(null);
  let pollingEnabled: boolean = $state(true);
  let aeKeymap = loadAeKeymap();

  // Keys whose VK code doesn't equal their JS KeyboardEvent.key charCode -
  // letters and digits do (VK_A..Z = 0x41..5A, VK_0..9 = 0x30..39, same as
  // their uppercase ASCII), so only the rest need an explicit table.
  const SPECIAL_VKEYS: Record<string, number> = {
    ArrowLeft: 0x25,
    ArrowUp: 0x26,
    ArrowRight: 0x27,
    ArrowDown: 0x28,
    Escape: 0x1b,
    Tab: 0x09,
    Enter: 0x0d,
    Backspace: 0x08,
    Delete: 0x2e,
    " ": 0x20,
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
  const VKEY_LABELS: Record<number, string> = Object.fromEntries(
    Object.entries(SPECIAL_VKEYS).map(([k, v]) => [v, k === " " ? "Space" : k])
  );

  const keyToVKey = (key: string): number | null => {
    if (key.length === 1) {
      const code = key.toUpperCase().charCodeAt(0);
      if ((code >= 0x41 && code <= 0x5a) || (code >= 0x30 && code <= 0x39))
        return code;
      return null;
    }
    return SPECIAL_VKEYS[key] ?? null;
  };

  const vkeyLabel = (vkey: number) =>
    VKEY_LABELS[vkey] ?? String.fromCharCode(vkey);

  const shortcutLabel = (b: HotkeyBinding) => {
    const mods: string[] = [];
    if (b.mods & 1) mods.push("Ctrl");
    if (b.mods & 2) mods.push("Shift");
    if (b.mods & 4) mods.push("Alt");
    return [...mods, vkeyLabel(b.vkey)].join("+");
  };

  const onKeydown = (e: KeyboardEvent) => {
    if (listeningId === null) return;
    e.preventDefault();
    if (e.key === "Escape") {
      listeningId = null;
      return;
    }
    const vkey = keyToVKey(e.key);
    if (vkey === null) return; // unsupported key - keep listening
    const mods = (e.ctrlKey ? 1 : 0) | (e.shiftKey ? 2 : 0) | (e.altKey ? 4 : 0);
    bindings = bindings.map((b) =>
      b.id === listeningId ? { ...b, vkey, mods } : b
    );
    listeningId = null;
    saveHotkeyTable(bindings);
    notifyHotkeysChanged();

    const clash = findClash(aeKeymap, mods, vkey);
    clashWarning = clash ? `Clashes with After Effects' "${clash}"` : null;
  };

  onMount(() => {
    bindings = loadHotkeyTable();
    pollingEnabled = loadPollingEnabled();
    if (window.cep) subscribeBackgroundColor((c: string) => (backgroundColor = c));
  });

  const onPollingToggle = (e: Event) => {
    pollingEnabled = (e.target as HTMLInputElement).checked;
    savePollingEnabled(pollingEnabled);
  };
</script>

<svelte:window onkeydown={onKeydown} />

<div class="app" style="background-color: {backgroundColor};">
  <header class="app-header">
    <h2>Shortcuts</h2>
    <ul class="shortcut-list">
      {#each bindings as b (b.id)}
        <li>
          <span class="fn-name">{b.fn}</span>
          <button
            class:listening={listeningId === b.id}
            onclick={() => (listeningId = b.id)}
          >
            {listeningId === b.id ? "Press a key..." : shortcutLabel(b)}
          </button>
        </li>
      {/each}
    </ul>
    {#if clashWarning}
      <p class="clash-warning">{clashWarning}</p>
    {/if}
    <button
      class="ease-btn"
      onclick={(e) => {
        const mode = e.ctrlKey ? "in" : e.shiftKey ? "out" : "both";
        evalTS("applyEase", 70, mode, false);
      }}
    >
      Ease 70%
    </button>
    <p class="ease-hint">Ctrl = in only · Shift = out only</p>
    <label class="polling-toggle">
      <input type="checkbox" checked={pollingEnabled} onchange={onPollingToggle} />
      Live preview while dragging
    </label>
  </header>
</div>

<style lang="scss">
  @use "../variables.scss" as *;
  .app {
    padding: 10px;
    width: 100%;
    box-sizing: border-box;
    color: white;
    font-size: 0.85rem;
  }
  .app-header h2 {
    margin: 0 0 8px;
    font-size: 1rem;
  }
  .shortcut-list {
    list-style: none;
    padding: 0;
    margin: 0;
    li {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 6px 0;
    }
  }
  .listening {
    color: orange;
  }
  .clash-warning {
    color: #ff6b6b;
  }
  .ease-btn {
    margin-top: 12px;
    width: 100%;
    padding: 6px 0;
  }
  .ease-hint {
    margin: 4px 0 0;
    font-size: 0.7rem;
    color: $font;
  }
  .polling-toggle {
    display: flex;
    align-items: center;
    gap: 6px;
    margin-top: 8px;
    font-size: 0.75rem;
  }
</style>

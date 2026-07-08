import { CEP_Config } from "vite-cep-plugin";
import { version } from "./package.json";

const config: CEP_Config = {
  version,
  id: "barbar", 
  displayName: "BarBar", 
  symlink: "local",
  port: 3000,
  servePort: 5000,
  startingDebugPort: 8860,
  extensionManifestVersion: 6.0,
  requiredRuntimeVersion: 11.0,
  hosts: [
    { name: "AEFT", version: "[0.0,99.9]" }, 
  ],

  type: "Panel",
  iconDarkNormal: "./src/assets/light-icon.png",
  iconNormal: "./src/assets/dark-icon.png",
  iconDarkNormalRollOver: "./src/assets/light-icon.png",
  iconNormalRollOver: "./src/assets/dark-icon.png",
  parameters: ["--v=0", "--enable-nodejs", "--mixed-context"],
  width: 280,
  height: 240,

  panels: [
    {
      mainPath: "./main/index.html",
      name: "main",
      panelDisplayName: "BarBar",
      autoVisible: true,
      width: 280,
      height: 240,
    },
    {
      mainPath: "./main/index.html",
      name: "floating",
      panelDisplayName: "",
      autoVisible: true,
      type: "Custom",
      width: 300,
      height: 160,
    },
    {
      // No UI - just keeps the ws server (AEGP<->CEP) alive from AE launch,
      // independent of whether main/floating are open. floating.svelte closes
      // itself on blur, so it can't be relied on for this.
      //
      // CEP never auto-starts Custom-type extensions on its own - without
      // startOnEvents this panel simply never loads, and the AEGP gets no
      // hotkey table until the user happens to open the main panel. AE fires
      // applicationActivate at launch/first focus (both spellings kept for
      // older CEP versions), which is exactly when hotkeys start mattering -
      // they only fire while AE is foreground anyway.
      mainPath: "./main/index.html",
      name: "background",
      panelDisplayName: "",
      autoVisible: false,
      type: "Custom",
      startOnEvents: [
        "applicationActivate",
        "com.adobe.csxs.events.ApplicationActivate",
      ],
      width: 1,
      height: 1,
    },
  ],
  build: {
    jsxBin: "off",
    sourceMap: true,
  },
  zxp: {
    country: "US",
    province: "CA",
    org: "Company",
    password: "password",
    tsa: [
      "http://timestamp.digicert.com/", // Windows Only
      "http://timestamp.apple.com/ts01", // MacOS Only
    ],
    allowSkipTSA: false,
    sourceMap: false,
    jsxBin: "off",
  },
  installModules: ["ws"],
  copyAssets: [],
  copyZipAssets: [],
};
export default config;

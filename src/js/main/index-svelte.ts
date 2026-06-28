import App from "../app.svelte";
import { initBolt, csi } from "../lib/utils/bolt";
import { mount } from "svelte";

initBolt();

// the "background" panel only exists to keep the ws server alive from AE
// launch - it has no UI to mount.
if (!csi.getExtensionID()?.includes("background")) {
  mount(App, {
    target: document.getElementById("app")!,
  });
}

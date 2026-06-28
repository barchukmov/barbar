<script lang="ts">
  import { onMount } from "svelte";
  import { csi, subscribeBackgroundColor } from "../lib/utils/bolt";
  import "../index.scss";

  let backgroundColor: string = $state("#282c34");

  onMount(() => {
    if (window.cep) {
      subscribeBackgroundColor((c: string) => (backgroundColor = c));
      // Delay so the window's own open doesn't immediately trigger blur
      window.addEventListener("focus", () => {
        const raw = localStorage.getItem("floatingPos");
        if (raw) { const { x, y } = JSON.parse(raw); window.moveTo(x, y); }
      });
      setTimeout(() => window.addEventListener("blur", () => csi.closeExtension()), 300);
    }
  });
</script>

<div class="app" style="background-color: {backgroundColor};">
  <p>Floating panel</p>
</div>

<style lang="scss">
  @use "../variables.scss" as *;
  .app {
    padding: 16px;
    height: 100vh;
    box-sizing: border-box;
    color: white;
  }
</style>

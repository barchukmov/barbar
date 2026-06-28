<script lang="ts">
  import Router from "svelte-spa-router";
  import Main from "./main/main.svelte";
  import Floating from "./floating/floating.svelte";
  import { csi } from "./lib/utils/bolt";

  const routes = {
    "/": Main,
    "/floating": Floating,
  };

  // csi.getExtensionID() wraps __adobe_cep__.getExtensionId() (lowercase d)
  const id: string = window.__adobe_cep__ ? csi.getExtensionID() ?? "" : "";
  if (id.includes("floating")) location.hash = "#/floating";
</script>

<Router {routes} />

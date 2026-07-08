#include "AegpDemo.h"
#include <new>
#ifdef AE_OS_WIN
#include "HotkeyOverlay.h"
#include "WsClient.h"
#include <windows.h>
#endif

// This .aex has no After Effects-side UI - the panel the user interacts with
// is the CEP extension. The plugin exists only to host the global-hotkey
// overlay and the WebSocket client that bridges the popup to CEP. So its
// entire job is to spin those up when AE loads the plugin and tear them down
// on shutdown; it registers a death hook and nothing else (no menu command,
// no panel).
class BarbarPlugin {
public:
  BarbarPlugin(SPBasicSuite *pica_basicP, AEGP_PluginID pluginID)
      : i_sp(pica_basicP), i_pluginID(pluginID) {
    // The death hook is the clean-shutdown signal that lets us join the
    // hotkey thread and stop the socket before AE unloads the DLL.
    PT_ETX(i_sp.RegisterSuite5()->AEGP_RegisterDeathHook(
        i_pluginID, &BarbarPlugin::S_DeathHook, NULL));

#ifdef AE_OS_WIN
    StartHotkeyOverlay();
    StartWsClient("ws://127.0.0.1:41420", [](const std::string &msg) {
      OutputDebugStringA(("[ws] " + msg + "\n").c_str());
      // The ping carries no table - the hotkey file is the source of truth
      // and the overlay re-reads it (see ReloadHotkeyTableFromFile).
      if (WsJsonGetString(msg, "type") == "hotkeysChanged") {
        ReloadHotkeyTableFromFile();
      }
    });
#endif
  }

private:
  AEGP_SuiteHandler i_sp;
  AEGP_PluginID i_pluginID;

  void DeathHook() {
#ifdef AE_OS_WIN
    StopHotkeyOverlay();
    StopWsClient();
#endif
  }

  static A_Err S_DeathHook(AEGP_GlobalRefcon plugin_refconP,
                           AEGP_DeathRefcon refconP) {
    PT_XTE_START {
      reinterpret_cast<BarbarPlugin *>(plugin_refconP)->DeathHook();
    }
    PT_XTE_CATCH_RETURN_ERR;
  }
};

A_Err EntryPointFunc(struct SPBasicSuite *pica_basicP,  /* >> */
                     A_long major_versionL,             /* >> */
                     A_long minor_versionL,             /* >> */
                     AEGP_PluginID aegp_plugin_id,      /* >> */
                     AEGP_GlobalRefcon *global_refconP) /* << */
{
  PT_XTE_START {
    *global_refconP =
        (AEGP_GlobalRefcon) new BarbarPlugin(pica_basicP, aegp_plugin_id);
  }
  PT_XTE_CATCH_RETURN_ERR;
}

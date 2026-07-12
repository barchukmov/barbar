#include "AegpDemo.h"
#include <new>
#ifdef AE_OS_WIN
#include "HotkeyOverlay.h"
#include "ScriptRunner.h"
#endif

// This .aex has no After Effects-side UI - the panel the user interacts with
// is the CEP extension, and it's purely a settings editor. The plugin's
// entire job is the global-hotkey overlay and running the resulting
// ExtendScript calls on AE's main thread (ScriptRunner); it spins those up
// when AE loads the plugin, tears them down on shutdown, and registers a
// death hook and nothing else (no menu command, no panel).
class BarbarPlugin {
public:
  BarbarPlugin(SPBasicSuite *pica_basicP, AEGP_PluginID pluginID)
      : i_sp(pica_basicP), i_pluginID(pluginID) {
    // The death hook is the clean-shutdown signal that lets us join the
    // hotkey thread before AE unloads the DLL.
    PT_ETX(i_sp.RegisterSuite5()->AEGP_RegisterDeathHook(
        i_pluginID, &BarbarPlugin::S_DeathHook, NULL));

#ifdef AE_OS_WIN
    // ScriptRunner first: it must save off suite pointers and register its
    // idle hook on this (the main) thread before the overlay thread exists
    // and a hotkey can fire a jsx call at it.
    StartScriptRunner(pica_basicP, pluginID);
    StartHotkeyOverlay();
#endif
  }

private:
  AEGP_SuiteHandler i_sp;
  AEGP_PluginID i_pluginID;

  void DeathHook() {
#ifdef AE_OS_WIN
    StopHotkeyOverlay();
    StopScriptRunner();
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

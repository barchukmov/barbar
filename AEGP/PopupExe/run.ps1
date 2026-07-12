param(
  # argv passed straight through to popup_main.cpp: mouseX mouseY screenW screenH [toast|recreate]
  [Parameter(ValueFromRemainingArguments = $true)]
  [string[]]$PopupArgs = @()
)

$ErrorActionPreference = 'Stop'

# Everything is anchored to this script's own directory, so it can be run
# from anywhere (e.g. the repo root), not just from inside PopupExe/.
$here = $PSScriptRoot

# Debug-only: the CMakeLists links a hardcoded Vendor/raylib/lib/Debug/raylib.lib, not a $<CONFIG>-relative path.
Write-Host "== Configure/build raylib_popup (standalone popup UI runner) =="
cmake -S $here -B "$here\build" | Out-Null
cmake --build "$here\build" --config Debug | Out-Null

& "$here\build\Debug\raylib_popup.exe" @PopupArgs

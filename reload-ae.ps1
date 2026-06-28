$ErrorActionPreference = 'Stop'

# CloseMainWindow asks the app to quit through its own message loop (same as
# clicking the title-bar X), giving it a chance to release file locks on
# AegpDemo.aex cleanly - a hard Stop-Process can leave Premiere/AE holding
# the handle a moment longer, which is what broke the last build (LNK1168).
function Close-Gracefully($processName) {
  $procs = Get-Process $processName -ErrorAction SilentlyContinue
  if (-not $procs) { return }
  $procs | ForEach-Object { $_.CloseMainWindow() | Out-Null }
  $procs | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
  # Still running after 15s (e.g. blocked on its own "Save changes?" prompt) - fall back to a hard kill.
  Get-Process $processName -ErrorAction SilentlyContinue | Stop-Process -Force
}

Write-Host "== Closing After Effects and Premiere Pro =="
Close-Gracefully "AfterFX"
Close-Gracefully "Adobe Premiere Pro"
Start-Sleep -Milliseconds 500

Write-Host "== Building =="
pnpm build

# Best-effort only: AE's prefs file wraps/hex-encodes non-ASCII or long paths
# across multiple lines, which a single-line regex can't reconstruct. If the
# most recent entry isn't a plain single-line path, skip it rather than risk
# launching the wrong project.
$prefs = Get-ChildItem "$env:APPDATA\Adobe\After Effects" -Filter "* Prefs.txt" -Recurse -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending | Select-Object -First 1

$projectPath = $null
if ($prefs) {
  $match = Select-String -Path $prefs.FullName -Pattern '"MRU Project Path ID # 0, File Path" = "([^"]+)"' | Select-Object -First 1
  if ($match) {
    $candidate = $match.Matches[0].Groups[1].Value
    if (Test-Path -LiteralPath $candidate) { $projectPath = $candidate }
  }
}

$aeExe = "C:\Program Files\Adobe\Adobe After Effects 2026\Support Files\AfterFX.exe"
if ($projectPath) {
  Write-Host "== Launching After Effects with $projectPath =="
  Start-Process $aeExe -ArgumentList "`"$projectPath`""
} else {
  Write-Host "== Could not resolve last opened project (non-ASCII/long path or none found) - launching After Effects without one =="
  Start-Process $aeExe
}

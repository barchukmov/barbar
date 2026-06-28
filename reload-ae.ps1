$ErrorActionPreference = 'Stop'

Write-Host "== Killing After Effects =="
Get-Process AfterFX -ErrorAction SilentlyContinue | Stop-Process -Force
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

param(
  [ValidateSet('Debug', 'Release')]
  [string]$Configuration = 'Debug',
  [ValidateSet('x64', 'ARM64')]
  [string]$Platform = 'x64',
  [string]$MSBuildExe = $(if ($env:MSBUILD_EXE) { $env:MSBUILD_EXE } else { 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' }),
  [string]$Vcxproj = 'Win\AegpDemo.vcxproj',
  # same plugin folder Composition Groups 02 installs into
  [string]$OutDir = 'C:\Program Files\Adobe\Adobe After Effects 2026\Support Files\Plug-ins\AEGP\',
  [string]$IntDir = "dist\obj\$Platform\$Configuration\"
)

$ErrorActionPreference = 'Stop'

Write-Host "== Vendor raylib (for the ctrl+h overlay) =="
$overlayBuild = 'HotkeyOverlay\build'
cmake -S HotkeyOverlay -B $overlayBuild | Out-Null
cmake --build $overlayBuild --config $Configuration | Out-Null

$raylibSrc = Get-ChildItem "$overlayBuild\_deps\raylib-src\src" -Filter '*.h' | Select-Object -ExpandProperty FullName
$raylibLib = "$overlayBuild\_deps\raylib-build\raylib\$Configuration\raylib.lib"
$rayguiH = "$overlayBuild\_deps\raygui-src\src\raygui.h"

New-Item -ItemType Directory -Force 'Vendor\raylib\include' | Out-Null
New-Item -ItemType Directory -Force "Vendor\raylib\lib\$Configuration" | Out-Null
Copy-Item -Force $raylibSrc 'Vendor\raylib\include\'
Copy-Item -Force $rayguiH 'Vendor\raylib\include\'
Copy-Item -Force $raylibLib "Vendor\raylib\lib\$Configuration\raylib.lib"

Write-Host "== Build AegpDemo.vcxproj =="
& "$MSBuildExe" $Vcxproj "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:OutDir=$OutDir" "/p:IntDir=$IntDir"

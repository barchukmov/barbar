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

New-Item -ItemType Directory -Force 'Vendor\raylib\include' | Out-Null
New-Item -ItemType Directory -Force "Vendor\raylib\lib\$Configuration" | Out-Null
Copy-Item -Force $raylibSrc 'Vendor\raylib\include\'
Copy-Item -Force $raylibLib "Vendor\raylib\lib\$Configuration\raylib.lib"

Write-Host "== Vendor IXWebSocket (CEP<->AEGP transport) =="
$ixwsInclude = "$overlayBuild\_deps\ixwebsocket-src\ixwebsocket"
$ixwsLib = "$overlayBuild\_deps\ixwebsocket-build\$Configuration\ixwebsocket.lib"

New-Item -ItemType Directory -Force 'Vendor\IXWebSocket\include\ixwebsocket' | Out-Null
New-Item -ItemType Directory -Force "Vendor\IXWebSocket\lib\$Configuration" | Out-Null
Copy-Item -Force "$ixwsInclude\*.h" 'Vendor\IXWebSocket\include\ixwebsocket\'
Copy-Item -Force $ixwsLib "Vendor\IXWebSocket\lib\$Configuration\ixwebsocket.lib"

Write-Host "== Ship popup font + icons next to the .aex =="
New-Item -ItemType Directory -Force "$OutDir\Fonts" | Out-Null
Copy-Item -Force 'Resources\Fonts\Mannin-Regular.otf' "$OutDir\Fonts\"
# The Ubuntu Font Licence requires its text to accompany the font wherever
# the font is distributed.
Copy-Item -Force 'Resources\Fonts\LICENCE_UFL.txt' "$OutDir\Fonts\"
New-Item -ItemType Directory -Force "$OutDir\Icons" | Out-Null
Copy-Item -Force 'Resources\Icons\*.svg' "$OutDir\Icons\"

Write-Host "== Build AegpDemo.vcxproj =="
# a single trailing backslash escapes the closing quote in MSBuild's argv parsing
# and swallows the next argument - double it so the quote closes correctly.
$outDirArg = $OutDir -replace '\\$', '\\'
$intDirArg = $IntDir -replace '\\$', '\\'
& "$MSBuildExe" $Vcxproj "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:OutDir=$outDirArg" "/p:IntDir=$intDirArg"

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

Write-Host "== Patch raygui.h: add forceDragging param to GuiSliderPro =="
# GuiSlider()/GuiSliderBar() only let a slider track the mouse while the
# button is held - raygui has no flag for "always follow the cursor". This
# adds one. Patched by line search (not full-file replace) because the
# dragging-update code block this touches is duplicated verbatim in three
# other controls (scrollbar/spinner/listview) further down the file.
$rayguiVendored = 'Vendor\raylib\include\raygui.h'
$rayguiLines = [Collections.Generic.List[string]](Get-Content $rayguiVendored)

$sigIndex = $rayguiLines.FindIndex({ param($l) $l -eq 'int GuiSliderPro(Rectangle bounds, const char *textLeft, const char *textRight, float *value, float minValue, float maxValue, int sliderWidth)' })
if ($sigIndex -lt 0) { throw 'raygui.h patch: GuiSliderPro signature not found - upstream layout changed' }
$rayguiLines[$sigIndex] = $rayguiLines[$sigIndex] -replace '\)$', ', bool forceDragging)'

$dragIndex = $rayguiLines.FindIndex({ param($l) $l -eq '        if (guiSliderDragging) // Keep dragging outside of bounds' })
if ($dragIndex -lt 0) { throw 'raygui.h patch: slider dragging block not found - upstream layout changed' }
$rayguiLines[$dragIndex] = '        if (forceDragging) { state = STATE_PRESSED; *value = ((maxValue - minValue)*(mousePoint.x - (float)(bounds.x + sliderWidth/2)))/(float)(bounds.width - sliderWidth) + minValue; }' + "`n" + '        else if (guiSliderDragging) // Keep dragging outside of bounds'

$sliderCallIndex = $rayguiLines.FindIndex({ param($l) $l -eq '    return GuiSliderPro(bounds, textLeft, textRight, value, minValue, maxValue, GuiGetStyle(SLIDER, SLIDER_WIDTH));' })
if ($sliderCallIndex -lt 0) { throw 'raygui.h patch: GuiSlider() call site not found - upstream layout changed' }
$rayguiLines[$sliderCallIndex] = $rayguiLines[$sliderCallIndex] -replace '\);$', ', false);'

$sliderBarCallIndex = $rayguiLines.FindIndex({ param($l) $l -eq '    return GuiSliderPro(bounds, textLeft, textRight, value, minValue, maxValue, 0);' })
if ($sliderBarCallIndex -lt 0) { throw 'raygui.h patch: GuiSliderBar() call site not found - upstream layout changed' }
$rayguiLines[$sliderBarCallIndex] = $rayguiLines[$sliderBarCallIndex] -replace '\);$', ', false);'

Set-Content -Path $rayguiVendored -Value $rayguiLines

Write-Host "== Vendor IXWebSocket (CEP<->AEGP transport) =="
$ixwsInclude = "$overlayBuild\_deps\ixwebsocket-src\ixwebsocket"
$ixwsLib = "$overlayBuild\_deps\ixwebsocket-build\$Configuration\ixwebsocket.lib"

New-Item -ItemType Directory -Force 'Vendor\IXWebSocket\include\ixwebsocket' | Out-Null
New-Item -ItemType Directory -Force "Vendor\IXWebSocket\lib\$Configuration" | Out-Null
Copy-Item -Force "$ixwsInclude\*.h" 'Vendor\IXWebSocket\include\ixwebsocket\'
Copy-Item -Force $ixwsLib "Vendor\IXWebSocket\lib\$Configuration\ixwebsocket.lib"

Write-Host "== Build AegpDemo.vcxproj =="
# a single trailing backslash escapes the closing quote in MSBuild's argv parsing
# and swallows the next argument - double it so the quote closes correctly.
$outDirArg = $OutDir -replace '\\$', '\\'
$intDirArg = $IntDir -replace '\\$', '\\'
& "$MSBuildExe" $Vcxproj "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:OutDir=$outDirArg" "/p:IntDir=$intDirArg"

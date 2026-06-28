cmake_minimum_required(VERSION 3.20)
project(raylib_popup)

# Compiles the real popup UI (HotkeyOverlay_Popup.cpp - the same file the
# .aex builds) plus a stub WS layer, so the UI can be iterated on by running
# this exe instead of rebuilding the .aex and reloading it inside AE.
add_executable(raylib_popup
	popup_main.cpp
	StubWs.cpp
	../FontPath.cpp
	../HotkeyOverlay_Popup.cpp)
target_include_directories(raylib_popup PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../Vendor/raylib/include)
target_link_libraries(raylib_popup PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../Vendor/raylib/lib/Debug/raylib.lib
	winmm opengl32 gdi32 user32 shell32)

# GetFontPath() resolves relative to the exe's own folder, same layout build.ps1
# gives the .aex - keep the test exe's Fonts/ folder in step with it.
add_custom_command(TARGET raylib_popup POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:raylib_popup>/Fonts"
	COMMAND ${CMAKE_COMMAND} -E copy
		${CMAKE_CURRENT_SOURCE_DIR}/../Resources/Fonts/Mannin-Regular.otf
		"$<TARGET_FILE_DIR:raylib_popup>/Fonts/")

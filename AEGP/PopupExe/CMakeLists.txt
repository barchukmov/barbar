cmake_minimum_required(VERSION 3.20)
project(raylib_popup)

# Links the already-vendored+patched raylib (Vendor/raylib, produced by build.ps1)
# so the exe and the old in-process build share the exact same raygui slider patch.
add_executable(raylib_popup popup_main.cpp)
target_include_directories(raylib_popup PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Vendor/raylib/include)
target_link_libraries(raylib_popup PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../Vendor/raylib/lib/Debug/raylib.lib
	winmm opengl32 gdi32 user32 shell32)

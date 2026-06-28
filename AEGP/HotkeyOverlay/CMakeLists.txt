cmake_minimum_required(VERSION 3.20)
project(hotkey_overlay_deps)

# Vendors raylib (+ raygui, header-only) for AegpDemo.vcxproj to link against
# directly - the overlay now runs as a thread inside the .aex, not its own exe.
include(FetchContent)
FetchContent_Declare(raylib GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG 5.5)
FetchContent_Declare(raygui GIT_REPOSITORY https://github.com/raysan5/raygui.git GIT_TAG 4.0)
FetchContent_MakeAvailable(raylib raygui)

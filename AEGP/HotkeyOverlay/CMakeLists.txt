cmake_minimum_required(VERSION 3.20)
project(hotkey_overlay_deps)

# Vendors raylib for AegpDemo.vcxproj to link against directly - the overlay
# runs as a thread inside the .aex, not its own exe.
include(FetchContent)
FetchContent_Declare(raylib GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG 6.0)
FetchContent_MakeAvailable(raylib)

# WebSocket client to CEP's ws server. Loopback-only, so TLS is off - no OpenSSL dependency.
set(USE_TLS OFF CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ixwebsocket GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git GIT_TAG v11.4.6)
FetchContent_MakeAvailable(ixwebsocket)

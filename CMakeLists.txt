cmake_minimum_required(VERSION 3.20)
project(caustic VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(CAUSTIC_BUILD_TESTS "Build the doctest harness" ON)
option(CAUSTIC_BUILD_APP "Build the GUI executable" ON)
option(CAUSTIC_BUILD_CLI "Build the headless CLI" ON)
option(CAUSTIC_USE_SYSTEM_RAYLIB "Use system-installed raylib instead of FetchContent" OFF)

include(FetchContent)

# nlohmann/json (header-only). Pulled at root so any subproject can depend on
# it without each declaring its own copy. First consumer is Phase 5 preset I/O.
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

if(CAUSTIC_BUILD_TESTS)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
    )
    FetchContent_MakeAvailable(doctest)
    enable_testing()
endif()

if(CAUSTIC_BUILD_APP)
    if(CAUSTIC_USE_SYSTEM_RAYLIB)
        find_package(raylib REQUIRED)
    else()
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG 6.0
        )
        FetchContent_MakeAvailable(raylib)
    endif()

    # Dear ImGui ships no top-level CMakeLists.txt upstream; we build our own
    # target from its source files. Pinned to v1.92.7 because rlImGui's
    # Raylib_6_0 tag uses the v1.92 texture-manager APIs (GetPlatformIO,
    # ImTextureData, ImGuiBackendFlags_RendererHasTextures).
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.7
    )
    FetchContent_MakeAvailable(imgui)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    )
    target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})

    # rlImGui pinned to its Raylib_6_0 tag — rlImGui ships tagged versions
    # that name the raylib release they target. CLAUDE.md flags this as a
    # recurring pitfall: rlImGui and raylib must be bumped in lockstep.
    FetchContent_Declare(
        rlimgui
        GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
        GIT_TAG Raylib_6_0
    )
    FetchContent_GetProperties(rlimgui)
    if(NOT rlimgui_POPULATED)
        FetchContent_Populate(rlimgui)
    endif()
endif()

add_subdirectory(core)
add_subdirectory(render)
if(CAUSTIC_BUILD_APP)
    add_subdirectory(app)
endif()
if(CAUSTIC_BUILD_CLI)
    add_subdirectory(cli)
endif()
if(CAUSTIC_BUILD_TESTS)
    add_subdirectory(tests)
endif()

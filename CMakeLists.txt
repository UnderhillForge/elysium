cmake_minimum_required(VERSION 3.24)

project(ElysiumEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ELYSIUM_ENABLE_WARNINGS "Enable strict warnings" ON)

include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# ----------------------------
# Dependencies
# ----------------------------

# SDL2
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    SDL2
    URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.30.10.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(SDL2)

# GLM
FetchContent_Declare(
    glm
    URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(glm)

# spdlog
FetchContent_Declare(
    spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(spdlog)

# tinygltf (header-only)
FetchContent_Declare(
    tinygltf
    URL https://github.com/syoyo/tinygltf/archive/refs/tags/v2.9.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(tinygltf)

# Dear ImGui (docking branch)
FetchContent_Declare(
    imgui
    URL https://github.com/ocornut/imgui/archive/refs/heads/docking.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(imgui)

# Lua 5.4
FetchContent_Declare(
    lua
    URL https://github.com/lua/lua/archive/refs/tags/v5.4.7.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(lua)

# sol2 (Lua binding)
FetchContent_Declare(
    sol2
    URL https://github.com/ThePhD/sol2/archive/refs/tags/v3.3.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(sol2)

# Jolt Physics
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(TARGET_TOOLS OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    jolt
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.2.0
    SOURCE_SUBDIR Build
)
FetchContent_MakeAvailable(jolt)

# glad2 (OpenGL loader)
FetchContent_Declare(
    glad
    URL https://github.com/Dav1dde/glad/archive/refs/tags/v0.1.36.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_GetProperties(glad)
if(NOT glad_POPULATED)
    FetchContent_Populate(glad)
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(GLAD_GEN_DIR ${CMAKE_BINARY_DIR}/gladsources/glad_gl_core_45)

add_custom_command(
    OUTPUT
        ${GLAD_GEN_DIR}/src/glad.c
        ${GLAD_GEN_DIR}/include/glad/glad.h
        ${GLAD_GEN_DIR}/include/KHR/khrplatform.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GLAD_GEN_DIR}
    COMMAND ${CMAKE_COMMAND} -E env
            PYTHONPATH=${glad_SOURCE_DIR}
            ${Python3_EXECUTABLE} -m glad
            --out-path ${GLAD_GEN_DIR}
            --api gl=4.5
            --profile core
            --generator c
            --reproducible
    DEPENDS ${glad_SOURCE_DIR}/glad/__main__.py
    COMMENT "Generating glad OpenGL loader sources"
    VERBATIM
)

add_library(glad_gl_core_45 STATIC ${GLAD_GEN_DIR}/src/glad.c)
target_include_directories(glad_gl_core_45 PUBLIC ${GLAD_GEN_DIR}/include)

# tinygltf interface target
add_library(tinygltf_headers INTERFACE)
target_include_directories(tinygltf_headers INTERFACE ${tinygltf_SOURCE_DIR})

# ImGui target
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(lua54 STATIC
    ${lua_SOURCE_DIR}/lapi.c
    ${lua_SOURCE_DIR}/lauxlib.c
    ${lua_SOURCE_DIR}/lbaselib.c
    ${lua_SOURCE_DIR}/lcode.c
    ${lua_SOURCE_DIR}/lcorolib.c
    ${lua_SOURCE_DIR}/lctype.c
    ${lua_SOURCE_DIR}/ldblib.c
    ${lua_SOURCE_DIR}/ldebug.c
    ${lua_SOURCE_DIR}/ldo.c
    ${lua_SOURCE_DIR}/ldump.c
    ${lua_SOURCE_DIR}/lfunc.c
    ${lua_SOURCE_DIR}/lgc.c
    ${lua_SOURCE_DIR}/linit.c
    ${lua_SOURCE_DIR}/liolib.c
    ${lua_SOURCE_DIR}/llex.c
    ${lua_SOURCE_DIR}/lmathlib.c
    ${lua_SOURCE_DIR}/lmem.c
    ${lua_SOURCE_DIR}/loadlib.c
    ${lua_SOURCE_DIR}/lobject.c
    ${lua_SOURCE_DIR}/lopcodes.c
    ${lua_SOURCE_DIR}/loslib.c
    ${lua_SOURCE_DIR}/lparser.c
    ${lua_SOURCE_DIR}/lstate.c
    ${lua_SOURCE_DIR}/lstring.c
    ${lua_SOURCE_DIR}/lstrlib.c
    ${lua_SOURCE_DIR}/ltable.c
    ${lua_SOURCE_DIR}/ltablib.c
    ${lua_SOURCE_DIR}/ltm.c
    ${lua_SOURCE_DIR}/lundump.c
    ${lua_SOURCE_DIR}/lutf8lib.c
    ${lua_SOURCE_DIR}/lvm.c
    ${lua_SOURCE_DIR}/lzio.c
)
target_include_directories(lua54 PUBLIC ${lua_SOURCE_DIR})

add_library(sol2_headers INTERFACE)
target_include_directories(sol2_headers INTERFACE ${sol2_SOURCE_DIR}/include)

target_include_directories(imgui_lib
    PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_lib
    PUBLIC
        SDL2::SDL2-static
)

# ImGui OpenGL backend uses custom loader include path.
# ----------------------------
# Engine target
# ----------------------------
add_executable(elysium
    src/main.cpp
    src/Application.cpp
    src/Assets/AssetCatalog.cpp
    src/Foundation/SubsystemManager.cpp
    src/Renderer/Camera.cpp
    src/Renderer/Shader.cpp
    src/Editor/EditorViewport.cpp
    src/Assets/GLTFLoader.cpp
    src/Scene/Scene.cpp
    src/Tile/TileMap.cpp
    src/Walkmesh/Walkmesh.cpp
    src/Walkmesh/CharacterController.cpp
    src/Scripting/ScriptSystem.cpp
    src/Scripting/ScriptRuntimeSubsystem.cpp
    src/Physics/PhysicsSystem.cpp
    src/Physics/PhysicsRuntimeSubsystem.cpp
    src/Audio/SpatialAudioSystem.cpp
    src/Networking/NetworkingSystem.cpp
    src/Packaging/ResourcePackager.cpp
    src/Procedural/ProceduralTileTypes.cpp
    src/Procedural/ProceduralTileGenerator.cpp
    src/Procedural/TileSetGenerator.cpp
    src/Particles/ParticleSystem.cpp
)

target_include_directories(elysium PRIVATE
    src
    ${tinygltf_SOURCE_DIR}
)

target_link_libraries(elysium PRIVATE
    SDL2::SDL2-static
    imgui_lib
    glad_gl_core_45
    glm
    spdlog::spdlog
    tinygltf_headers
    lua54
    sol2_headers
    Jolt
)

if(APPLE)
    target_link_libraries(elysium PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(elysium PRIVATE OpenGL::GL)
endif()

if(MSVC)
    target_compile_definitions(elysium PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(ELYSIUM_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(elysium PRIVATE /W4 /permissive-)
    else()
        target_compile_options(elysium PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endif()

# Use source groups for IDEs.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
        src/Particles/ParticleSystem.cpp
    src/Foundation/SubsystemManager.cpp
    src/Scripting/ScriptRuntimeSubsystem.cpp
    src/Physics/PhysicsRuntimeSubsystem.cpp
    src/Audio/SpatialAudioSystem.cpp
    src/Networking/NetworkingSystem.cpp
    src/Packaging/ResourcePackager.cpp
    src/Procedural/ProceduralTileTypes.cpp
    src/Procedural/ProceduralTileGenerator.cpp
    src/Procedural/TileSetGenerator.cpp
    src/main.cpp
    src/Application.cpp
    src/Renderer/Camera.cpp
    src/Renderer/Shader.cpp
    src/Editor/EditorViewport.cpp
    src/Assets/GLTFLoader.cpp
    src/Scene/Scene.cpp
)

add_custom_target(pack_resources_client
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/scripts/package_resources.py
            --input ${CMAKE_SOURCE_DIR}/assets
            --output ${CMAKE_BINARY_DIR}/packaged_assets/client
            --manifest ${CMAKE_BINARY_DIR}/packaged_assets/client/manifest.txt
            --profile client
    COMMENT "Packaging client runtime assets (glTF-first)"
    VERBATIM
)

add_custom_target(pack_resources_server
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/scripts/package_resources.py
            --input ${CMAKE_SOURCE_DIR}/assets
            --output ${CMAKE_BINARY_DIR}/packaged_assets/server
            --manifest ${CMAKE_BINARY_DIR}/packaged_assets/server/manifest.txt
            --profile server
    COMMENT "Packaging headless server assets"
    VERBATIM
)

add_custom_target(pack_resources_editor
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/scripts/package_resources.py
            --input ${CMAKE_SOURCE_DIR}/assets
            --output ${CMAKE_BINARY_DIR}/packaged_assets/editor
            --manifest ${CMAKE_BINARY_DIR}/packaged_assets/editor/manifest.txt
            --profile editor
    COMMENT "Packaging full editor asset bundle (includes source formats)"
    VERBATIM
)

add_custom_target(pack_resources
    DEPENDS pack_resources_client
    COMMENT "Alias: package client runtime assets"
    VERBATIM
)

add_custom_target(smoke_startup
    COMMAND ${CMAKE_COMMAND} -E env
            RUN_SECONDS=6
            MAX_GLTF_LINES=5
            MAX_WALKMESH_LINES=3
            ${CMAKE_SOURCE_DIR}/scripts/smoke_startup.sh
            ${CMAKE_BINARY_DIR}/elysium
            ${CMAKE_BINARY_DIR}/elysium_smoke.log
    DEPENDS elysium
    COMMENT "Run startup smoke check (load/rebuild log guard)"
    VERBATIM
)

add_custom_target(smoke_startup_gdb
    COMMAND ${CMAKE_COMMAND} -E env
            RUN_SECONDS=20
            ${CMAKE_SOURCE_DIR}/scripts/smoke_startup_gdb.sh
            ${CMAKE_BINARY_DIR}/elysium
            ${CMAKE_BINARY_DIR}/elysium_crash_bt.log
    DEPENDS elysium
    COMMENT "Run startup under gdb and fail only on crash signals"
    VERBATIM
)

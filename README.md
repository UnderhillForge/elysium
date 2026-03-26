# Elysium Engine

A lightweight, glTF-first 3D world builder and VTT engine prototype.

## Current Prototype Features

- SDL2 window + OpenGL 4.5 core context
- Dear ImGui editor shell with docking and multi-viewport enabled
- Editor panels: Viewport, Assets Browser, Outliner, Properties, Console
- Basic glTF (`.gltf` / `.glb`) loading via tinygltf
- Scene graph with transform + tint per instance
- Viewport controls:
  - RMB + WASD/E/Q fly camera
  - Alt + LMB orbit
  - MMB pan
  - Mouse wheel dolly
- Placeholder tile workflow:
  - World grid render
  - Grid snapping for new placements and selected node
  - Procedural area authoring with preview/regenerate/apply/clear-applied flow
- Drag-and-drop workflows:
  - Drag glTF files from `Assets Browser` into viewport
  - Drop `.gltf/.glb` files from OS into app window to instantiate
  - Drop a folder from OS into app window to set `Assets Browser` root
- Procedural controls:
  - `Viewport -> Procedural Area` panel for seed, size, origin, water/beach levels, transition, jitter, and biome/noise tuning
  - `Generate Preview` creates a non-destructive overlay before commit
  - `Apply` commits preview tiles to the tilemap
  - `Clear Applied` restores the tilemap state from before the last procedural apply
  - Shortcuts: `Ctrl+G` generate preview, `Ctrl+Shift+G` apply preview, `Ctrl+Alt+G` clear last applied procedural area

## Requirements

- CMake 3.24+
- C++20 compiler
  - Linux: GCC 12+ or Clang 15+
  - Windows: MSVC 2022+
  - macOS: Apple Clang 15+
- Git
- OpenGL 4.5-capable GPU/driver

## Quick Setup

### Linux

Install core tools (example for Debian/Ubuntu):

```bash
sudo apt update
sudo apt install -y build-essential cmake git ninja-build pkg-config unzip \
  libgl1-mesa-dev libglu1-mesa-dev \
  libx11-dev libxext-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev libxfixes-dev
```

### macOS

Install Homebrew, then:

```bash
brew install cmake git ninja
xcode-select --install
```

### Windows

Install:

- Visual Studio 2022 with Desktop C++ workload
- CMake (or use VS bundled CMake)
- Git

Optional: use PowerShell bootstrap script in `scripts/bootstrap.ps1`.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

Run:

```bash
./build/elysium
```

Start with a model:

```bash
./build/elysium /absolute/path/to/asset.glb
```

## Scripts

- `scripts/bootstrap.sh`: install basic Linux/macOS build tools
- `scripts/bootstrap.ps1`: install basic Windows tools with `winget`
- `scripts/smoke_startup.sh`: launch-time sanity check (guards glTF load flood, walkmesh rebuild spam, and tinygltf startup errors)
- `scripts/smoke_startup_gdb.sh`: gdb-based startup run that fails only when a crash signal is detected and writes a backtrace log

Run smoke startup check:

```bash
cmake --build build --target smoke_startup
```

Run gdb startup smoke check:

```bash
cmake --build build --target smoke_startup_gdb
```

## Notes

Dependencies are pulled with CMake `FetchContent` during configure/build.

This is a Phase 1 prototype scaffold and intentionally keeps architecture lightweight for future streaming, scripting, and dedicated-server/runtime split work.

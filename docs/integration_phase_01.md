# Elysium Integration Phase 01

This phase intentionally adapts patterns (not code) from reference engines while keeping Elysium lightweight, ImGui-centric, and glTF-first.

## 1) Viewport Rendering and Debug Improvements

Inspired by Enigine: editor-facing material/debug visualization in the viewport.
- Added material debug views in `EditorViewport` shader path:
  - Lit
  - Normals
  - Roughness
  - Metallic
  - Emissive

Inspired by Enigine: practical post-lighting controls for lookdev.
- Added tone mapping toggle and exposure/gamma controls:
  - ACES-like tonemap curve (approximation)
  - Display gamma correction after shading

Why this fits Elysium:
- Keeps rendering pipeline simple (single forward path, no deferred rewrite).
- Improves art iteration speed directly in the ImGui viewport.
- No new heavy dependencies.

## 2) Foundation Subsystem Architecture

Inspired by RavEngine: subsystem lifecycle orchestration and explicit update phases.
- Introduced:
  - `Subsystem` interface (`initialize`, `shutdown`, `preUpdate`, `update`, `postUpdate`)
  - `SubsystemManager` with ordered lifecycle and reverse shutdown

Why this fits Elysium:
- Preserves the existing `Application`/`SceneEntity` model.
- Provides clean insertion points for Lua, Jolt, Audio, Networking.
- Creates a migration path for headless server mode (selective subsystem startup).

## 3) Upcoming Systems Design (Phase-1 Skeletons)

Inspired by RavEngine: decoupled service boundaries and data-driven command submission.

### Lua
- Current `ScriptSystem` remains authoritative.
- Added `ScriptRuntimeSubsystem` adapter wrapper to run `ScriptSystem` through subsystem lifecycle phases.

### Physics (Jolt)
- Current `PhysicsSystem` remains authoritative.
- Added `PhysicsRuntimeSubsystem` adapter wrapper to run scene sync + Jolt stepping through subsystem lifecycle phases.

### Spatial Audio
- Added `SpatialAudioSystem` skeleton:
  - Listener state
  - Emitter submission per frame
  - Backend-agnostic update loop (OpenAL/miniaudio swap possible later)

### Networking
- Added `NetworkingSystem` skeleton:
  - Modes: disabled/client/server
  - Tick-rate management
  - Replication channel metadata (QoS/reliability)

## 4) Lightweight Resource Packaging

Inspired by RavEngine `pack_resources`: deterministic build-time packaging utility.
- Added `scripts/package_resources.py` and CMake target `pack_resources`.
- Packages glTF-first runtime assets (glb/gltf), Lua scripts, audio, and common textures.
- Writes `build/packaged_assets/manifest.txt` for deterministic runtime/server loading.

Why this fits Elysium:
- Keeps packaging simple and transparent.
- Works for local runtime and future headless deployment pipelines.
- Avoids forcing custom archive format too early.

## 5) Next Increment Suggestions

Inspired by Enigine: viewport visual debugging depth.
- Add overdraw-like and linear-depth debug modes.
- Add optional wireframe overlay toggle for selected entity.

Inspired by RavEngine: network-ready simulation boundaries.
- Add fixed-step simulation clock service.
- Add transform replication component set and authority roles.
- Add deterministic snapshot serialization for tile + entity state.

## 6) Follow-Up Implemented

Inspired by RavEngine: subsystem orchestration in the main app loop.
- `Application` now initializes/runs/shuts down via `SubsystemManager` lifecycle calls.
- Added runtime adapters for Lua and Jolt integration through subsystem phases.

Inspired by Enigine: lightweight editor-visible runtime diagnostics.
- Added an in-viewport debug stats overlay (toggleable from top menu as `Viewport Stats`).

Inspired by RavEngine `pack_resources`: build profile packaging.
- Added packaging profiles and CMake targets:
  - `pack_resources_client`
  - `pack_resources_server`
  - `pack_resources_editor`
- `pack_resources` now aliases client profile output.

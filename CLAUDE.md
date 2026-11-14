# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Windows-only Vulkan renderer (C++20, CMake target `VkEngine`). It loads glTF scenes and renders them with a GPU-driven pipeline: compute-based culling, sorting and batching into indirect draws, plus HZB occlusion, deferred PBR + IBL, SDSM-fitted cascaded shadow maps, and post-processing. Shaders are written in Slang.

## Build & run

Requirements: the Vulkan SDK with `VULKAN_SDK` set (shaders compile with `$VULKAN_SDK/Bin/slangc.exe`), clang, and Ninja. The existing `build/` directory is configured with Ninja and VS 2022's bundled LLVM clang in Debug. VS Code's CMake Tools is set up the same way (`.vscode/settings.json`).

```sh
# configure (single-config generator; build type: Debug | RelWithDebInfo | Release — MinSizeRel is rejected)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++

# build (also compiles all shaders via the SHADERS custom target)
cmake --build build

# run — must be run from build/: asset paths are hard-coded relative ("../assets/...")
cd build && ./VkEngine.exe [path/to/scene.gltf]
```

- Source and shader files are collected with `file(GLOB_RECURSE)`. **After adding or removing a `.cpp`/`.h` or `.slang` file, re-run the CMake configure step.**
- Dependencies (glm, fastgltf, stb, meshoptimizer, VMA, plus Tracy and imgui_bundle outside Release) come from `FetchContent` into `build/_deps`.
- There are no tests and no lint setup. Clang builds use `-Wall -Wextra -Wpedantic`; MSVC builds use `/W4 /WX`.
- The default scene is set in `main()` (`../assets/ShadowTest/ShadowTest.gltf`). Other scenes are commented out next to it. `.gitignore` excludes most of `assets/`; only `LightSponza/` and `SkyboxTextures/` are tracked, and large binaries go through Git LFS.

### Build-type defines
| Config | C++ defines | Notes |
|---|---|---|
| Debug | `ENG_BUILD_DEBUG`, `ENG_PROFILING_ENABLED`, `ENG_ENABLE_DBG_DRAW` | Slang `-O0 -g -DENV_DEBUG`; logging, Vulkan debug utils and object names on |
| RelWithDebInfo ("Profile") | `ENG_BUILD_PROFILE`, profiling + dbg draw | logging and debug names on |
| Release | `ENG_BUILD_RELEASE` | no Tracy, no ImGui, no logging |

`SHADER_USE_REVERSED_Z` (CMake option, ON by default) defines `ENG_REVERSED_Z` in C++ and `ENV_REVERSED_Z` in Slang. Depth-related code must handle both. In `main.cpp`, `ENG_ENABLE_DBG_DRAW` is re-exposed as `ENG_DEBUG_DRAW_ENABLED`.

## Architecture

### Layers
- `source/core/` — engine-agnostic code: Win32 window and event queue (`platform/`), math (frustum, AABB, transforms on glm), camera, Tracy-based profiler macros (`TM_MARKER_C`, `TM_GPU_MARKER_C`, …), and logging/assert macros (`CORE_LOG_*`, `CORE_ASSERT*`).
- `source/render/core/vulkan/` — thin RAII wrappers in namespace `vkn` (`Instance`, `Device`, `Swapchain`, `Allocator` are singletons reached through `::Inst()`; `Buffer`, `Texture`, `TextureView`, `PSO`, `CmdBuffer`, `DescriptorBuffer`, …). Use `VK_CHECK` and `VK_ASSERT*` from `vk_core.h`. Textures and buffers carry access trackers (`vk_resource_access_tracker.h`), so barriers name only the *destination* layout/stage/access through `cmdBuffer.BeginBarrierList().AddTextureBarrier(...)/AddBufferBarrier(...).Push()`.
- `source/render/debug/dbg_ui.*` — ImGui integration. It compiles to no-ops without `ENG_ENABLE_DBG_DRAW`.
- `source/main.cpp` — **the renderer and application itself**: all GPU structs, resources (file-static `s_*` globals), descriptor/PSO setup, scene loading, every render pass, and the frame loop.

### Vulkan feature model
Vulkan 1.4 instance; uses dynamic rendering (`CmdBeginRendering`), synchronization2, `VK_EXT_descriptor_buffer`, `VK_KHR_push_descriptor`, extended dynamic state 3, and maintenance8. There is a single frame in flight: `RenderScene()` waits on `s_renderFinishedFence` before reusing resources.

Descriptors use two sets per pipeline:
- **Set 0 (`DESC_SET_PER_FRAME`)** — the "common" set in a descriptor buffer: samplers, common/camera CBs, geometry streams, mesh/LOD/material/instance buffers, material texture and sampler arrays, depth, HZB. It's bound with `CmdBindDescriptorBufferSets(pso, { GetDescriptorSetIndex(CommonDescSetDesc{}), DESC_SET_PER_FRAME })`.
- **Set 1 (`DESC_SET_PER_DRAW`)** — per-pass resources pushed with `CmdPushDescriptors(pso, DESC_SET_PER_DRAW, { vkn::PushDescriptor::... })`.

### main.cpp map (search by name — line numbers drift)
1. `GPU_*` structs/enums — CPU mirrors of shader structs (guarded with `static_assert` on size/offset).
2. `DescSetLayoutID`, `PassID` enums and their parallel `*_DBG_NAME` string arrays (each has a `static_assert` on count).
3. `*_DESCRIPTOR_SLOT` constants — binding indices for each pass.
4. Globals `s_*` — resources, CPU-side scene data, and tweakable settings (CSM, PCF/PCSS, debug toggles).
5. `Create*()` — init: device, RTs (`CreateDynamicRenderTargets`, recreated on resize), samplers, `CreateDescriptors()`, then `Create*DescriptorSetLayout`, `Create*PSOLayout` and `Create*PSO`, all wired up in `CreateDescriptorSetLayouts()` / `CreatePipelines()`.
6. Scene loading — `LoadScene()` → fastgltf → `LoadScene{Mesh,Samplers,Material,Textures,Inst}Data` (meshoptimizer generates up to `GEOM_MAX_LOD_COUNT` LODs) → `UploadGPU*`.
7. Passes, in frame order (`RenderScene()`): `PrevFrameHzbPass` → `MainViewPass` (GPU cull → radix sort → batch → depth prepass → HZB) → `CsmPass` (SDSM depth reduction → split fitting → per-split cull + depth) → `DeferredPass` (GBuffer + lighting) → `ForwardPass` (skybox) → `PostProcessingPass` (tonemap) → `DebugPass` (debug primitives, RT viewer, ImGui) → `ResolveToBackbufferPass`.
8. `main()` → init sequence → IBL precompute via `ImmediateSubmitQueue` → `ProcessFrame()` loop.

### Shaders (`shaders/`)
- `source/**/name.{vs,ps,cs}.slang` compiles to `build/shaders/bin/<same relative path>/name.{vs,ps,cs}.spv`. The profile comes from the suffix (`vs_6_6`/`ps_6_6`/`cs_6_6`) and the entry point is always `main`. C++ loads them through the `SHADERS_SPIRV_DIR` define.
- `include/registers/<pass>_registers.slang` declares each pass's per-draw bindings (`[vk::binding(N, DESC_SET_PER_DRAW)]`). `common_registers.slang` declares set 0.
- `include/common/*_structs.slang` defines the shader side of the `GPU_*` structs; `include/pass/<pass>/vs{in,out}.slang` defines the stage interfaces.
- Use the macros in `include/common/system/sys_constants.slang` (`SAMPLE_TEX`, `LOAD_TEX`, `SV_DISPATCH_THREAD_ID`, …) instead of raw intrinsics.

### Invariants to keep in sync when changing things
- **`GPU_*` structs**: edit the C++ struct in `main.cpp` *and* its Slang twin in `shaders/include/common/`, then update the `static_assert`s.
- **Bindings**: a pass's `*_DESCRIPTOR_SLOT` constants in `main.cpp`, its `Create*DescriptorSetLayout()`, and its `registers/*_registers.slang` must agree. The same goes for the common set (`COMMON_*_DESCRIPTOR_SLOT` vs `common_registers.slang`).
- **Constants shared with shaders** (CS group sizes, `CSM_SPLIT_COUNT`, sampler IDs, enum values like `GPU_TonemapPreset`, `GPU_DbgRTViewType`) are duplicated in Slang and must match.
- **Adding a pass**: add a `PassID` (+ debug name) and, if it needs a new layout, a `DescSetLayoutID` (+ debug name) → slot constants → layout/PSO-layout/PSO creation functions registered in `CreateDescriptorSetLayouts()` / `CreatePipelines()` → a `*_registers.slang` file plus the shader source(s) → a pass function called from `RenderScene()` with barriers and `TM_MARKER_C`/`TM_GPU_MARKER_C` → re-run CMake configure.
- Push constant structs must stay ≤ 256 bytes (`static_assert` on `GPU_GeomCullPushConst`).

## Conventions
- Naming: `PascalCase` functions and types, `s_` file statics, `m_` members, `p` prefix for pointers (`s_pWnd`), `ALL_CAPS` constants and enum values. GPU-visible types use the `GPU_` prefix.
- Debug-only code goes under `#ifdef ENG_DEBUG_DRAW_ENABLED` (main.cpp) / `ENG_ENABLE_DBG_DRAW`. Vulkan objects get `SetDebugName(...)`.
- `pch.h` is the precompiled header (STL + platform includes).

# 3DAndEngine — Release Notes

## Release v0.4.0 — Pure Blind Architecture (current)

### Core
*   **Paged Sparse-Set ECS** (`EcsRegistry.h`): 4 KB sparse pages, dense component arrays, O(1) swap-and-pop removal, `EcsListener` callbacks.
*   **Job System** (`JobSystem.h`): bounded lock-free MPMC queue (Vyukov algorithm) with cache-line aligned slots and cooperative waiting.
*   **EventBus** (`EventBus.h`): 64-bit FNV-1a event ids (`""_id`, evaluated at compile time), immediate and buffered dispatch.
*   **Main loop** (`Application.h`): fixed-timestep accumulator (default 1/60 s) with three phases: `on_fixed_update`, `on_update`, `on_render(alpha)`.
*   **Plugin ABI** (`PluginContract.h`, `PluginLoader.h`): `core::PluginInterface`, `GetPluginAPI()` / `DECLARE_ENGINE_PLUGIN`, ABI handshake `0x00040000`, recursive discovery and priority ordering.
*   **Self-test and CI mode:** unit tests run at startup; `3DAndEngine --ci` runs them plus 30 headless frames.

### Fixes after v0.4.0
*   **Crash on exit with several plugins** fixed: plugin libraries are now unloaded after `Application` destroys its ECS pools (`src/main.cpp`).
*   **`EngineContext` is now long-lived.** `on_load` receives a pointer to one object owned by `Application` (previously a temporary copy that dangled after `on_load` returned). Its fields follow `ServiceRegistry` changes, so services registered later are visible to already loaded plugins. The struct layout is unchanged, ABI stays `0x00040000`: existing plugins keep working without a rebuild.
*   Removed duplicate and dead code (`LibraryLoader.h`, unused platform helpers, "fast slots" in `ServiceRegistry`, copy-pasted plugin dispatch loops); swallowed exceptions in `EventBus`, `JobSystem` and ECS listeners are now logged.

### Repository maintenance
*   Removed build output (`3DAndEngine/x64/Debug`) and the vendored `vcpkg_installed/` directory from version control.
*   Removed the unused duplicate `include/core/EngineContext.h` (the active definition lives in `PluginContract.h`), the unused `cmake/Config.cmake.in` and the misplaced `.github/workflows/config.json`.
*   Tests now use `CORE_CHECK` instead of `assert`, so they are no longer disabled in Release builds; `ctest` runs the engine in `--ci` mode.
*   Build output moved from the source tree to `<build>/bin`.
*   `vcpkg.json`: version aligned with the project, unused dependencies (`fmt`, `spdlog`, `nlohmann-json`) removed.
*   `PLUGIN_GUIDE.md` rewritten for the v0.4.0 plugin API; README corrected to match the code.

### Earlier versions
v0.2.0 and v0.3.0 were intermediate steps between the v0.1.0 baseline below and v0.4.0 (see the evolution table in the README).

---

## Release v0.1.0 (Initial MVP) — Legacy Baseline
**Release Date:** 2026-07-23

### Summary
The initial public MVP build of the `3DAndEngine Core` ecosystem. This release establishes the fundamental runtime environment, basic system orchestration, and an automated continuous integration pipeline.

### Core Architecture Changes
*   **Microkernel Foundation:** Introduced the initial `Application` host runtime loop, an early non-DOD `ServiceManager`, and a synchronous `EventBus` using string-based topic lookups.
*   **Primitive Plugin Subsystem:** Implemented a recursive directory exploration scanner (`plugins/**`) driven by runtime C-ABI identification. Added support for raw JSON manifests featuring basic semantic versioning (`core_version_required`), active-state toggles (`enabled`), and a multi-phase system fallback sequence in the event of an initialization crash.
*   **System Infrastructure:** Integrated automated `LoggerService` (driven by `spdlog`) and a centralized `ConfigService` (leveraging `nlohmann::json`).
*   **Early Concurrency:** Deployed a foundational heap-allocated `TaskScheduler` thread pool utilizing standard blocking mutex synchronization primitives.
*   **Verification & CI Automation:** Engineered a comprehensive GitHub Actions matrix pipeline with native cross-platform compilation targets (`ubuntu-latest`, `windows-latest`, `macos-latest`), integrated `vcpkg` manifest dependencies caching, and automated distribution bundle packaging upon formal VCS tag tracking.

---

### Deep-Dive Specifications
*   **The Binary Contract:** Standardized the low-level C-ABI boundary using strictly exported signatures: `CreatePlugin()`, `DestroyPlugin()`, and `GetPluginManifest()`. Manifest validation strictly monitors root configuration tokens (`name`, `version`, `core_version_required`, and `enabled`).
*   **Service Lifecycle Management:** The infrastructure pipeline orchestrates rigid initialization states (`init` $\rightarrow$ `start` $\rightarrow$ `postStart` $\rightarrow$ `stop`) and actively executes graphs to intercept recursive dependency deadlocks.
*   **Graceful Shutdown Sequences:** The `TaskScheduler` pipeline monitors ongoing compute frames, forcing orderly worker relaxation and job completion boundaries during engine teardown procedures.

---

### Release & Deployment Blueprint (VCS Workflow)
1.  Verify the core SDK version identifier matches the targeted release token (`ENGINE_ABI_VERSION` in `PluginContract.h`, `VERSION` in `CMakeLists.txt`, `vcpkg.json`).
2.  Commit workspace progress and tag the repository directly through the command line interface:
    ```bash
    git add .
    git commit -m "Release v0.1.0"
    git tag -a v0.1.0 -m "Architecture Baseline v0.1.0"
    git push origin master --follow-tags
    ```
3.  Formulate a formal GitHub Release via the Web UI or GitHub CLI (`gh release create`). The automated pipeline will intercept the tag, build the Windows executable and attach it to the release as a single `.exe` file.

---

### Historical Constraints (Resolved in v0.4.0 Evolution)
*   *Limitation:* Heavy object scatter across the heap and tight coupling due to reliance on standard shared pointer wrappers (`std::shared_ptr`).  
    *👉 **v0.4.0 Resolution:** Fully migrated to a Paged Sparse Set ECS memory model with strict Data-Oriented Design (DOD) layouts.*
*   *Limitation:* High synchronization overhead caused by blocking mutex guards and standard thread pool implementations.  
    *👉 **v0.4.0 Resolution:** Replaced with Dmitry Vyukov's Lock-Free Bounded MPMC task queue matrix and explicit `alignas(64)` false-sharing padding.*
*   *Limitation:* String parsing runtime overhead on event dispatch networks.  
    *👉 **v0.4.0 Resolution:** Implemented 1-cycle 64-bit FNV-1a constexpr hash literal evaluations (`""_id`).*

---
*Thank you for exploring the roots of 3DAndEngine Core — Welcome to the high-performance engineering ecosystem!*

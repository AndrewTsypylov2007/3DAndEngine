# 3DAndEngine — Historical Release Notes

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
1.  Verify the core SDK version identifier matches the targeted release token within the `PluginLoader` runtime engine.
2.  Commit workspace progress and tag the repository directly through the command line interface:
    ```bash
    git add .
    git commit -m "Release v0.1.0"
    git tag -a v0.1.0 -m "Architecture Baseline v0.1.0"
    git push origin main --follow-tags
    ```
3.  Formulate a formal GitHub Release via the Web UI or GitHub CLI (`gh release create`). The automated pipeline will intercept the tag, compile production-ready distribution blocks, and bind the cross-platform SDK zip archives natively.

---

### Historical Constraints (Resolved in v0.4.0 Evolution)
*   *Limitation:* Heavy object scatter across the heap and tight coupling due to reliance on standard shared pointer wrappers (`std::shared_ptr`).  
    *👉 **v0.4.0 Resolution:** Fully migrated to a Paged Sparse Set ECS memory model with strict Data-Oriented Design (DOD) layouts.*
*   *Limitation:* High synchronization overhead caused by blocking мutex guards and standard thread pool implementations.  
    *👉 **v0.4.0 Resolution:** Replaced with Dmitry Vyukov's Lock-Free Bounded MPMC task queue matrix and explicit `alignas(64)` false-sharing padding.*
*   *Limitation:* String parsing runtime overhead on event dispatch networks.  
    *👉 **v0.4.0 Resolution:** Implemented 1-cycle 64-bit FNV-1a constexpr hash literal evaluations (`""_id`).*

---
*Thank you for exploring the roots of 3DAndEngine Core — Welcome to the high-performance engineering ecosystem!*

# 3DAndEngine — High-Performance AAA Microkernel Framework (v0.4.0)

A high-performance, fault-tolerant, C++20 microkernel framework designed using strict **Data-Oriented Design (DOD)** principles and a **Pure Blind Architecture** ("Motherboard" pattern). 

The engine core is fully isolated from heavy OOP abstractions, zero dynamic memory allocations are performed during the main loop runtime, and it utilizes lock-free synchronization primitives. This guarantees a stable, predictable Frame Time (60–240+ Hz) even under extreme operational loads.

---

## 🔄 Architectural Evolution: Core Generations

| Architectural Layer | v0.1.0 / v0.2.0 (Legacy) | v0.3.0 (Commercial Baseline) | v0.4.0 (Pure Blind AAA Standard) | Performance & Engineering Impact |
| :--- | :--- | :--- | :--- | :--- |
| **Core Paradigm** | Tight coupling (OOP), heavy reliance on `std::shared_ptr`. | Basic DOD, partial module isolation. | **Pure Blind Architecture.** Modules are 100% blind and isolated. | **Zero Coupling.** Graphics, Physics, and Input modules share no state, communicating exclusively via raw data structures inside `EngineContext`. |
| **Memory Management (ECS)** | Scattered objects allocated across the Heap. | Naive Sparse Set (unstable `std::vector<uint32_t>` handling <100k entities). | **Paged Sparse Set ECS** (4KB Sparse Pages) + Swap-and-Pop defragmentation. | **Zero CPU Cache Misses & No Out-of-Bounds errors.** Reduces RAM usage by up to 95% on sparse indices. Contiguous Dense arrays enable optimal SIMD and L1/L2 cache utilization. |
| **Event System (EventBus)** | `std::string` topics with character-by-character comparison. | 32-bit hashes protected by blocking mutexes. | **64-bit FNV-1a event ids** (`""_id`, computed at compile time) + buffered (`publish` / `processEvents`) and immediate (`dispatchImmediate`) dispatch. | **No string comparisons at dispatch.** Recursive mutexes make nested dispatch deadlock-safe; ids use the full 64-bit space, so collisions are practically impossible. |
| **Multithreading** | Basic Thread Pool driven by heavy `std::mutex`. | Task queue with high thread contention. | **Lock-Free Bounded MPMC Queue** (Dmitry Vyukov's algorithm) + `alignas(64)` padding. | **Zero Lock Contention.** Fixed 64-byte task slots, active `_mm_pause()` loop-relaxation, and automated cooperative work-assisting. |
| **Main Loop Logic** | Variable `dt` via `std::chrono` (causing object tunneling). | Simple frame cycle lacking state interpolation. | **Fixed Physical Accumulator** (64-bit Fixed Timestep @ 60 Hz) + Render Alpha-Interpolation. | **Deterministic Physics.** Full protection against the "Spiral of Death" during render lags; guarantees absolute rendering smoothness. |
| **Binary Contract (C-ABI)** | C++ Virtual Tables (vtable); UB across different compilers. | Basic `extern "C"` bindings. | **Strict C-ABI Handshake** (0x00040000) with compile-time version validation. | **100% Binary Safety.** Eliminates engine crashes caused by SDK version mismatches or conflicting compiler toolchains. |

---

## 🛠 Core Microkernel Components

*   **`Types.h`:** Provides a 64-bit compile-time FNV-1a string hashing mechanism. String literals like `"renderer/main_pass"_id` are fully evaluated as `constexpr` during compilation, resulting in zero runtime overhead.
*   **`EcsRegistry.h`:** Implements a high-density Paged Sparse Set component registry. Entity creation, lookup, and deletion are handled via a Swap-and-Pop routine, keeping memory tightly packed inside Dense vectors.
*   **`EventBus.h`:** An event bus with 64-bit ids. Supports immediate dispatch (`dispatchImmediate`) and a per-frame buffer (`publish` + `processEvents`). Subscriber table and buffer are guarded by recursive mutexes, so handlers may safely dispatch further events.
*   **`JobSystem.h`:** A lock-free task scheduler utilizing a Bounded Multi-Producer Multi-Consumer (MPMC) queue. Features explicit cache-line alignment (`alignas(64)`) to mitigate false sharing on multi-core systems.
*   **`PluginContract.h` & `PluginLoader.h`:** Define the C-ABI plugin layer: `core::PluginInterface`, the `GetPluginAPI()` entry point, the `DECLARE_ENGINE_PLUGIN` macro and the `ENGINE_ABI_VERSION` handshake. `PluginManager` discovers `.dll` / `.so` / `.dylib` files, rejects plugins with a mismatched ABI and sorts the rest by priority. See [PLUGIN_GUIDE.md](PLUGIN_GUIDE.md).
*   **`Application.h`:** The main engine host engine loop framework. Drives deterministic timing through a physical accumulator (`fixed_dt = 1/60s`) separated into three execution phases:
    1.  `on_fixed_update(1/60s)` — Governs deterministic systems: physics, collision detection, and network state sync.
    2.  `on_update(dt)` — Processes frame-variable logic: gameplay scripting, AI behavior trees, and skeletal animations.
    3.  `on_render(alpha)` — Handles frame-rate independent rendering, interpolating state transforms using the remaining `alpha` factor.

---

## 🚀 Architectural Example: "Blind" Module Interaction

Modules share no dependencies and are unaware of each other. They interact only through data pools (components) and events exposed by the `EngineContext` the core hands to every plugin in `on_load`.

```cpp
#include "core/PluginContract.h"
#include "core/EcsRegistry.h"

struct Position { float x = 0, y = 0; };
struct Velocity { float vx = 0, vy = 0; };

static core::EngineContext* g_ctx = nullptr;   // owned by Application, valid until on_unload

static void on_load(core::EngineContext* ctx) { g_ctx = ctx; }
static void on_update(float) {}       // mandatory callback

// Physics-style plugin: linearly streams over a contiguous dense array at a fixed step.
static void on_fixed_update(float fixed_dt) {
    auto* velocities = g_ctx->ecs->view<Velocity>();
    if (!velocities) return;

    velocities->each([&](core::Entity e, Velocity& vel) {
        if (auto* pos = g_ctx->ecs->getComponent<Position>(e)) {
            pos->x += vel.vx * fixed_dt;
            pos->y += vel.vy * fixed_dt;
        }
    });
}

static core::PluginInterface g_plugin = {
    "Movement", "1.0.0", core::ENGINE_ABI_VERSION, 100,
    on_load, on_fixed_update, on_update, /*on_render*/ nullptr, /*on_unload*/ nullptr
};
DECLARE_ENGINE_PLUGIN(g_plugin)
```

A complete, tested plugin (including build and install steps) is in [PLUGIN_GUIDE.md](PLUGIN_GUIDE.md).

---

## 🔨 Build Instructions & System Requirements

*   **Language Standard:** Strict **C++20** (ISO/IEC 14882:2020).
*   **Supported Compilers:**
    *   **MSVC:** Visual Studio 2022 (v143+) flags: `/std:c++20 /permissive-`
    *   **GCC:** 13.0+ flags: `-std=c++20 -O3`
    *   **Clang:** 16.0+ flags: `-std=c++20 -O3`
*   **Build System:** CMake 3.20+
*   **Package Management:** `vcpkg` (Manifest Mode).

### Local Build Commands

#### Linux / macOS
```bash
# Configure the project builds
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Compile the core framework and runner
cmake --build build --config Release --parallel

# Execute the test suite (core self-test + 30 headless frames)
ctest --test-dir build --output-on-failure
```

Binaries are produced in the build directory: `build/bin/3DAndEngine`. The runtime folders `plugins/`, `shaders/`, `assets/` and `configs/` are created next to it.

#### Windows (PowerShell / Developer Command Prompt)
```powershell
# Configure and build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel

# Run verification tests
ctest --test-dir build -C Release --output-on-failure
.\build\bin\Release\3DAndEngine.exe --ci
```

---

## 🧪 Core Verification Suite (`src/UnitTestsMain.cpp`)

The unit tests are compiled into the engine executable and run automatically at startup (a failure aborts startup with exit code 1). The checks use their own `CORE_CHECK` macro instead of `assert`, so they also run in Release builds (`-DNDEBUG`).

```bash
# Via CMake / CTest
ctest --test-dir build --output-on-failure

# Or run the binary directly (self-test + 30 headless frames)
./build/bin/3DAndEngine --ci

# Or compile without CMake
g++ -std=c++20 -O2 -I./include src/main.cpp src/UnitTestsMain.cpp -o 3DAndEngine -pthread -ldl
./3DAndEngine --ci
```

The suite currently covers:
1.  **64-bit FNV-1a hashing:** the `""_id` literal and `hash_str` agree and produce 64-bit values.
2.  **Paged Sparse-Set ECS:** component add / get and `EcsListener` notifications.
3.  **EventBus:** subscription and immediate dispatch.
4.  **Job System:** 100 jobs on 2 worker threads complete before `wait` returns.

---

## ⚙️ Automation & CI/CD Pipeline (GitHub Actions)

The repository features an automated multi-platform matrix pipeline:
*   **Cross-Platform Matrix:** Code validation is compiled and executed natively across `ubuntu-latest`, `windows-latest`, and `macos-latest`.
*   **CTest Quality Gate:** pull requests and commits are checked by `ctest`, which runs the core self-test and a 30-frame headless run; the job fails if any check fails.
*   **Release asset:** when a release is published, CI builds a Release binary on Windows and attaches a single file, `3DAndEngine-<tag>-windows-x64.exe`, to it. (GitHub additionally shows its automatic "Source code" archives on every release; they cannot be disabled.)

---

## 📁 Repository Layout

```
include/core/   Public header-only engine core (ECS, EventBus, JobSystem, plugin ABI, Application)
src/            main.cpp (runtime entry point) and UnitTestsMain.cpp (core self-tests)
.github/        CI workflow
CMakeLists.txt  Build definition (CMake 3.20+)
PLUGIN_GUIDE.md How to write and install a plugin
```

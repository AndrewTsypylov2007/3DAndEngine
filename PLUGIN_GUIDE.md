# Developer Guide: Authoring Plugins for 3DAndEngine Core

This document provides a concise, step-by-step specification for engineering, compiling, and validating a dynamic shared library (plugin) compatible with the `3DAndEngine` microkernel.

---

## 1. The Plugin Binary Contract (C-ABI)

To enforce strict boundary isolation and eliminate C++ compiler runtime mismatches, the engine core communicates with external modules via an immutable, low-level **C-ABI Export Layer**. Every plugin must explicitly export the following three functions:

*   `CreatePlugin()` $\rightarrow$ `core::IPlugin*`  
    Allocates and returns a raw pointer to the custom plugin instance.
*   `DestroyPlugin(core::IPlugin*)` $\rightarrow$ `void`  
    Safely deallocates the plugin instance and frees associated heap memory.
*   `GetPluginManifest()` $\rightarrow$ `const char*`  
    Returns a null-terminated, raw ASCII string containing a valid JSON manifest string for version and dependency validation.

---

## 2. Minimal Architecture Skeleton (`plugin.cpp`)

```cpp
#include "core/IPlugin.h"
#include "core/ServiceManager.h"
#include <iostream>

namespace core {
class MyPlugin : public IPlugin {
public:
    // Invoked by the microkernel upon successful loading and C-ABI validation
    bool initialize(ServiceManager& services) override {
        std::cerr \)<TARGET_FILE_DIR:engine_core>
  COMMENT "Deploying my_plugin binary artifact directly to engine_core run path")
```

---

## 4. Manifest Specification (`GetPluginManifest`)

The JSON string provided by the module boundary requires the following keys:
*   `name` *(string)*: Unique identifier of the plugin module.
*   `version` *(semver string)*: Version of the current plugin payload.
*   `core_version_required` *(semver string)*: Target engine engine SDK boundary layout.
*   `enabled` *(bool)*: Hardware-level skip token. If set to `false`, the microkernel runtime discover cycle completely ignores the binary during exploration.

---

## 5. Compilation & Lifecycle Verification

1.  **Generate and Compile the Engine Suite (Core + Target Plugin):**
    ```bash
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build --config Debug --parallel
    ```
2.  **Verify Module Attachment:**
    The post-build automation macro clones the `.dll`/`.so` module directly adjacent to the `engine_core` executable block. Run `engine_core` and inspect stdout/stderr. A successful hook outputs:  
    `[MyPlugin] Initializing core modules...`

---

## 6. Architectural Best Practices

*   **ABI Integrity:** Ensure the target plugin is built utilizing the exact same compiler version, flags (`/std:c++20` or `-std=c++20`), and `vcpkg` target triplet configuration as the main microkernel host. 
*   **Initialization Rollback:** If `initialize()` encounters a critical failure (e.g., failed VRAM allocation, missing asset pipelines), it **must** return `false`. The engine core will intercept this state and trigger a safety asset rollback sequence.
*   **Re-entrant Shutdown Safety:** The `shutdown()` function must be completely idempotent. It must release memory footprints safely, with robust guards to protect against double-free errors or memory corruption during rapid re-initialization passes.
*   **Service Registration:** To publish API bindings across the abstract engine system landscape, inject instances into the application context using the service manager:  
    `services.registerService<T, Dependencies...>(std::make_shared<T>());`

---

## 7. Advanced Configurations

*   **Zero-File-Deletion Disabling:** To disable a plugin without removing the compiled binary file from the installation path, set `"enabled": false` inside the C-ABI manifest return payload.
*   **External Dependencies:** If your plugin introduces heavy third-party assets (`glm`, `assimp`, `flecs`), manage them by adding `find_package` calls and linking them directly via `target_link_libraries` in the plugin’s local `CMakeLists.txt`, leveraging `vcpkg` manifest parameters.

---
*The provided blueprint can be freely copied into `plugins/new_plugin` to serve as a fast boilerplate skeleton for customized engine subsystems.*

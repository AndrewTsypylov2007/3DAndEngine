#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <string>
#include <stdexcept>

namespace core::platform {

    using LibHandle = void*;

    inline LibHandle load_library(const std::string& path) {
#if defined(_WIN32)
        LibHandle handle = ::LoadLibraryA(path.c_str());
        if (!handle) {
            throw std::runtime_error("Failed to load DLL: " + path + " (Error code: " + std::to_string(::GetLastError()) + ")");
        }
        return handle;
#else
        LibHandle handle = ::dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
        if (!handle) {
            throw std::runtime_error("Failed to load SO/DYLIB: " + path + " (Error: " + ::dlerror() + ")");
        }
        return handle;
#endif
    }

    inline void* get_symbol(LibHandle handle, const std::string& symbol_name) {
#if defined(_WIN32)
        void* sym = reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(handle), symbol_name.c_str()));
#else
        void* sym = ::dlsym(handle, symbol_name.c_str());
#endif
        if (!sym) {
            throw std::runtime_error("Failed to find symbol: " + symbol_name);
        }
        return sym;
    }

    inline void free_library(LibHandle handle) {
        if (!handle) return;
#if defined(_WIN32)
        ::FreeLibrary(static_cast<HMODULE>(handle));
#else
        ::dlclose(handle);
#endif
    }

} // namespace core::platform

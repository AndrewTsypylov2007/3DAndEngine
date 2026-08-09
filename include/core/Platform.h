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
#include <iostream>
#include <filesystem>

namespace core::platform {

    using LibHandle = void*;

    /**
     * @brief Автоматическая коррекция путей под платформу (v0.3.0)
     * Добавляет .dll, .so или .dylib если они отсутствуют.
     */
    inline std::string fix_lib_path(const std::string& path) {
        std::filesystem::path p(path);
#if defined(_WIN32)
        if (p.extension() != ".dll") p.replace_extension(".dll");
#elif defined(__APPLE__)
        if (p.extension() != ".dylib") p.replace_extension(".dylib");
#else
        if (p.extension() != ".so") p.replace_extension(".so");
#endif
        return p.string();
    }

    /**
     * @brief RAII Обертка над динамической библиотекой.
     * Гарантирует выгрузку модуля при выходе объекта из области видимости.
     */
    class SharedLibrary {
    private:
        LibHandle handle_ = nullptr;
        std::string path_;

    public:
        explicit SharedLibrary(const std::string& path) : path_(fix_lib_path(path)) {
#if defined(_WIN32)
            handle_ = ::LoadLibraryA(path_.c_str());
            if (!handle_) {
                DWORD error = ::GetLastError();
                throw std::runtime_error("[Platform] Failed to load DLL: " + path_ +
                    " (WinError: " + std::to_string(error) + ")");
            }
#else
            handle_ = ::dlopen(path_.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!handle_) {
                throw std::runtime_error("[Platform] Failed to load Shared Lib: " + path_ +
                    " (Error: " + ::dlerror() + ")");
            }
#endif
            std::cout << "[Platform] Module loaded successfully: " << path_ << std::endl;
        }

        ~SharedLibrary() {
            if (handle_) {
#if defined(_WIN32)
                ::FreeLibrary(static_cast<HMODULE>(handle_));
#else
                ::dlclose(handle_);
#endif
            }
        }

        // Удаляем копирование, чтобы избежать двойного освобождения хэндла
        SharedLibrary(const SharedLibrary&) = delete;
        SharedLibrary& operator=(const SharedLibrary&) = delete;

        // Разрешаем перемещение для хранения в std::vector внутри Application
        SharedLibrary(SharedLibrary&& other) noexcept
            : handle_(other.handle_), path_(std::move(other.path_)) {
            other.handle_ = nullptr;
        }

        SharedLibrary& operator=(SharedLibrary&& other) noexcept {
            if (this != &other) {
                if (handle_) {
#if defined(_WIN32)
                    ::FreeLibrary(static_cast<HMODULE>(handle_));
#else
                    ::dlclose(handle_);
#endif
                }
                handle_ = other.handle_;
                path_ = std::move(other.path_);
                other.handle_ = nullptr;
            }
            return *this;
        }

        /**
         * @brief Типобезопасное получение функции из библиотеки.
         * Пример: auto func = lib.get_function<void(*)(int)>("my_func");
         */
        template<typename T>
        T get_function(const std::string& name) const {
            void* sym = nullptr;
#if defined(_WIN32)
            sym = reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(handle_), name.c_str()));
#else
            sym = ::dlsym(handle_, name.c_str());
#endif
            if (!sym) {
                throw std::runtime_error("[Platform] Symbol not found: " + name + " in " + path_);
            }
            return reinterpret_cast<T>(sym);
        }

        LibHandle raw_handle() const { return handle_; }
        const std::string& path() const { return path_; }
    };

    // --- Legacy API (Оставлено для совместимости со старым кодом ядра) ---

    inline LibHandle load_library(const std::string& path) {
        std::string fixed = fix_lib_path(path);
#if defined(_WIN32)
        return static_cast<LibHandle>(::LoadLibraryA(fixed.c_str()));
#else
        return ::dlopen(fixed.c_str(), RTLD_NOW | RTLD_GLOBAL);
#endif
    }

    inline void* get_symbol(LibHandle handle, const std::string& symbol_name) {
        if (!handle) return nullptr;
#if defined(_WIN32)
        return reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(handle), symbol_name.c_str()));
#else
        return ::dlsym(handle, symbol_name.c_str());
#endif
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

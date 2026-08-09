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
     * @brief Автоматическая коррекция расширений под ОС (v0.3.0)
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
     * @brief SharedLibrary (v0.3.0 Commercial)
     * RAII-обертка: сама загружает и сама выгружает библиотеку.
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
                throw std::runtime_error("[Platform] Не удалось загрузить DLL: " + path_ +
                    " (Код ошибки WinAPI: " + std::to_string(error) + ")");
            }
#else
            handle_ = ::dlopen(path_.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!handle_) {
                throw std::runtime_error("[Platform] Не удалось загрузить библиотеку: " + path_ +
                    " (Ошибка: " + ::dlerror() + ")");
            }
#endif
            std::cout << "[Platform] Модуль успешно загружен: " << path_ << std::endl;
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

        // Запрещаем копирование (защита от двойного удаления)
        SharedLibrary(const SharedLibrary&) = delete;
        SharedLibrary& operator=(const SharedLibrary&) = delete;

        // Разрешаем перемещение (для хранения в контейнерах)
        SharedLibrary(SharedLibrary&& other) noexcept
            : handle_(other.handle_), path_(std::move(other.path_)) {
            other.handle_ = nullptr;
        }

        /**
         * @brief Типобезопасный поиск функции
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
                throw std::runtime_error("[Platform] Символ не найден: " + name + " в " + path_);
            }
            return reinterpret_cast<T>(sym);
        }

        LibHandle raw_handle() const { return handle_; }
    };

} // namespace core::platform

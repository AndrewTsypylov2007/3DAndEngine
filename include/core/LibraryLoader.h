#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <string>
#include <stdexcept>
#include <iostream>
#include <filesystem>
#include <system_error>

namespace core::platform {

    using LibHandle = void*;

    /**
     * @brief Автоматическая коррекция расширений и кроссплатформенная нормализация путей
     */
    inline std::filesystem::path fix_lib_path(const std::filesystem::path& raw_path) {
        std::filesystem::path p = raw_path;
#if defined(_WIN32)
        if (p.extension() != ".dll") p.replace_extension(".dll");
#elif defined(__APPLE__)
        if (p.extension() != ".dylib") p.replace_extension(".dylib");
#else
        if (p.extension() != ".so") p.replace_extension(".so");
#endif
        return p;
    }

    /**
     * @brief SharedLibrary (v0.3.5 AAA Commercial Standard)
     * Высоконадежная RAII-обертка для динамической загрузки библиотек с поддержкой Unicode.
     */
    class SharedLibrary {
    private:
        LibHandle             handle_ = nullptr;
        std::filesystem::path path_;

    public:
        explicit SharedLibrary(const std::filesystem::path& filepath)
            : path_(fix_lib_path(filepath)) {

#if defined(_WIN32)
            // Поддержка путей в Unicode (LoadLibraryW)
            handle_ = static_cast<LibHandle>(::LoadLibraryW(path_.wstring().c_str()));
            if (!handle_) {
                DWORD err_code = ::GetLastError();
                std::string err_msg = std::system_category().message(err_code);
                throw std::runtime_error("[Platform::SharedLibrary] Ошибка загрузки DLL: " +
                    path_.string() + " (WinAPI " + std::to_string(err_code) + ": " + err_msg + ")");
            }
#else
            // RTLD_DEEPBIND изолирует зависимости плагинов от пересечения глобальных символов в Linux
#if defined(__linux__) && defined(RTLD_DEEPBIND)
            int flags = RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND;
#else
            int flags = RTLD_NOW | RTLD_LOCAL;
#endif

            // Сброс предыдущих ошибок
            ::dlerror();
            handle_ = ::dlopen(path_.c_str(), flags);
            if (!handle_) {
                const char* error_str = ::dlerror();
                throw std::runtime_error("[Platform::SharedLibrary] Ошибка загрузки модуля: " +
                    path_.string() + " (" + (error_str ? error_str : "Unknown dlopen error") + ")");
            }
#endif
            std::cout << "[Platform::SharedLibrary] Модуль успешно загружен: " << path_.string() << std::endl;
        }

        ~SharedLibrary() {
            unload();
        }

        // Запрет копирования (защита от двойного FreeLibrary)
        SharedLibrary(const SharedLibrary&) = delete;
        SharedLibrary& operator=(const SharedLibrary&) = delete;

        // Перемещение владения ресурсом
        SharedLibrary(SharedLibrary&& other) noexcept
            : handle_(other.handle_), path_(std::move(other.path_)) {
            other.handle_ = nullptr;
        }

        SharedLibrary& operator=(SharedLibrary&& other) noexcept {
            if (this != &other) {
                unload();
                handle_ = other.handle_;
                path_ = std::move(other.path_);
                other.handle_ = nullptr;
            }
            return *this;
        }

        /**
         * @brief Безопасный поиск функции без выброса исключений (для опциональных хуков)
         */
        template<typename T>
        T try_get_function(const std::string& name) const noexcept {
            if (!handle_) return nullptr;

            void* sym = nullptr;
#if defined(_WIN32)
            sym = reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(handle_), name.c_str()));
#else
            sym = ::dlsym(handle_, name.c_str());
#endif
            return reinterpret_cast<T>(sym);
        }

        /**
         * @brief Строгий поиск обязательного символа (бросает исключение при отсутствии)
         */
        template<typename T>
        T get_function(const std::string& name) const {
            T func = try_get_function<T>(name);
            if (!func) {
                throw std::runtime_error("[Platform::SharedLibrary] Обязательный символ '" + name +
                    "' не найден в модуле " + path_.string());
            }
            return func;
        }

        bool is_loaded() const noexcept { return handle_ != nullptr; }
        LibHandle raw_handle() const noexcept { return handle_; }
        const std::filesystem::path& path() const noexcept { return path_; }

    private:
        void unload() noexcept {
            if (handle_) {
#if defined(_WIN32)
                ::FreeLibrary(static_cast<HMODULE>(handle_));
#else
                ::dlclose(handle_);
#endif
                handle_ = nullptr;
            }
        }
    };

} // namespace core::platform
#pragma once

#include "Platform.h"
#include "PluginContract.h"
#include <filesystem>
#include <vector>
#include <memory>
#include <algorithm>
#include <iostream>
#include <string>

namespace core {

    /**
     * @brief Обертка над загруженным плагином, удерживающая библиотеку в памяти.
     */
    class LoadedPlugin {
    private:
        std::unique_ptr<platform::SharedLibrary> library_;
        PluginInterface* interface_ = nullptr;

    public:
        LoadedPlugin(std::unique_ptr<platform::SharedLibrary> lib, PluginInterface* api)
            : library_(std::move(lib)), interface_(api) {
        }

        ~LoadedPlugin() = default;

        LoadedPlugin(const LoadedPlugin&) = delete;
        LoadedPlugin& operator=(const LoadedPlugin&) = delete;
        LoadedPlugin(LoadedPlugin&&) noexcept = default;
        LoadedPlugin& operator=(LoadedPlugin&&) noexcept = default;

        PluginInterface* get() const noexcept { return interface_; }
        std::string path() const { return library_ ? library_->path_string() : ""; }
        const char* name() const noexcept { return (interface_ && interface_->name) ? interface_->name : "Unnamed Plugin"; }
        uint32_t priority() const noexcept { return interface_ ? interface_->priority : 1000; }
    };

    /**
     * @brief Менеджер динамического обнаружения и загрузки модулей ядра v0.4.0.
     */
    class PluginManager {
    public:
        /**
         * @brief Проверка, является ли файл динамической библиотекой под текущую ОС
         */
        static bool is_shared_library(const std::filesystem::path& path) {
            auto ext = path.extension().string();
#if defined(_WIN32)
            return ext == ".dll";
#elif defined(__APPLE__)
            return ext == ".dylib" || ext == ".so";
#else
            return ext == ".so";
#endif
        }

        /**
         * @brief Загрузить один конкретный плагин по пути с валидацией ABI
         */
        static std::unique_ptr<LoadedPlugin> load_plugin(const std::filesystem::path& file_path) {
            try {
                auto lib = std::make_unique<platform::SharedLibrary>(file_path);

                auto get_api = lib->try_get_function<PluginInterface * (*)()>("GetPluginAPI");
                if (!get_api) {
                    std::cerr << "[PluginManager WARNING] Точка входа GetPluginAPI не найдена в "
                        << file_path.string() << std::endl;
                    return nullptr;
                }

                PluginInterface* api = get_api();
                if (!api) {
                    std::cerr << "[PluginManager WARNING] GetPluginAPI вернул nullptr в "
                        << file_path.string() << std::endl;
                    return nullptr;
                }

                // Проверка версии ABI
                if (api->abi_version != ENGINE_ABI_VERSION) {
                    std::cerr << "[PluginManager ERROR] Несовместимая версия ABI у плагина '"
                        << (api->name ? api->name : "Unknown") << "' (" << file_path.filename().string() << ")!"
                        << " Ожидалась: 0x" << std::hex << ENGINE_ABI_VERSION
                        << ", Получена: 0x" << api->abi_version << std::dec << std::endl;
                    return nullptr;
                }

                if (!api->on_update) {
                    std::cerr << "[PluginManager ERROR] Плагин '" << (api->name ? api->name : "Unknown")
                        << "' не содержит обязательного метода on_update!" << std::endl;
                    return nullptr;
                }

                std::cout << "[PluginManager] Верифицирован модуль: ["
                    << (api->name ? api->name : "Unknown")
                    << "] v" << (api->version ? api->version : "0.0.0")
                    << " (Приоритет: " << api->priority << ")" << std::endl;

                return std::make_unique<LoadedPlugin>(std::move(lib), api);
            }
            catch (const std::exception& e) {
                std::cerr << "[PluginManager ERROR] Ошибка при загрузке плагина "
                    << file_path.string() << ": " << e.what() << std::endl;
                return nullptr;
            }
        }

        /**
         * @brief 1. СЛЕПОЙ РЕКУРСИВНЫЙ ПОИСК (Ищет во всех вложенных папках)
         */
        static std::vector<std::unique_ptr<LoadedPlugin>> discover_recursive(const std::filesystem::path& root_dir) {
            std::vector<std::unique_ptr<LoadedPlugin>> found;
            std::error_code ec;

            if (!std::filesystem::exists(root_dir, ec)) {
                std::filesystem::create_directories(root_dir, ec);
                return found;
            }

            for (const auto& entry : std::filesystem::recursive_directory_iterator(root_dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
                if (entry.is_regular_file(ec) && is_shared_library(entry.path())) {
                    auto plugin = load_plugin(entry.path());
                    if (plugin) {
                        found.push_back(std::move(plugin));
                    }
                }
            }

            sort_by_priority(found);
            return found;
        }

        /**
         * @brief 2. ПРОСТОЙ ПЛОСКИЙ ПОИСК (Только указанная папка)
         */
        static std::vector<std::unique_ptr<LoadedPlugin>> discover_flat(const std::filesystem::path& directory) {
            std::vector<std::unique_ptr<LoadedPlugin>> found;
            std::error_code ec;

            if (!std::filesystem::exists(directory, ec)) {
                std::filesystem::create_directories(directory, ec);
                return found;
            }

            for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
                if (entry.is_regular_file(ec) && is_shared_library(entry.path())) {
                    auto plugin = load_plugin(entry.path());
                    if (plugin) {
                        found.push_back(std::move(plugin));
                    }
                }
            }

            sort_by_priority(found);
            return found;
        }

        /**
         * @brief Стандартный псевдоним (по умолчанию рекурсивный)
         */
        static std::vector<std::unique_ptr<LoadedPlugin>> discover(const std::filesystem::path& directory) {
            return discover_recursive(directory);
        }

    private:
        static void sort_by_priority(std::vector<std::unique_ptr<LoadedPlugin>>& list) {
            std::sort(list.begin(), list.end(),
                [](const std::unique_ptr<LoadedPlugin>& a, const std::unique_ptr<LoadedPlugin>& b) {
                    return a->priority() < b->priority();
                }
            );
        }
    };

} // namespace core
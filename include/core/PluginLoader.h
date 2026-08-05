#pragma once
#include "Platform.h"
#include "PluginContract.h"
#include <filesystem>
#include <vector>
#include <memory>
#include <iostream>

namespace core {

    class LoadedPlugin {
    private:
        platform::LibHandle handle_ = nullptr;
        PluginInterface* interface_ = nullptr;

    public:
        LoadedPlugin(platform::LibHandle handle, PluginInterface* interface)
            : handle_(handle), interface_(interface) {
        }

        ~LoadedPlugin() {
            if (handle_) {
                std::cout << "[Core] Unloading binary module handle.\n";
                platform::free_library(handle_);
            }
        }

        // Запрещаем копирование, чтобы случайно не освободить handle дважды (RAII)
        LoadedPlugin(const LoadedPlugin&) = delete;
        LoadedPlugin& operator=(const LoadedPlugin&) = delete;

        // Разрешаем перемещение
        LoadedPlugin(LoadedPlugin&& other) noexcept
            : handle_(other.handle_), interface_(other.interface_) {
            other.handle_ = nullptr;
            other.interface_ = nullptr;
        }

        PluginInterface* getAPI() const { return interface_; }
    };

    class PluginLoader {
    public:
        static std::unique_ptr<LoadedPlugin> load(const std::filesystem::path& path) {
            try {
                // Атомарно загружаем бинарник в виртуальную память процесса
                platform::LibHandle handle = platform::load_library(path.string());

                // Запрашиваем AAA C-контракт точки входа
                auto* get_api_func = reinterpret_cast<PluginInterface * (*)()>(
                    platform::get_symbol(handle, "GetPluginAPI")
                    );

                PluginInterface* api = get_api_func();
                if (!api) {
                    platform::free_library(handle);
                    return nullptr;
                }

                std::cout << "[Core] Successfully linked AAA plugin: " << api->name << " (Priority: " << api->priority << ")\n";
                return std::make_unique<LoadedPlugin>(handle, api);

            }
            catch (const std::exception& e) {
                std::cerr << "[Core Exception] Plugin loading failed from path " << path << ". Reason: " << e.what() << "\n";
                return nullptr;
            }
        }
    };

} // namespace core

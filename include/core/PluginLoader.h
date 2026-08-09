#pragma once
#include "LibraryLoader.h"
#include "PluginContract.h"
#include <filesystem>
#include <vector>
#include <memory>
#include <algorithm>

namespace core {

    class LoadedPlugin {
    private:
        std::unique_ptr<platform::SharedLibrary> library_;
        PluginInterface* interface_ = nullptr;

    public:
        LoadedPlugin(std::unique_ptr<platform::SharedLibrary> lib, PluginInterface* api)
            : library_(std::move(lib)), interface_(api) {
        }

        PluginInterface* get() const { return interface_; }
    };

    class PluginManager {
    public:
        static std::vector<std::unique_ptr<LoadedPlugin>> discover(const std::string& directory) {
            std::vector<std::unique_ptr<LoadedPlugin>> found;

            if (!std::filesystem::exists(directory)) {
                std::filesystem::create_directories(directory);
                return found;
            }

            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (entry.path().extension() == ".dll" || entry.path().extension() == ".so") {
                    try {
                        auto lib = std::make_unique<platform::SharedLibrary>(entry.path().string());
                        auto get_api = lib->get_function<PluginInterface * (*)()>("GetPluginAPI");
                        PluginInterface* api = get_api();
                        if (api) {
                            found.push_back(std::make_unique<LoadedPlugin>(std::move(lib), api));
                        }
                    }
                    catch (...) { continue; }
                }
            }

            // ИСПРАВЛЕНО: Явное указание типов в лямбде вместо auto
            std::sort(found.begin(), found.end(),
                [](const std::unique_ptr<LoadedPlugin>& a, const std::unique_ptr<LoadedPlugin>& b) {
                    return a->get()->priority < b->get()->priority;
                }
            );

            return found;
        }
    };
}

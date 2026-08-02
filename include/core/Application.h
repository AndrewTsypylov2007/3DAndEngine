// include/core/Application.h — Версия v0.2.0 (Interface-Based Standard)
#pragma once
#include <vector>
#include <string>
#include <memory>
#include <filesystem>
#include "ServiceManager.h"
#include "LibraryLoader.h"

namespace core {
    class Application {
    private:
        ServiceManager m_services;
        std::unique_ptr<LibraryLoader> m_loader;
        std::vector<std::string> m_startPlugins;
        bool m_running = false;

        void mainLoop();
        void discoverPluginsInPath(const std::filesystem::path& searchPath);

    public:
        Application();
        ~Application();

        int run();
        void stop();
    };
}

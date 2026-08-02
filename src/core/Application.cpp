// src/core/Application.cpp — Версия v0.2.0 (Умная слепая загрузка)

#include "../../include/core/Application.h"
#include "../../include/core/EventBus.h"
#include "../../include/core/ConfigService.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace core {

    Application::Application() {
        m_services.registerService(std::make_unique<EventBus>());
        m_services.registerService(std::make_unique<ConfigService>(m_services));
        m_loader = std::make_unique<LibraryLoader>(m_services);
    }

    Application::~Application() { stop(); }

    void Application::discoverPluginsInPath(const std::filesystem::path& searchPath) {
        try {
            if (!std::filesystem::exists(searchPath)) return;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(searchPath)) {
                if (entry.is_regular_file() && entry.path().extension() == ".dll") {
                    std::string fullPath = entry.path().string();
                    std::string fileName = entry.path().filename().string();

                    // ПРАВИЛО v0.2.0: Пропускаем всё, что не содержит "_plugin" в имени.
                    // Это мгновенно отсечет fmt.dll, spdlog.dll, engine_core_lib и системный мусор Windows!
                    if (fileName.find("_plugin") == std::string::npos) {
                        continue;
                    }

                    // Дополнительная защита на всякий случай
                    if (fileName.find("api-ms") != std::string::npos ||
                        fileName.find("vcruntime") != std::string::npos ||
                        fileName.find("3DAndEngine") != std::string::npos ||
                        fileName.find("engine_core") != std::string::npos) {
                        continue;
                    }

                    std::cout << "[Core] Discovery: Attempting to load " << fileName << std::endl;
                    if (m_loader->loadPlugin(fullPath)) {
                        m_startPlugins.push_back(fullPath);
                    }
                }
            }
        }
        catch (...) {}
    }


    int Application::run() {
        std::cout << "[Core] Runtime environment v0.2.0 initialized." << std::endl;
        m_services.startAll();

        // Прочесываем папки рекурсивно на любую глубину (как ты и просил)
        discoverPluginsInPath("plugins");
        if (m_startPlugins.empty()) discoverPluginsInPath(".");

        m_running = true;
        mainLoop();
        return 0;
    }

    void Application::mainLoop() {
        auto* bus = static_cast<EventBus*>(m_services.getServiceByName("EventBus"));
        while (m_running) {
            if (bus) bus->publish("tick");
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    void Application::stop() {
        if (!m_running) return;
        m_running = false;
        for (auto it = m_startPlugins.rbegin(); it != m_startPlugins.rend(); ++it) {
            m_loader->unloadPlugin(*it);
        }
        m_services.stopAll();
    }
}

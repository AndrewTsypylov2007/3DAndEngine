// src/core/Application.cpp — Версия v0.2.0 Stable Sync
#include "../../include/core/Application.h"
#include "../../include/core/ServiceManager.h"
#include "../../include/core/EventBus.h"
#include "../../include/core/LibraryLoader.h"
#include "../../include/core/ConfigService.h"
#include <filesystem>
#include <thread>
#include <iostream>

namespace core {

    // Сигнатура 1: Конструктор
    Application::Application() {
        m_services.registerService(std::make_unique<EventBus>());
        m_services.registerService(std::make_unique<ConfigService>(m_services));
        m_loader = std::make_unique<LibraryLoader>(m_services);
    }

    // Сигнатура 2: Деструктор
    Application::~Application() { 
        stop(); 
    }

    // Сигнатура 3: discoverPluginsInPath с правильным типом std::filesystem::path
    void Application::discoverPluginsInPath(const std::filesystem::path& searchPath) {
        try {
            if (!std::filesystem::exists(searchPath)) return;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(searchPath)) {
                if (entry.is_regular_file() && entry.path().extension() == ".dll") {
                    std::string fullPath = entry.path().string();
                    std::string fileName = entry.path().filename().string();

                    if (fileName.find("api-ms") != std::string::npos || 
                        fileName.find("vcruntime") != std::string::npos ||
                        fileName.find("3DAndEngine") != std::string::npos ||
                        fileName.find("glfw") != std::string::npos ||
                        fileName.find("fmt") != std::string::npos) {
                        continue; 
                    }

                    std::cout << "[Core] Discovery: Attempting to load " << fileName << std::endl;
                    if (m_loader->loadPlugin(fullPath)) {
                        m_startPlugins.push_back(fullPath);
                    }
                }
            }
        } catch (...) {}
    }

    // Сигнатура 4: Точка запуска рантайма
    int Application::run() {
        std::cout << "[Core] Runtime environment v0.2.0 (Multithreaded Fix) initialized." << std::endl;

        discoverPluginsInPath("plugins");
        if (m_startPlugins.empty()) {
            discoverPluginsInPath(".");
        }

        m_running = true;

        std::thread logicThread([this]() {
            std::cout << "[Core] Worker thread for engine logic spawned successfully." << std::endl;
            this->mainLoop();
        });

        m_services.startAll(); 

        std::cout << "[Core] Window closed. Halting background thread..." << std::endl;
        m_running = false; 
        
        if (logicThread.joinable()) {
            logicThread.join();
        }

        stop();
        return 0;
    }

    // Сигнатура 5: Генератор системных тиков
    void Application::mainLoop() {
        auto* bus = static_cast<EventBus*>(m_services.getServiceByName("EventBus"));
        while (m_running) {
            if (bus) {
                bus->publish("tick");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); 
        }
    }

    // Сигнатура 6: Метод принудительной остановки
    void Application::stop() {
        std::cout << "[Core] Stopping engine runtime. Unloading modules..." << std::endl;
        for (auto it = m_startPlugins.rbegin(); it != m_startPlugins.rend(); ++it) {
            m_loader->unloadPlugin(*it);
        }
        m_services.stopAll();
    }
}

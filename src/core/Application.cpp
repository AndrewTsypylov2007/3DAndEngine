// src/core/Application.cpp — Версия v0.2.0 (Исправление ошибок: Многопоточный рантайм)
#include "../../include/core/Application.h"
#include "../../include/core/ServiceManager.h"
#include "../../include/core/EventBus.h"
#include "../../include/core/LibraryLoader.h"
#include "../../include/core/ConfigService.h"
#include <filesystem>
#include <thread>
#include <iostream>

namespace core {

    Application::Application() {
        m_services.registerService(std::make_unique<EventBus>());
        m_services.registerService(std::make_unique<ConfigService>(m_services));
        m_loader = std::make_unique<LibraryLoader>(m_services);
    }

    Application::~Application() {
        stop();
    }

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
        }
        catch (...) {}
    }

    int Application::run() {
        std::cout << "[Core] Runtime environment v0.2.0 (Multithreaded Fix) initialized." << std::endl;

        // 1. Сначала загружаем плагины в адресное пространство процесса
        discoverPluginsInPath("plugins");
        if (m_startPlugins.empty()) {
            discoverPluginsInPath(".");
        }

        m_running = true;

        // 2. АСИНХРОННОСТЬ: Запускаем генератор системных тиков в ОТДЕЛЬНОМ потоке логики!
        // Он будет крутиться на фоне и слать "tick", не блокируя и не подвешивая графику
        std::thread logicThread([this]() {
            std::cout << "[Core] Worker thread for engine logic spawned successfully." << std::endl;
            this->mainLoop();
            });

        // 3. ХИДЖЕКИНГ: Запускаем каскадный старт всех систем в ГЛАВНОМ потоке ОС.
        // Очередь дойдет до WindowPluginService, он заберет этот поток под свой непрерывный 
        // цикл опроса сообщений. На Windows это единственный способ убрать плашку "Не отвечает".
        m_services.startAll();

        // Когда оконный цикл плагина завершится (пользователь закроет окно), управление вернется сюда
        std::cout << "[Core] Window closed. Halting background thread..." << std::endl;
        m_running = false;

        if (logicThread.joinable()) {
            logicThread.join(); // Безопасно дожидаемся остановки потока тиков
        }

        stop();
        return 0;
    }

    void Application::mainLoop() {
        auto* bus = static_cast<EventBus*>(m_services.getServiceByName("EventBus"));
        while (m_running) {
            if (bus) {
                bus->publish("tick");
            }
            // Честная частота обновления логики (60 Гц) вне зависимости от скорости рендеринга!
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    void Application::stop() {
        std::cout << "[Core] Stopping engine runtime. Unloading modules..." << std::endl;
        for (auto it = m_startPlugins.rbegin(); it != m_startPlugins.rend(); ++it) {
            m_loader->unloadPlugin(*it);
        }
        m_services.stopAll();
    }
}

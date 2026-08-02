// src/core/ConfigService.cpp — Версия v0.2.0
#include "../../include/core/ConfigService.h"
#include "../../include/core/ServiceManager.h"
#include <fstream>
#include <iostream>

namespace core {
    ConfigService::ConfigService(ServiceManager& services) : services_(services) {}

    // Исправлено: теперь метод init четко привязан к интерфейсу v0.2.0
    bool ConfigService::init(IServiceManager&) {
        return reloadConfig();
    }

    void ConfigService::start() {}
    void ConfigService::stop() {}

    // ФИКС: Возвращаем полноценную реализацию метода класса!
    bool ConfigService::reloadConfig() {
        std::ifstream file(m_configPath);
        if (!file.is_open()) {
            std::cerr << "[Core::Config] Warning: Cannot open " << m_configPath << std::endl;
            return false;
        }
        try {
            file >> m_currentConfig;
            file.close();
            return true;
        }
        catch (...) {
            std::cerr << "[Core::Config] Critical parsing error in json file." << std::endl;
            return false;
        }
    }

    nlohmann::json ConfigService::getConfig() const {
        return m_currentConfig;
    }
}

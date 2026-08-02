// include/core/ConfigService.h — Версия v0.2.0 (Реализация Ядра)
#pragma once
#include "IService.h"
#include <nlohmann/json.hpp>
#include <string>

namespace core {
    class ServiceManager;

    class ConfigService : public IService {
    private:
        ServiceManager& services_;
        nlohmann::json m_currentConfig;
        std::string m_configPath = "config.json";

    public:
        ConfigService(ServiceManager& services);
        const char* getServiceName() const override { return "ConfigService"; }

        bool init(IServiceManager& services) override;
        void start() override;
        void stop() override;

        bool reloadConfig();
        nlohmann::json getConfig() const;
    };
}

#pragma once
#include "IService.h"

namespace core {
    class LoggerService : public IService {
    public:
        const char* getServiceName() const override { return "LoggerService"; }

        // ФИКС: Используем интерфейс IServiceManager
        bool init(IServiceManager& services) override;
        void start() override;
        void stop() override;
    };
}

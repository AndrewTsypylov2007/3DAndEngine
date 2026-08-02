#pragma once
#include "IService.h"

namespace core {
    class TaskScheduler : public IService {
    public:
        const char* getServiceName() const override { return "TaskScheduler"; }

        // ФИКС: Перевод на интерфейс IServiceManager
        bool init(IServiceManager& services) override;
        void start() override;
        void stop() override;
    };
}

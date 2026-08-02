#include "../../include/core/LoggerService.h"
#include "../../include/core/IServiceManager.h" // Обязательно подключаем чертеж интерфейса
#include <iostream>

namespace core {
    bool LoggerService::init(IServiceManager& services) {
        std::cout << "[Core::Logger] Logger initialized under v0.2.0 standard." << std::endl;
        return true;
    }
    void LoggerService::start() {}
    void LoggerService::stop() {}
}

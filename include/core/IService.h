// include/core/IService.h — Версия v0.2.0
#pragma once

namespace core {
    // ВАЖНО: Опережающее объявление вместо инклюда. 
    // Защищает от циклической зависимости заголовков!
    class IServiceManager;

    class IService {
    public:
        virtual ~IService() = default;
        virtual const char* getServiceName() const = 0;

        virtual bool init(IServiceManager& services) = 0;
        virtual void start() = 0;
        virtual void stop() = 0;
    };
}

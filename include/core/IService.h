// include/core/IService.h — Версия v0.2.0 (Global Type Anchor)
#pragma once
#include <cstdint> // ФИКС ДЛЯ LINUX/macOS: Гарантирует видимость uint64_t во всем движке!

namespace core {

    // Теперь этот тип скомпилируется на любой ОС без ошибок visibility
    using EventHandlerId = uint64_t;

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

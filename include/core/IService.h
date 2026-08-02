// include/core/IService.h — Версия v0.2.0 (Global Type Anchor)
#pragma once

namespace core {

    // Переносим идентификатор сюда. У этого файла ноль инклюдов, 
    // поэтому тип гарантированно скомпилируется ПЕРВЫМ во всем движке!
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

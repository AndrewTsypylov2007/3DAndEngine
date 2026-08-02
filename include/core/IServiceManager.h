// include/core/IServiceManager.h — Версия v0.2.0
#pragma once

namespace core {
    class IService; // Опережающее объявление

    class IServiceManager {
    public:
        virtual ~IServiceManager() = default;
        virtual IService* getServiceByName(const char* name) const = 0;

        template<typename T>
        T* getService(const char* name) const {
            return static_cast<T*>(getServiceByName(name));
        }
    };
}

#pragma once

namespace core {
    class ServiceManager;

    class IService {
    public:
        virtual ~IService() = default;

        // C-String интерфейс для безопасного прохождения через границы DLL
        virtual const char* getServiceName() const = 0;

        virtual bool init(ServiceManager& services) { return true; }
        virtual void start() {}
        virtual void postStart() {}
        virtual void stop() {}
    };
}

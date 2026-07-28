#pragma once
#include <string>

namespace core {
    class ServiceManager;

    class IService {
    public:
        virtual ~IService() = default;

        // Фиксация имени сервиса для надежной связи между EXE и DLL
        virtual std::string getServiceName() const = 0;

        virtual bool init(ServiceManager& services) { return true; }
        virtual void start() {}
        virtual void postStart() {}
        virtual void stop() {}
    };
}

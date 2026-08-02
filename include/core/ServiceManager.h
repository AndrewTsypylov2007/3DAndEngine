// include/core/ServiceManager.h — Версия v0.2.0 (Interface-Based Architecture)
#pragma once
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <shared_mutex>
#include "IServiceManager.h" // Наследуемся от нового интерфейса чертежа
#include "IService.h"

namespace core {

    // ВНИМАНИЕ: Макрос CORE_API полностью удален!
    // Класс скрыт от внешнего линкера, так как плагины будут общаться с ним 
    // только через базовый указатель IServiceManager*
    class ServiceManager : public IServiceManager {
    private:
        std::map<std::string, std::unique_ptr<IService>> services_;
        std::map<std::string, std::vector<std::string>> dependencies_;
        std::vector<std::string> activeOrder_;
        mutable std::shared_mutex rwMutex_;

        void dfs(const std::string& node, std::map<std::string, int>& visited, std::vector<std::string>& order);

    public:
        ServiceManager() = default;
        ~ServiceManager() override { stopAll(); }

        // Реализуем контракт абстрактного интерфейса IServiceManager
        IService* getServiceByName(const char* name) const override;

        // Внутренние методы регистрации (используются только внутри Application.cpp ядра)
        template<typename T>
        void registerService(std::unique_ptr<T> service) {
            std::unique_lock<std::shared_mutex> lock(rwMutex_);
            if (service) {
                services_[service->getServiceName()] = std::move(service);
            }
        }

        template<typename T, typename Dependency>
        void registerService(std::unique_ptr<T> service) {
            std::unique_lock<std::shared_mutex> lock(rwMutex_);
            if (service) {
                std::string name = service->getServiceName();
                services_[name] = std::move(service);
                Dependency depDummy;
                dependencies_[name].push_back(depDummy.getServiceName());
            }
        }

        // Методы управления жизненным циклом (вызывает только Application)
        void startAll();
        void stopAll();
    };
}

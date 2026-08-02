// include/core/ServiceManager.h — Версия v0.2.0 (Crossplatform Sync)
#pragma once
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <shared_mutex>
#include <mutex>             // ФИКС ДЛЯ LINUX: Добавили для работы std::unique_lock!
#include "IServiceManager.h"
#include "IService.h"

namespace core {

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

        IService* getServiceByName(const char* name) const override;

        template<typename T>
        void registerService(std::unique_ptr<T> service) {
            std::unique_lock<std::shared_mutex> lock(rwMutex_); // Теперь Linux понимает этот тип!
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

        void startAll();
        void stopAll();
    };
}

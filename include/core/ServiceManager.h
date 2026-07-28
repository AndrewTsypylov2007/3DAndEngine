#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <typeindex>
#include <mutex>
#include <vector>
#include "IService.h"

namespace core {

    class ServiceManager {
    public:
        ServiceManager() = default;
        ~ServiceManager();

        template<typename T, typename... Deps>
        void registerService(std::shared_ptr<T> svc) {
            std::lock_guard<std::mutex> lk(mutex_);
            services_[std::type_index(typeid(T))] = std::static_pointer_cast<IService>(svc);
            std::vector<std::type_index> dv = { std::type_index(typeid(Deps))... };
            deps_[std::type_index(typeid(T))] = dv;
        }

        template<typename T>
        std::shared_ptr<T> getService() {
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = services_.find(std::type_index(typeid(T)));
            if (it == services_.end()) return {};
            return std::static_pointer_cast<T>(it->second);
        }

        // НОВЫЙ МЕТОД: Поиск по строке для работы с DLL плагинами
        std::shared_ptr<IService> getServiceByName(const std::string& name) {
            std::lock_guard<std::mutex> lk(mutex_);
            for (auto& pair : services_) {
                if (pair.second && pair.second->getServiceName() == name) {
                    return pair.second;
                }
            }
            return nullptr;
        }

        void startAll();
        void stopAll();

    private:
        std::unordered_map<std::type_index, std::shared_ptr<IService>> services_;
        std::unordered_map<std::type_index, std::vector<std::type_index>> deps_;
        std::vector<std::type_index> activeOrder_;
        std::mutex mutex_;
    };
}

// src/core/ServiceManager.cpp — Версия v0.2.0
#include "../../include/core/ServiceManager.h"
#include <stdexcept>
#include <iostream>

namespace core {

    IService* ServiceManager::getServiceByName(const char* name) const {
        std::shared_lock<std::shared_mutex> lock(rwMutex_);
        auto it = services_.find(name);
        return (it != services_.end()) ? it->second.get() : nullptr;
    }

    void ServiceManager::dfs(const std::string& node, std::map<std::string, int>& visited, std::vector<std::string>& order) {
        visited[node] = 1;
        for (const auto& dep : dependencies_[node]) {
            if (visited[dep] == 1) {
                throw std::runtime_error("[Core::Fatal] Cyclic deadlock in ServiceManager!");
            }
            if (visited[dep] == 0) {
                dfs(dep, visited, order);
            }
        }
        visited[node] = 2;
        order.push_back(node);
    }

    void ServiceManager::startAll() {
        std::unique_lock<std::shared_mutex> lock(rwMutex_);
        std::map<std::string, int> visited;
        std::vector<std::string> sortedOrder;

        for (const auto& pair : services_) visited[pair.first] = 0;
        for (const auto& pair : services_) {
            if (visited[pair.first] == 0) dfs(pair.first, visited, sortedOrder);
        }
        activeOrder_ = sortedOrder;

        // ВЫЗЫВАЕМ ТОЛЬКО ДВА МЕТОДА. Жизненный цикл v0.2.0 чист!
        for (const auto& name : activeOrder_) services_[name]->init(*this);
        for (const auto& name : activeOrder_) services_[name]->start();

    }


    void ServiceManager::stopAll() {
        for (auto it = activeOrder_.rbegin(); it != activeOrder_.rend(); ++it) {
            if (services_.find(*it) != services_.end()) services_[*it]->stop();
        }
        services_.clear();
        activeOrder_.clear();
        dependencies_.clear();
    }
}

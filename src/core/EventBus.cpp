// src/core/EventBus.cpp — Версия v0.2.0 (POSIX Standard Fix)
#include "../../include/core/EventBus.h"
#include <algorithm>
#include <mutex>

namespace core {

    EventHandlerId EventBus::subscribe(std::string_view topic, IEventHandler* handler) {
        if (!handler) return 0;
        std::unique_lock<std::shared_mutex> lock(mutex_);
        EventHandlerId id = nextId_++;
        handlers_[std::string(topic)].push_back({ id, handler });
        return id;
    }

    void EventBus::unsubscribe(std::string_view topic, EventHandlerId id) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = handlers_.find(std::string(topic));
        if (it == handlers_.end()) return;

        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(), [id](const Subscription& sub) {
            return sub.id == id;
            }), vec.end());

        if (vec.empty()) handlers_.erase(it);
    }

    void EventBus::publish(std::string_view topic, std::string_view data) {
        std::vector<IEventHandler*> toCall;
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = handlers_.find(std::string(topic));
            if (it != handlers_.end()) {
                toCall.reserve(it->second.size());
                for (const auto& sub : it->second) {
                    toCall.push_back(sub.handler);
                }
            }
        }

        for (auto* handler : toCall) {
            if (handler) handler->onEvent(data);
        }
    }
}

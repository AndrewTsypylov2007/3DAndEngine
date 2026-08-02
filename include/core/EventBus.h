// include/core/EventBus.h — Версия v0.2.0 (POSIX Standard Fix)
#pragma once
#include "IEventBus.h"
#include <map>
#include <string>
#include <vector>
#include <shared_mutex>

namespace core {

    class EventBus : public IEventBus {
    private:
        struct Subscription {
            EventHandlerId id; // Чистый тип из IService
            IEventHandler* handler;
        };

        std::map<std::string, std::vector<Subscription>> handlers_;
        EventHandlerId nextId_ = 1;
        mutable std::shared_mutex mutex_;

    public:
        const char* getServiceName() const override { return "EventBus"; }

        bool init(IServiceManager&) override { return true; }
        void start() override {}
        void stop() override {}

        EventHandlerId subscribe(std::string_view topic, IEventHandler* handler) override;
        void unsubscribe(std::string_view topic, EventHandlerId id) override;
        void publish(std::string_view topic, std::string_view data = "") override;
    };
}

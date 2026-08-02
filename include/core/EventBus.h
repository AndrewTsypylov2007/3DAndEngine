// include/core/EventBus.h — Версия v0.2.0 (Strict POSIX Sync)
#pragma once
#include "IEventBus.h" // Подключает определение EventHandlerId
#include <map>
#include <string>
#include <vector>
#include <shared_mutex>

namespace core {

    class EventBus : public IEventBus {
    private:
        struct Subscription {
            core::EventHandlerId id; // ФИКС: Явно пишем пространство имен
            IEventHandler* handler;
        };

        std::map<std::string, std::vector<Subscription>> handlers_;
        core::EventHandlerId nextId_ = 1; // ФИКС
        mutable std::shared_mutex mutex_;

    public:
        const char* getServiceName() const override { return "EventBus"; }

        bool init(IServiceManager&) override { return true; }
        void start() override {}
        void stop() override {}

        core::EventHandlerId subscribe(std::string_view topic, IEventHandler* handler) override; // ФИКС
        void unsubscribe(std::string_view topic, core::EventHandlerId id) override; // ФИКС
        void publish(std::string_view topic, std::string_view data = "") override;
    };
}

#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cstdint>
#include "IService.h"

namespace core {

    using EventHandlerId = std::uint64_t;

    class EventBus : public IService {
    public:
        EventBus() = default;
        virtual ~EventBus() = default;

        // Реализация методов IService
        const char* getServiceName() const override { return "EventBus"; }
        bool init(ServiceManager& services) override { return true; }

        EventHandlerId subscribe(const std::string& topic, std::function<void()> handler);
        void unsubscribe(const std::string& topic, EventHandlerId id);
        void publish(const std::string& topic);

    private:
        std::mutex mutex_;
        std::unordered_map<std::string, std::vector<std::pair<EventHandlerId, std::function<void()>>>> handlers_;
        EventHandlerId nextId_{ 1 };
    };

}

// include/core/IEventBus.h — Версия v0.2.0 (POSIX Standard Fix)
#pragma once
#include "IService.h" // Тянет EventHandlerId из корня
#include <string_view>

namespace core {

    class IEventHandler {
    public:
        virtual ~IEventHandler() = default;
        virtual void onEvent(std::string_view data) = 0;
    };

    class IEventBus : public IService {
    public:
        // Используем чистый тип без префиксов
        virtual EventHandlerId subscribe(std::string_view topic, IEventHandler* handler) = 0;
        virtual void unsubscribe(std::string_view topic, EventHandlerId id) = 0;
        virtual void publish(std::string_view topic, std::string_view data = "") = 0;
    };
}

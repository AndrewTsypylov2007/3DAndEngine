// include/core/IEventBus.h — Версия v0.2.0 (Чистый C++ Интерфейс)
#pragma once
#include "IService.h"
#include <string_view>

namespace core {
    using EventHandlerId = uint64_t;

    // Класс-слушатель. Плагин наследуется от него для безопасного ABI-вызова
    class IEventHandler {
    public:
        virtual ~IEventHandler() = default;
        virtual void onEvent(std::string_view data) = 0;
    };

    class IEventBus : public IService {
    public:
        virtual EventHandlerId subscribe(std::string_view topic, IEventHandler* handler) = 0;
        virtual void unsubscribe(std::string_view topic, EventHandlerId id) = 0;
        virtual void publish(std::string_view topic, std::string_view data = "") = 0;
    };
}

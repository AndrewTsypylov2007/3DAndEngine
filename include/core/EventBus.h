#pragma once
#include "Types.h"
#include <array>
#include <atomic>

namespace core {

    // Универсальный атомарный пакет события
    struct EventPacket {
        EventId id;
        uint64_t payload; // Сюда можно упаковать как число, так и сырой указатель на структуру данных
    };

    class EventBus {
    private:
        static constexpr size_t MAX_EVENTS = 8192; // Буфер на 8к одновременных событий за 1 кадр
        std::array<EventPacket, MAX_EVENTS> buffer_;
        std::atomic<size_t> count_{ 0 };

    public:
        // Потокобезопасная отправка события из любого рабочего потока без аллокаций в куче
        void broadcast(EventId id, uint64_t payload = 0) {
            size_t index = count_.fetch_add(1, std::memory_order_relaxed);
            if (index < MAX_EVENTS) {
                buffer_[index] = EventPacket{ id, payload };
            }
            // Если буфер переполнен (в продакшене), здесь логируется предупреждение
        }

        // Вызывается ядром строго в конце системного кадра
        void clear() {
            count_.store(0, std::memory_order_release);
        }

        size_t getEventsCount() const {
            size_t current_count = count_.load(std::memory_order_acquire);
            return current_count > MAX_EVENTS ? MAX_EVENTS : current_count;
        }

        const EventPacket* getEventsData() const {
            return buffer_.data();
        }
    };

} // namespace core

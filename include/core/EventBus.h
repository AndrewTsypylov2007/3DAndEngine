#pragma once
#include "Types.h"
#include <array>
#include <atomic>

namespace core {

    struct EventPacket {
        EventId id;
        uint64_t payload;
    };

    class EventBus {
    private:
        static constexpr size_t MAX_EVENTS = 8192;
        std::array<EventPacket, MAX_EVENTS> buffer_;

        // Два независимых атомарных индекса для предотвращения гонок при очистке кадра
        std::atomic<size_t> write_index_{ 0 };
        std::atomic<size_t> ready_count_{ 0 };

    public:
        // Потокобезопасная отправка
        void broadcast(EventId id, uint64_t payload = 0) {
            size_t index = write_index_.fetch_add(1, std::memory_order_relaxed);

            if (index < MAX_EVENTS) {
                buffer_[index] = EventPacket{ id, payload };

                // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Гарантируем, что данные физически записаны в buffer_ 
                // до того, как инкрементируется счетчик готовых к чтению событий.
                ready_count_.fetch_add(1, std::memory_order_release);
            }
            else {
                // Предотвращаем бесконечный рост счетчика при переполнении
                write_index_.store(MAX_EVENTS, std::memory_order_relaxed);
            }
        }

        // Вызывается строго в конце кадра, когда все параллельные задачи (Jobs) ГАРАНТИРОВАННО завершены
        void clear() {
            // Перед очисткой ядро обязано сделать синхронизацию (join) всех потоков JobSystem!
            write_index_.store(0, std::memory_order_relaxed);
            ready_count_.store(0, std::memory_order_release);
        }

        // Потокобезопасное чтение
        size_t getEventsCount() const {
            // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Читаем с барьером acquire.
            // Процессор гарантирует: всё, что было записано до ready_count_.fetch_add, теперь видно здесь.
            size_t current_count = ready_count_.load(std::memory_order_acquire);
            return current_count > MAX_EVENTS ? MAX_EVENTS : current_count;
        }

        const EventPacket* getEventsData() const {
            return buffer_.data();
        }
    };

} // namespace core

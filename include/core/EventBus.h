#pragma once
#include "Types.h"
#include <vector>
#include <atomic>
#include <functional>
#include <shared_mutex>

namespace core {

    struct EventPacket {
<<<<<<< HEAD
        EventId id = 0;
        uint64_t payload = 0;
=======
        EventId id;
        uint64_t payload;
>>>>>>> 28d53b185e478edcb19a5b99a606b2a5a10e3a5a
    };

    using EventCallback = std::function<void(uint64_t)>;

    class EventBus {
    private:
        static constexpr size_t MAX_EVENTS = 8192;
<<<<<<< HEAD
        // ПЕРЕНОС В КУЧУ: Используем вектор, чтобы не раздувать стек
        std::vector<EventPacket> buffer_;

        std::atomic<size_t> write_index_{ 0 };
        std::atomic<size_t> ready_count_{ 0 };

        struct Listener {
            EventId id;
            EventCallback callback;
        };
        std::vector<Listener> listeners_;
        mutable std::shared_mutex listeners_mutex_;

    public:
        // Явный конструктор с инициализацией (исправляет type.6)
        EventBus() {
            buffer_.resize(MAX_EVENTS);
        }

        void broadcast(EventId id, uint64_t payload = 0) {
            {
                std::shared_lock lock(listeners_mutex_);
                for (const auto& listener : listeners_) {
                    if (listener.id == id) listener.callback(payload);
                }
            }

            size_t index = write_index_.fetch_add(1, std::memory_order_relaxed);
            if (index < MAX_EVENTS) {
                buffer_[index] = { id, payload };
                ready_count_.fetch_add(1, std::memory_order_release);
            }
        }

        void subscribe(EventId id, EventCallback callback) {
            std::unique_lock lock(listeners_mutex_);
            listeners_.push_back({ id, std::move(callback) });
        }

        void clear() {
=======
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
            } else {
                // Предотвращаем бесконечный рост счетчика при переполнении
                write_index_.store(MAX_EVENTS, std::memory_order_relaxed);
            }
        }

        // Вызывается строго в конце кадра, когда все параллельные задачи (Jobs) ГАРАНТИРОВАННО завершены
        void clear() {
            // Перед очисткой ядро обязано сделать синхронизацию (join) всех потоков JobSystem!
>>>>>>> 28d53b185e478edcb19a5b99a606b2a5a10e3a5a
            write_index_.store(0, std::memory_order_relaxed);
            ready_count_.store(0, std::memory_order_release);
        }

        // Потокобезопасное чтение
        size_t getEventsCount() const {
<<<<<<< HEAD
            size_t current = ready_count_.load(std::memory_order_acquire);
            return (current > MAX_EVENTS) ? MAX_EVENTS : current;
=======
            // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Читаем с барьером acquire.
            // Процессор гарантирует: всё, что было записано до ready_count_.fetch_add, теперь видно здесь.
            size_t current_count = ready_count_.load(std::memory_order_acquire);
            return current_count > MAX_EVENTS ? MAX_EVENTS : current_count;
>>>>>>> 28d53b185e478edcb19a5b99a606b2a5a10e3a5a
        }

        const EventPacket* getEventsData() const {
            return buffer_.data();
        }
    };
}

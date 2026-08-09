#pragma once
#include "Types.h"
#include <vector>
#include <atomic>
#include <functional>
#include <shared_mutex>

namespace core {

    struct EventPacket {
        EventId id = 0;
        uint64_t payload = 0;
    };

    using EventCallback = std::function<void(uint64_t)>;

    class EventBus {
    private:
        static constexpr size_t MAX_EVENTS = 8192;
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
            write_index_.store(0, std::memory_order_relaxed);
            ready_count_.store(0, std::memory_order_release);
        }

        size_t getEventsCount() const {
            size_t current = ready_count_.load(std::memory_order_acquire);
            return (current > MAX_EVENTS) ? MAX_EVENTS : current;
        }

        const EventPacket* getEventsData() const {
            return buffer_.data();
        }
    };
}

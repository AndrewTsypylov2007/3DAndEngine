#pragma once

#include <vector>
#include <unordered_map>
#include <functional>
#include <shared_mutex>
#include <mutex>
#include <cstdint>
#include <atomic>
#include "Types.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324)
#endif

namespace core {

    class EventBus {
    public:
        using EventCallback = std::function<void(uint64_t)>;

    private:
        struct BufferedEvent {
            EventId  id{ 0 };
            uint64_t payload{ 0 };
        };

        std::unordered_map<EventId, std::vector<EventCallback>> subscribers_;
        mutable std::shared_mutex                               subscribers_mutex_;

        std::vector<BufferedEvent> buffer_a_;
        std::vector<BufferedEvent> buffer_b_;
        std::vector<BufferedEvent>* active_write_buffer_{ &buffer_a_ };
        std::vector<BufferedEvent>* active_read_buffer_{ &buffer_b_ };
        std::mutex                  buffer_mutex_;

        std::atomic<size_t> event_count_{ 0 };

    public:
        EventBus() {
            buffer_a_.reserve(256);
            buffer_b_.reserve(256);
        }

        ~EventBus() {
            unsubscribeAll();
        }

        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;

        // БЕЗОПАСНАЯ НЕМЕДЛЕННАЯ РАССЫЛКА (Копирует список коллбеков для защиты от рекурсивной мутации вектора)
        void dispatchImmediate(EventId id, uint64_t payload = 0) {
            event_count_.fetch_add(1, std::memory_order_relaxed);

            std::vector<EventCallback> callbacks_snapshot;
            {
                std::shared_lock lock(subscribers_mutex_);
                auto it = subscribers_.find(id);
                if (it != subscribers_.end()) {
                    callbacks_snapshot = it->second; // Снимок защищает от повреждения итератора
                }
            }

            for (const auto& cb : callbacks_snapshot) {
                if (cb) {
                    try { cb(payload); }
                    catch (...) {}
                }
            }
        }

        void publish(EventId id, uint64_t payload = 0) {
            event_count_.fetch_add(1, std::memory_order_relaxed);
            std::lock_guard lock(buffer_mutex_);
            active_write_buffer_->push_back({ id, payload });
        }

        void subscribe(EventId id, EventCallback callback) {
            if (!callback) return;
            std::unique_lock lock(subscribers_mutex_);
            subscribers_[id].push_back(std::move(callback));
        }

        void processEvents() {
            std::vector<BufferedEvent> events_to_process;
            {
                std::lock_guard lock(buffer_mutex_);
                if (!active_write_buffer_->empty()) {
                    events_to_process.swap(*active_write_buffer_);
                }
            }

            if (events_to_process.empty()) return;

            for (const auto& evt : events_to_process) {
                dispatchImmediate(evt.id, evt.payload);
            }
        }

        void clear() {
            processEvents();
        }

        void unsubscribeAll() {
            std::unique_lock lock(subscribers_mutex_);
            subscribers_.clear();
            std::lock_guard lock2(buffer_mutex_);
            buffer_a_.clear();
            buffer_b_.clear();
        }

        size_t eventCount() const {
            return event_count_.load(std::memory_order_relaxed);
        }
    };

} // namespace core

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
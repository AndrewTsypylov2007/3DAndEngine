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
#pragma warning(disable: 4324) // Подавление информационного варнинга о padding для alignas(64)
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

        // Потокобезопасная таблица подписчиков
        std::unordered_map<EventId, std::vector<EventCallback>> subscribers_;
        mutable std::shared_mutex                               subscribers_mutex_;

        // Двойная буферизация для событий кадра (Double-Buffered Event Queue)
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

        ~EventBus() = default;
        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;

        // Немедленная рассылка (Immediate Dispatch)
        void dispatchImmediate(EventId id, uint64_t payload = 0) {
            event_count_.fetch_add(1, std::memory_order_relaxed);
            std::shared_lock lock(subscribers_mutex_);
            auto it = subscribers_.find(id);
            if (it != subscribers_.end()) {
                for (const auto& cb : it->second) {
                    if (cb) cb(payload);
                }
            }
        }

        // Буферизованная публикация события кадра (Thread-safe Frame Publish)
        void publish(EventId id, uint64_t payload = 0) {
            event_count_.fetch_add(1, std::memory_order_relaxed);
            std::lock_guard lock(buffer_mutex_);
            active_write_buffer_->push_back({ id, payload });
        }

        // Подписка на событие
        void subscribe(EventId id, EventCallback callback) {
            std::unique_lock lock(subscribers_mutex_);
            subscribers_[id].push_back(std::move(callback));
        }

        // Обработка всех накопленных за кадр событий
        void processEvents() {
            std::vector<BufferedEvent>* read_buf = nullptr;

            {
                std::lock_guard lock(buffer_mutex_);
                read_buf = active_write_buffer_;
                active_write_buffer_ = active_read_buffer_;
                active_read_buffer_ = read_buf;
                active_write_buffer_->clear();
            }

            if (read_buf && !read_buf->empty()) {
                std::shared_lock lock(subscribers_mutex_);
                for (const auto& evt : *read_buf) {
                    auto it = subscribers_.find(evt.id);
                    if (it != subscribers_.end()) {
                        for (const auto& cb : it->second) {
                            if (cb) cb(evt.payload);
                        }
                    }
                }
            }
        }

        void clear() {
            processEvents();
        }

        void unsubscribeAll() {
            std::unique_lock lock(subscribers_mutex_);
            subscribers_.clear();
        }

        size_t eventCount() const {
            return event_count_.load(std::memory_order_relaxed);
        }
    };

} // namespace core

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#pragma once

#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <cstdint>
#include <atomic>
#include "Types.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324) // Подавление информационного варнинга C4324 о padding для alignas(64)
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

        // Таблица подписчиков, защищенная рекурсивным мьютексом
        std::unordered_map<EventId, std::vector<EventCallback>> subscribers_;
        mutable std::recursive_mutex                            subscribers_mutex_;

        // Буфер отложенных событий текущего кадра
        std::vector<BufferedEvent>                              write_buffer_;
        mutable std::recursive_mutex                            buffer_mutex_;

        std::atomic<size_t>                                     event_count_{ 0 };

    public:
        EventBus() {
            write_buffer_.reserve(256);
        }

        ~EventBus() {
            unsubscribeAll();
        }

        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;

        // Немедленная рассылка события (Immediate Dispatch с защитой от инвалидации итераторов)
        void dispatchImmediate(EventId id, uint64_t payload = 0) {
            event_count_.fetch_add(1, std::memory_order_relaxed);

            std::vector<EventCallback> callbacks_copy;
            {
                std::lock_guard<std::recursive_mutex> lock(subscribers_mutex_);
                auto it = subscribers_.find(id);
                if (it != subscribers_.end()) {
                    callbacks_copy = it->second;
                }
            }

            for (const auto& cb : callbacks_copy) {
                if (cb) {
                    try {
                        cb(payload);
                    }
                    catch (...) {
                        // Защита главного потока от исключений внутри пользовательских плагинов
                    }
                }
            }
        }

        // Потокобезопасная буферизованная публикация события
        void publish(EventId id, uint64_t payload = 0) {
            event_count_.fetch_add(1, std::memory_order_relaxed);
            std::lock_guard<std::recursive_mutex> lock(buffer_mutex_);
            write_buffer_.push_back({ id, payload });
        }

        // Подписка на событие
        void subscribe(EventId id, EventCallback callback) {
            if (!callback) return;
            std::lock_guard<std::recursive_mutex> lock(subscribers_mutex_);
            subscribers_[id].push_back(std::move(callback));
        }

        // Обработка всех накопленных событий кадра (Swap Snapshot Pattern)
        void processEvents() {
            std::vector<BufferedEvent> events_snapshot;
            {
                std::lock_guard<std::recursive_mutex> lock(buffer_mutex_);
                if (!write_buffer_.empty()) {
                    events_snapshot.swap(write_buffer_);
                }
            }

            if (events_snapshot.empty()) return;

            for (const auto& evt : events_snapshot) {
                dispatchImmediate(evt.id, evt.payload);
            }
        }

        void clear() {
            processEvents();
        }

        void unsubscribeAll() {
            std::lock_guard<std::recursive_mutex> lock1(subscribers_mutex_);
            subscribers_.clear();
            std::lock_guard<std::recursive_mutex> lock2(buffer_mutex_);
            write_buffer_.clear();
        }

        size_t eventCount() const {
            return event_count_.load(std::memory_order_relaxed);
        }
    };

} // namespace core

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
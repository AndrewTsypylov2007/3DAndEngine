#pragma once

#include "Types.h"
#include <vector>
#include <atomic>
#include <functional>
#include <shared_mutex>
#include <mutex>
#include <cstdint>
#include <algorithm>

namespace core {

    // DOD-пакет события кадра (C-ABI совместимый, 16 байт POD)
    struct EventPacket {
        EventId  id = 0;
        uint64_t payload = 0;
    };

    // Сигнатура обработчиков событий
    using EventCallback = std::function<void(uint64_t)>;
    using SubscriptionId = uint64_t;

    class EventBus {
    public:
        static constexpr size_t MAX_EVENTS = 8192;

    private:
        // Пакетный буфер кадра
        std::vector<EventPacket> buffer_;

        // Атомарные счетчики кольцевого буфера кадра (разнесены по 64-байтовым кэш-линиям)
        alignas(64) std::atomic<size_t> write_index_{ 0 };
        alignas(64) std::atomic<size_t> ready_count_{ 0 };

        // Реестр синхронных слушателей
        struct Listener {
            SubscriptionId id_token;
            EventId        event_id;
            EventCallback  callback;
        };

        std::vector<Listener>     listeners_;
        std::atomic<SubscriptionId> next_token_{ 1 };
        mutable std::shared_mutex listeners_mutex_;

    public:
        EventBus() {
            buffer_.resize(MAX_EVENTS);
        }

        ~EventBus() {
            unsubscribeAll();
        }

        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;

        /**
         * @brief Потокобезопасная отправка события (Мгновенное прерывание + Запись в буфер кадра)
         */
        void broadcast(EventId id, uint64_t payload = 0) {
            // -------------------------------------------------------------
            // 1. МГНОВЕННАЯ СИНХРОННАЯ РЕАКЦИЯ (С защитой от Reentrancy Deadlock)
            // -------------------------------------------------------------
            // Быстро копируем подходящие коллбэки под shared_lock в локальный буфер,
            // чтобы освободить мьютекс ДО фактического вызова функции пользователя.
            std::vector<EventCallback> targets;
            {
                std::shared_lock lock(listeners_mutex_);
                for (const auto& listener : listeners_) {
                    if (listener.event_id == id) {
                        targets.push_back(listener.callback);
                    }
                }
            }

            // Вызываем обработчики вне зоны действия lock — теперь слушатель 
            // может безопасно вызывать subscribe/unsubscribe/broadcast без дедлока!
            for (auto& cb : targets) {
                if (cb) {
                    cb(payload);
                }
            }

            // -------------------------------------------------------------
            // 2. ОТЛОЖЕННАЯ ЗАПИСЬ В БУФЕР КАДРА
            // -------------------------------------------------------------
            size_t index = write_index_.fetch_add(1, std::memory_order_relaxed);
            if (index < MAX_EVENTS) {
                buffer_[index] = { id, payload };
                // release барьер: гарантирует видимость данных пакета до инкремента ready_count_
                ready_count_.fetch_add(1, std::memory_order_release);
            }
            else {
                write_index_.store(MAX_EVENTS, std::memory_order_relaxed);
            }
        }

        /**
         * @brief Подписка на мгновенные события. Возвращает токен для отписки.
         */
        SubscriptionId subscribe(EventId id, EventCallback callback) {
            SubscriptionId token = next_token_.fetch_add(1, std::memory_order_relaxed);
            std::unique_lock lock(listeners_mutex_);
            listeners_.push_back({ token, id, std::move(callback) });
            return token;
        }

        /**
         * @brief Точечная отписка по токену подписки
         */
        void unsubscribe(SubscriptionId token) {
            std::unique_lock lock(listeners_mutex_);
            auto it = std::remove_if(listeners_.begin(), listeners_.end(),
                [token](const Listener& l) { return l.id_token == token; });
            listeners_.erase(it, listeners_.end());
        }

        /**
         * @brief Сброс буфера кадра (вызывается в конце игрового цикла кадра)
         */
        void clear() {
            write_index_.store(0, std::memory_order_relaxed);
            ready_count_.store(0, std::memory_order_release);
        }

        /**
         * @brief Количество событий в буфере текущего кадра
         */
        size_t getEventsCount() const {
            size_t count = ready_count_.load(std::memory_order_acquire);
            return (count > MAX_EVENTS) ? MAX_EVENTS : count;
        }

        /**
         * @brief Прямой доступ к сырому массиву пакетов кадра (DOD)
         */
        const EventPacket* getEventsData() const {
            return buffer_.data();
        }

        /**
         * @brief Итерация по всем событиям заданного типа за текущий кадр
         */
        template<typename Func>
        void consumeEvents(EventId target_id, Func&& func) const {
            size_t count = getEventsCount();
            for (size_t i = 0; i < count; ++i) {
                if (buffer_[i].id == target_id) {
                    func(buffer_[i].payload);
                }
            }
        }

        /**
         * @brief Принудительное удаление всех слушателей (при остановке ядра)
         */
        void unsubscribeAll() {
            std::unique_lock lock(listeners_mutex_);
            listeners_.clear();
        }
    };

} // namespace core
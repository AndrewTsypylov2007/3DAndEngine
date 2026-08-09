#pragma once
#include "Types.h"
#include <vector>
#include <atomic>
#include <functional>
#include <shared_mutex>

namespace core {

    // DOD-пакет события кадра (C-ABI совместимый)
    struct EventPacket {
        EventId  id = 0;
        uint64_t payload = 0;
    };

    // Сигнатура подписчиков для мгновенных синхронных прерываний
    using EventCallback = std::function<void(uint64_t)>;

    class EventBus {
    private:
        static constexpr size_t MAX_EVENTS = 8192;

        // ПЕРЕНОС В КУЧУ: Защищает от переполнения стека (исправляет ошибку 131136 байт)
        std::vector<EventPacket> buffer_;

        // Независимые атомарные счетчики кольцевого буфера
        std::atomic<size_t> write_index_{ 0 };
        std::atomic<size_t> ready_count_{ 0 };

        // Реестр мгновенных слушателей (v0.3.0)
        struct Listener {
            EventId       id;
            EventCallback callback;
        };
        std::vector<Listener>     listeners_;
        mutable std::shared_mutex listeners_mutex_; // Защита Read-Write Lock для многопоточности

    public:
        // Явный конструктор с предвыделением памяти (исправляет предупреждение type.6)
        EventBus() {
            buffer_.resize(MAX_EVENTS);
        }

        ~EventBus() {
            unsubscribeAll();
        }

        // Запрещаем копирование тяжелой шины событий кадра
        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;

        /**
         * @brief Потокобезопасная отправка события кадра (Гибридная модель)
         */
        void broadcast(EventId id, uint64_t payload = 0) {
            // 1. Мгновенная синхронная реакция (Слушатели реагируют в ту же микросекунду)
            {
                std::shared_lock lock(listeners_mutex_);
                for (const auto& listener : listeners_) {
                    if (listener.id == id) {
                        listener.callback(payload);
                    }
                }
            }

            // 2. Отложенная запись в кольцевой буфер (Для пассивной логики кадра)
            size_t index = write_index_.fetch_add(1, std::memory_order_relaxed);
            if (index < MAX_EVENTS) {
                buffer_[index] = { id, payload };

                // Барьер release гарантирует: данные физически попали в buffer_ до инкремента счетчика кадра
                ready_count_.fetch_add(1, std::memory_order_release);
            }
            else {
                // Защита от бесконечного инкремента при переполнении буфера
                write_index_.store(MAX_EVENTS, std::memory_order_relaxed);
            }
        }

        /**
         * @brief Подписка на мгновенные реактивные события (Критично для ввода и команд ОС)
         */
        void subscribe(EventId id, EventCallback callback) {
            std::unique_lock lock(listeners_mutex_);
            listeners_.push_back({ id, std::move(callback) });
        }

        /**
         * @brief Сброс буфера кадра (Вызывается строго в конце кадра ядром)
         */
        void clear() {
            write_index_.store(0, std::memory_order_relaxed);
            ready_count_.store(0, std::memory_order_release);
        }

        /**
         * @brief Потокобезопасное чтение количества накопленных отложенных событий
         */
        size_t getEventsCount() const {
            // Барьер acquire гарантирует атомарную видимость данных из других потоков
            size_t current_count = ready_count_.load(std::memory_order_acquire);
            return (current_count > MAX_EVENTS) ? MAX_EVENTS : current_count;
        }

        /**
         * @brief Получить доступ к сырому массиву данных для пакетной DOD-обработки
         */
        const EventPacket* getEventsData() const {
            return buffer_.data();
        }

        /**
         * @brief Принудительное удаление всех подписчиков для безопасной выгрузки плагинов
         */
        void unsubscribeAll() {
            std::unique_lock lock(listeners_mutex_);
            listeners_.clear();
        }
    };

} // namespace core

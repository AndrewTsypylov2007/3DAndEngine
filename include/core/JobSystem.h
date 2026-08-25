#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <cstdint>
#include <cassert>
#include <chrono>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__i386__) || defined(__x86_64__)
#include <immintrin.h>
#endif

namespace core {

    inline void cpu_pause() noexcept {
#if defined(_MSC_VER) || defined(__i386__) || defined(__x86_64__)
        _mm_pause();
#elif defined(__arm__) || defined(__aarch64__)
        asm volatile("yield" ::: "memory");
#else
        std::this_thread::yield();
#endif
    }

    // ==============================================================================
    // СЧЕТЧИК СИНХРОНИЗАЦИИ (Counter-based synchronization)
    // ==============================================================================
    struct JobCounter {
        std::atomic<uint32_t> remaining{ 0 };
    };

    struct Job {
        void (*function)(void*) = nullptr;
        void* data = nullptr;
        JobCounter* counter = nullptr;
    };

    // ==============================================================================
    // LOCK-FREE BOUNDED MPMC ОЧЕРЕДЬ (Dmitry Vyukov Algorithm)
    // ==============================================================================
    template<typename T, size_t Capacity>
    class MpmcBoundedQueue {
        static_assert((Capacity >= 2) && ((Capacity& (Capacity - 1)) == 0),
            "Размер очереди Capacity обязан быть степенью двойки!");

    private:
        struct Node {
            std::atomic<size_t> sequence;
            T data;
        };

        static constexpr size_t BufferMask = Capacity - 1;
        Node buffer_[Capacity];

        // Разделяем счетчики по разным кэш-линиям (64 байта) для исключения False Sharing
        alignas(64) std::atomic<size_t> enqueue_pos_{ 0 };
        alignas(64) std::atomic<size_t> dequeue_pos_{ 0 };

    public:
        MpmcBoundedQueue() {
            for (size_t i = 0; i < Capacity; ++i) {
                buffer_[i].sequence.store(i, std::memory_order_relaxed);
            }
        }

        ~MpmcBoundedQueue() = default;
        MpmcBoundedQueue(const MpmcBoundedQueue&) = delete;
        MpmcBoundedQueue& operator=(const MpmcBoundedQueue&) = delete;

        bool push(const T& item) {
            size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
            for (;;) {
                Node& node = buffer_[pos & BufferMask];
                size_t seq = node.sequence.load(std::memory_order_acquire);
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

                if (diff == 0) {
                    if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        node.data = item;
                        // Барьер release гарантирует: данные записаны до обновления sequence
                        node.sequence.store(pos + 1, std::memory_order_release);
                        return true;
                    }
                }
                else if (diff < 0) {
                    // Очередь заполнена
                    return false;
                }
                else {
                    pos = enqueue_pos_.load(std::memory_order_relaxed);
                }
            }
        }

        bool pop(T& item) {
            size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
            for (;;) {
                Node& node = buffer_[pos & BufferMask];
                size_t seq = node.sequence.load(std::memory_order_acquire);
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

                if (diff == 0) {
                    if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        item = node.data;
                        // Освобождаем ячейку для следующего цикла записи
                        node.sequence.store(pos + BufferMask + 1, std::memory_order_release);
                        return true;
                    }
                }
                else if (diff < 0) {
                    // Очередь пуста
                    return false;
                }
                else {
                    pos = dequeue_pos_.load(std::memory_order_relaxed);
                }
            }
        }
    };

    // ==============================================================================
    // ВЫСОКОПРОИЗВОДИТЕЛЬНАЯ СИСТЕМА ДЖОБОВ (v0.3.5 Commercial Standard)
    // ==============================================================================
    class JobSystem {
    private:
        static constexpr size_t QUEUE_CAPACITY = 4096;
        MpmcBoundedQueue<Job, QUEUE_CAPACITY> queue_;

        std::vector<std::thread> workers_;
        std::atomic<bool>        running_{ false };
        std::atomic<uint32_t>    active_jobs_{ 0 };

        std::mutex               cv_mutex_;
        std::condition_variable  cv_;

    public:
        JobSystem() = default;

        ~JobSystem() {
            shutdown();
        }

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;

        void initialize(uint32_t user_thread_count = 0) {
            if (running_.load(std::memory_order_acquire)) return;
            running_.store(true, std::memory_order_release);

            uint32_t threads_count = user_thread_count;
            if (threads_count == 0) {
                threads_count = std::thread::hardware_concurrency();
                // 1 ядро под ОС, 1 ядро под Main Thread движка
                if (threads_count > 2) threads_count -= 2;
                if (threads_count == 0) threads_count = 1;
            }

            workers_.reserve(threads_count);
            for (uint32_t i = 0; i < threads_count; ++i) {
                workers_.emplace_back([this]() {
                    workerLoop();
                    });
            }
        }

        /**
         * @brief Добавить задачу в систему (Потокобезопасно из любого потока)
         */
        bool push(void (*func)(void*), void* data = nullptr, JobCounter* counter = nullptr) {
            if (!func) return false;

            if (counter) {
                counter->remaining.fetch_add(1, std::memory_order_relaxed);
            }
            active_jobs_.fetch_add(1, std::memory_order_relaxed);

            Job job{ func, data, counter };
            if (!queue_.push(job)) {
                // Если очередь переполнена, отменяем инкременты
                if (counter) {
                    counter->remaining.fetch_sub(1, std::memory_order_relaxed);
                }
                active_jobs_.fetch_sub(1, std::memory_order_relaxed);
                return false;
            }

            // Будим одного воркера
            cv_.notify_one();
            return true;
        }

        /**
         * @brief Ожидание завершения группы задач с помощью текущего потока (Work Assisting)
         */
        void wait(const JobCounter* counter) {
            if (!counter) return;

            while (counter->remaining.load(std::memory_order_acquire) > 0) {
                Job job;
                if (queue_.pop(job)) {
                    execute(job);
                }
                else {
                    // Короткая пауза для снятия нагрузки на шину памяти
                    std::this_thread::yield();
                }
            }
        }

        /**
         * @brief Ожидание завершения вообще всех задач в системе
         */
        void waitAll() {
            while (active_jobs_.load(std::memory_order_acquire) > 0) {
                Job job;
                if (queue_.pop(job)) {
                    execute(job);
                }
                else {
                    std::this_thread::yield();
                }
            }
        }

        void shutdown() {
            if (!running_.load(std::memory_order_acquire)) return;

            running_.store(false, std::memory_order_release);
            cv_.notify_all(); // Будим всех воркеров для безопасного выхода

            for (auto& worker : workers_) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
            workers_.clear();
        }

        uint32_t getActiveJobsCount() const {
            return active_jobs_.load(std::memory_order_relaxed);
        }

    private:
        void execute(const Job& job) {
            if (job.function) {
                job.function(job.data);
            }
            if (job.counter) {
                job.counter->remaining.fetch_sub(1, std::memory_order_release);
            }
            active_jobs_.fetch_sub(1, std::memory_order_relaxed);
        }

        void workerLoop() {
            while (running_.load(std::memory_order_acquire)) {
                Job job;
                if (queue_.pop(job)) {
                    execute(job);
                }
                else {
                    bool found = false;
                    for (int spin = 0; spin < 64; ++spin) {
                        if (queue_.pop(job)) {
                            execute(job);
                            found = true;
                            break;
                        }
                        // ИСПРАВЛЕНО: Безопасный вызов на любой платформе (x86/x64/ARM/Linux/Windows)
                        core::cpu_pause();
                    }

                    if (found) continue;

                    std::unique_lock<std::mutex> lock(cv_mutex_);
                    cv_.wait_for(lock, std::chrono::milliseconds(2), [this]() {
                        return !running_.load(std::memory_order_relaxed) || (active_jobs_.load(std::memory_order_relaxed) > 0);
                        });
                }
            }
        }
    };

} // namespace core
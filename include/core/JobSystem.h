#pragma once
#include <atomic>
#include <thread>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <functional>

namespace core {

    // Группа задач для отслеживания завершения (Counter-based synchronization)
    struct JobCounter {
        std::atomic<uint32_t> remaining{ 0 };
    };

    struct Job {
        void (*function)(void*) = nullptr;
        void* data = nullptr;
        JobCounter* counter = nullptr; // Опциональный счетчик для синхронизации
    };

    class JobSystem {
    private:
        std::vector<std::thread> workers_;
        std::atomic<bool> running_{ false };

        // Использование кольцевого буфера с блокировкой на условной переменной 
        // для предотвращения 100% загрузки CPU в режиме ожидания
        static constexpr size_t RING_SIZE = 4096;
        Job queue_[RING_SIZE];
        std::atomic<size_t> head_{ 0 };
        std::atomic<size_t> tail_{ 0 };

        std::mutex mutex_;
        std::condition_variable cv_;
        std::atomic<uint32_t> active_jobs_{ 0 };

    public:
        void initialize() {
            if (running_.load()) return;
            running_.store(true, std::memory_order_release);

            unsigned int threads_count = std::thread::hardware_concurrency();
            // Оставляем одно ядро под ОС и одно под главный поток движка
            if (threads_count > 2) threads_count -= 2;
            if (threads_count == 0) threads_count = 1;

            for (unsigned int i = 0; i < threads_count; ++i) {
                workers_.emplace_back([this]() {
                    while (running_.load(std::memory_order_acquire)) {
                        Job job;
                        if (pop(job)) {
                            execute(job);
                        }
                        else {
                            // Вместо yield используем ожидание на CV (экономия энергии и ресурсов)
                            std::unique_lock<std::mutex> lock(mutex_);
                            cv_.wait_for(lock, std::chrono::milliseconds(1), [this] {
                                return tail_.load() != head_.load() || !running_.load();
                                });
                        }
                    }
                    });
            }
        }

        // Выполнение задачи с обработкой счетчика
        void execute(const Job& job) {
            if (job.function) {
                job.function(job.data);
            }
            if (job.counter) {
                job.counter->remaining.fetch_sub(1, std::memory_order_release);
            }
            active_jobs_.fetch_sub(1, std::memory_order_relaxed);
        }

        // Добавление задачи в очередь
        bool push(void (*func)(void*), void* data = nullptr, JobCounter* counter = nullptr) {
            size_t current_tail = tail_.load(std::memory_order_relaxed);
            size_t next_tail = (current_tail + 1) % RING_SIZE;

            if (next_tail == head_.load(std::memory_order_acquire)) {
                return false; // Очередь переполнена
            }

            if (counter) counter->remaining.fetch_add(1, std::memory_order_relaxed);
            active_jobs_.fetch_add(1, std::memory_order_relaxed);

            queue_[current_tail] = { func, data, counter };
            tail_.store(next_tail, std::memory_order_release);

            cv_.notify_one(); // Пробуждаем одного свободного воркера
            return true;
        }

        // Ожидание завершения конкретной группы задач
        void wait(const JobCounter* counter) {
            if (!counter) return;
            while (counter->remaining.load(std::memory_order_acquire) > 0) {
                // Пока ждем, текущий поток (Main Thread) тоже помогает выполнять работу
                Job job;
                if (pop(job)) {
                    execute(job);
                }
                else {
                    std::this_thread::yield();
                }
            }
        }

        bool pop(Job& job) {
            size_t current_head = head_.load(std::memory_order_relaxed);
            if (current_head == tail_.load(std::memory_order_acquire)) {
                return false;
            }
            job = queue_[current_head];
            head_.store((current_head + 1) % RING_SIZE, std::memory_order_release);
            return true;
        }

        void shutdown() {
            running_.store(false, std::memory_order_release);
            cv_.notify_all(); // Будим всех для выхода

            for (auto& worker : workers_) {
                if (worker.joinable()) worker.join();
            }
            workers_.clear();
        }

        uint32_t getActiveJobsCount() const {
            return active_jobs_.load(std::memory_order_relaxed);
        }
    };

} // namespace core

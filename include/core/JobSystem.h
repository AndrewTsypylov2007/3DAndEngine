#pragma once
#include <atomic>
#include <thread>
#include <vector>

namespace core {

    struct Job {
        void (*function)(void*) = nullptr;
        void* data = nullptr;
    };

    class JobSystem {
    private:
        std::vector<std::thread> workers_;
        std::atomic<bool> running_{ false };

        static constexpr size_t RING_SIZE = 1048;
        Job queue_[RING_SIZE];
        std::atomic<size_t> head_{ 0 };
        std::atomic<size_t> tail_{ 0 };

    public:
        void initialize() {
            // ИСПРАВЛЕНО: Явно публикуем состояние true с барьером release
            running_.store(true, std::memory_order_release);

            unsigned int threads_count = std::thread::hardware_concurrency();
            if (threads_count > 2) threads_count -= 1;

            for (unsigned int i = 0; i < threads_count; ++i) {
                workers_.emplace_back([this]() {
                    // ИСПРАВЛЕНО: Используем memory_order_acquire вместо relaxed.
                    // Это заставляет процессор перечитывать флаг из системной памяти на каждом шаге.
                    while (running_.load(std::memory_order_acquire)) {
                        Job job;
                        if (pop(job)) {
                            job.function(job.data);
                        }
                        else {
                            std::this_thread::yield();
                        }
                    }
                    });
            }
        }

        bool push(Job job) {
            size_t current_tail = tail_.load(std::memory_order_relaxed);
            size_t next_tail = (current_tail + 1) % RING_SIZE;
            if (next_tail == head_.load(std::memory_order_acquire)) {
                return false;
            }
            queue_[current_tail] = job;
            tail_.store(next_tail, std::memory_order_release);
            return true;
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
            // ИСПРАВЛЕНО: Выставляем false с барьером release, 
            // чтобы все фоновые ядра CPU мгновенно увидели команду на выход
            running_.store(false, std::memory_order_release);

            for (auto& worker : workers_) {
                if (worker.joinable()) {
                    worker.join(); // Главный поток гарантированно дожидается остановки потока
                }
            }
            workers_.clear();
        }
    };

} // namespace core

#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <cstdint>
#include <algorithm>
#include <cassert>

#if defined(_MSC_VER)
#include <intrin.h>
#pragma warning(push)
#pragma warning(disable: 4324)
#elif defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
#include <immintrin.h>
#endif

namespace core {

    inline void cpu_relax() noexcept {
#if defined(_MSC_VER) || defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
#if defined(_MSC_VER)
        _mm_pause();
#else
        __builtin_ia32_pause();
#endif
#else
        std::this_thread::yield();
#endif
    }

    struct JobCounter {
        std::atomic<int32_t> value{ 0 };
    };

    struct Job {
        std::function<void()> task;
        JobCounter* counter{ nullptr };
    };

    template<typename T>
    class MpmcBoundedQueue {
    private:
        struct alignas(64) Cell {
            std::atomic<size_t> sequence;
            T                   data;
        };

        Cell* const          buffer_;
        size_t const         buffer_mask_;
        alignas(64) std::atomic<size_t> enqueue_pos_{ 0 };
        alignas(64) std::atomic<size_t> dequeue_pos_{ 0 };

    public:
        explicit MpmcBoundedQueue(size_t buffer_size = 4096)
            : buffer_(new Cell[buffer_size])
            , buffer_mask_(buffer_size - 1) {
            assert((buffer_size >= 2) && ((buffer_size & (buffer_size - 1)) == 0));
            for (size_t i = 0; i < buffer_size; ++i) {
                buffer_[i].sequence.store(i, std::memory_order_relaxed);
            }
        }

        ~MpmcBoundedQueue() {
            delete[] buffer_;
        }

        MpmcBoundedQueue(const MpmcBoundedQueue&) = delete;
        MpmcBoundedQueue& operator=(const MpmcBoundedQueue&) = delete;

        bool push(const T& data) {
            Cell* cell;
            size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
            for (;;) {
                cell = &buffer_[pos & buffer_mask_];
                size_t seq = cell->sequence.load(std::memory_order_acquire);
                intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
                if (dif == 0) {
                    if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        break;
                    }
                }
                else if (dif < 0) {
                    return false;
                }
                else {
                    pos = enqueue_pos_.load(std::memory_order_relaxed);
                }
                cpu_relax();
            }
            cell->data = data;
            cell->sequence.store(pos + 1, std::memory_order_release);
            return true;
        }

        bool pop(T& data) {
            Cell* cell;
            size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
            for (;;) {
                cell = &buffer_[pos & buffer_mask_];
                size_t seq = cell->sequence.load(std::memory_order_acquire);
                intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
                if (dif == 0) {
                    if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        break;
                    }
                }
                else if (dif < 0) {
                    return false;
                }
                else {
                    pos = dequeue_pos_.load(std::memory_order_relaxed);
                }
                cpu_relax();
            }
            data = std::move(cell->data);
            cell->sequence.store(pos + buffer_mask_ + 1, std::memory_order_release);
            return true;
        }
    };

    class JobSystem {
    private:
        MpmcBoundedQueue<Job>   queue_{ 4096 };
        std::vector<std::thread> workers_;
        std::atomic<bool>       is_running_{ false };

        std::mutex              sleep_mutex_;
        std::condition_variable sleep_cv_;

    public:
        JobSystem() = default;

        ~JobSystem() {
            shutdown();
        }

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;

        void initialize(uint32_t thread_count = 0) {
            if (is_running_.load(std::memory_order_acquire)) {
                return;
            }

            if (thread_count == 0) {
                unsigned int hc = std::thread::hardware_concurrency();
                thread_count = (hc > 1) ? (hc - 1) : 1;
            }
            thread_count = (std::max)(1u, thread_count);

            is_running_.store(true, std::memory_order_release);
            workers_.reserve(thread_count);

            for (uint32_t i = 0; i < thread_count; ++i) {
                workers_.emplace_back([this]() {
                    worker_loop();
                    });
            }
        }

        void run(std::function<void()> task, JobCounter* counter = nullptr) {
            if (!task) return;

            if (counter) {
                counter->value.fetch_add(1, std::memory_order_relaxed);
            }

            Job job{ std::move(task), counter };

            if (!queue_.push(job)) {
                // Если очередь заполнена, выполняем прямо в текущем потоке
                execute_job(job);
            }
            else {
                sleep_cv_.notify_one();
            }
        }

        void wait(const JobCounter* counter) {
            if (!counter) return;

            while (counter->value.load(std::memory_order_acquire) > 0) {
                Job job;
                if (queue_.pop(job)) {
                    execute_job(job);
                }
                else {
                    cpu_relax();
                }
            }
        }

        void shutdown() {
            bool expected = true;
            if (!is_running_.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
                return;
            }

            sleep_cv_.notify_all();

            for (auto& worker : workers_) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
            workers_.clear();
        }

        bool isRunning() const noexcept {
            return is_running_.load(std::memory_order_acquire);
        }

        uint32_t workerCount() const noexcept {
            return static_cast<uint32_t>(workers_.size());
        }

    private:
        void worker_loop() {
            while (is_running_.load(std::memory_order_acquire)) {
                Job job;
                if (queue_.pop(job)) {
                    execute_job(job);
                }
                else {
                    std::unique_lock<std::mutex> lock(sleep_mutex_);
                    sleep_cv_.wait_for(lock, std::chrono::milliseconds(2), [this, &job]() {
                        return !is_running_.load(std::memory_order_acquire) || queue_.pop(job);
                        });
                    if (job.task) {
                        execute_job(job);
                    }
                }
            }
        }

        void execute_job(Job& job) {
            if (job.task) {
                try {
                    job.task();
                }
                catch (...) {}
            }
            if (job.counter) {
                job.counter->value.fetch_sub(1, std::memory_order_release);
            }
        }
    };

} // namespace core

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
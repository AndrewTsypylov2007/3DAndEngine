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
#include <memory>
#include <algorithm>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324) // Подавление информационного варнинга C4324 о padding для alignas(64)
#include <intrin.h>
#elif defined(__i386__) || defined(__x86_64__)
#include <immintrin.h>
#endif

namespace core {

    // Кроссплатформенная микро-пауза конвейера CPU (Hardware Backoff)
    inline void cpu_pause() noexcept {
#if defined(_MSC_VER) || defined(__i386__) || defined(__x86_64__)
        _mm_pause();
#elif defined(__arm__) || defined(__aarch64__)
        asm volatile("yield" ::: "memory");
#else
        std::this_thread::yield();
#endif
    }

    struct JobCounter {
        std::atomic<uint32_t> count{ 0 };
    };

    struct Job {
        std::function<void()> task;
        JobCounter* counter{ nullptr };
    };

    // =========================================================================
    // LOCK-FREE BOUNDED MPMC QUEUE (Vyukov Algorithm)
    // Динамическое размещение буфера в куче (0 байт расхода стека)
    // =========================================================================
    template<typename T, size_t Capacity = 4096>
    class MpmcBoundedQueue {
        static_assert((Capacity& (Capacity - 1)) == 0, "Capacity must be a power of 2");
    public:
        struct Slot {
            std::atomic<size_t> sequence;
            T                   storage;
        };

        static constexpr size_t BUFFER_MASK = Capacity - 1;

    private:
        std::unique_ptr<Slot[]>         buffer_;

        alignas(64) std::atomic<size_t> enqueue_pos_{ 0 };
        alignas(64) std::atomic<size_t> dequeue_pos_{ 0 };

    public:
        MpmcBoundedQueue()
            : buffer_(std::make_unique<Slot[]>(Capacity)) {
            for (size_t i = 0; i < Capacity; ++i) {
                buffer_[i].sequence.store(i, std::memory_order_relaxed);
            }
        }

        ~MpmcBoundedQueue() = default;
        MpmcBoundedQueue(const MpmcBoundedQueue&) = delete;
        MpmcBoundedQueue& operator=(const MpmcBoundedQueue&) = delete;

        bool push(const T& data) {
            Slot* slot = nullptr;
            size_t pos = enqueue_pos_.load(std::memory_order_relaxed);

            for (;;) {
                slot = &buffer_[pos & BUFFER_MASK];
                size_t seq = slot->sequence.load(std::memory_order_acquire);
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
            }

            slot->storage = data;
            slot->sequence.store(pos + 1, std::memory_order_release);
            return true;
        }

        bool pop(T& result) {
            Slot* slot = nullptr;
            size_t pos = dequeue_pos_.load(std::memory_order_relaxed);

            for (;;) {
                slot = &buffer_[pos & BUFFER_MASK];
                size_t seq = slot->sequence.load(std::memory_order_acquire);
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
            }

            result = std::move(slot->storage);
            slot->sequence.store(pos + BUFFER_MASK + 1, std::memory_order_release);
            return true;
        }

        bool empty() const {
            size_t deq = dequeue_pos_.load(std::memory_order_relaxed);
            size_t enq = enqueue_pos_.load(std::memory_order_relaxed);
            return deq >= enq;
        }
    };

    // =========================================================================
    // MULTITHREADED JOB SYSTEM
    // =========================================================================
    class JobSystem {
    private:
        std::vector<std::thread>        workers_;
        MpmcBoundedQueue<Job, 4096>     queue_;

        std::atomic<bool>               running_{ false };
        std::atomic<uint32_t>           active_jobs_{ 0 };

        std::mutex                      cv_mutex_;
        std::condition_variable         cv_;

    public:
        JobSystem() = default;

        ~JobSystem() {
            shutdown();
        }

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;

        void initialize(uint32_t thread_count = 0) {
            if (running_.load(std::memory_order_acquire)) return;

            if (thread_count == 0) {
                uint32_t hw = std::thread::hardware_concurrency();
                thread_count = (hw > 1) ? (hw - 1) : 1;
            }

            running_.store(true, std::memory_order_release);
            workers_.reserve(thread_count);

            for (uint32_t i = 0; i < thread_count; ++i) {
                workers_.emplace_back(&JobSystem::workerLoop, this);
            }
        }

        void run(const std::function<void()>& task, JobCounter* counter = nullptr) {
            if (!task) return;

            if (counter) {
                counter->count.fetch_add(1, std::memory_order_relaxed);
            }
            active_jobs_.fetch_add(1, std::memory_order_relaxed);

            Job job{ task, counter };

            if (!queue_.push(job)) {
                execute(job);
            }
            else {
                cv_.notify_one();
            }
        }

        void parallel_for(size_t count, size_t chunk_size, const std::function<void(size_t start, size_t end)>& loop_body) {
            if (count == 0 || !loop_body) return;

            if (chunk_size == 0) chunk_size = 1;
            JobCounter counter;

            for (size_t i = 0; i < count; i += chunk_size) {
                size_t start = i;
                size_t end = std::min(i + chunk_size, count);

                run([start, end, &loop_body]() {
                    loop_body(start, end);
                    }, &counter);
            }

            wait(&counter);
        }

        void wait(JobCounter* counter) {
            if (!counter) return;

            while (counter->count.load(std::memory_order_acquire) > 0) {
                Job job;
                if (queue_.pop(job)) {
                    execute(job);
                }
                else {
                    core::cpu_pause();
                }
            }
        }

        void shutdown() {
            if (!running_.load(std::memory_order_acquire)) return;

            running_.store(false, std::memory_order_release);
            cv_.notify_all();

            for (auto& worker : workers_) {
                if (worker.joinable()) {
                    worker.join();
                }
            }

            workers_.clear();

            Job job;
            while (queue_.pop(job)) {
                execute(job);
            }
        }

        uint32_t workerCount() const {
            return static_cast<uint32_t>(workers_.size());
        }

    private:
        void execute(Job& job) {
            if (job.task) {
                try {
                    job.task();
                }
                catch (...) {
                }
            }

            if (job.counter) {
                job.counter->count.fetch_sub(1, std::memory_order_release);
            }
            active_jobs_.fetch_sub(1, std::memory_order_release);
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

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
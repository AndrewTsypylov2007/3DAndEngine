#pragma once

#include "IService.h"
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <string>

namespace core {

    class TaskScheduler : public IService {
    public:
        TaskScheduler();
        ~TaskScheduler();

        // Паспорт сервиса для версии 0.1.1
        const char* getServiceName() const override { return "TaskScheduler"; }

        // Методы жизненного цикла по стандарту IService
        bool init(ServiceManager& services) override;
        void start() override;
        void stop() override;

        // Метод добавления задач (реализация ниже)
        template<class F, class... Args>
        auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>;

    private:
        void workerLoop();

        std::vector<std::thread> threads_;
        std::queue<std::function<void()>> tasks_;

        std::mutex mutex_;
        std::condition_variable cv_;

        bool running_ = false;
        bool accepting_ = false;
        unsigned int threadCount_;
    };

    // Реализация шаблона enqueue
    template<class F, class... Args>
    auto TaskScheduler::enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!accepting_) throw std::runtime_error("enqueue on stopped TaskScheduler");
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return res;
    }

} // namespace core

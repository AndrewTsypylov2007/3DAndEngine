#pragma once

#include "IService.h"
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace core {

class TaskScheduler : public IService {
public:
	TaskScheduler();
	~TaskScheduler();

	// IService lifecycle
	bool init(ServiceManager &services) override;
	void start() override;
	void postStart() override {}
	void stop() override;

	// Submit a task returning future
	template<typename F, typename... Args>
	auto submit(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>> {
		using R = typename std::invoke_result_t<F, Args...>;
		auto task = std::make_shared<std::packaged_task<R()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		std::future<R> res = task->get_future();
		{
			std::lock_guard<std::mutex> lk(mutex_);
			if (!accepting_) throw std::runtime_error("TaskScheduler not accepting tasks");
			tasks_.emplace([task](){ (*task)(); });
		}
		cv_.notify_one();
		return res;
	}

	// Enqueue a void task
	void enqueue(std::function<void()> fn) {
		std::lock_guard<std::mutex> lk(mutex_);
		if (!accepting_) throw std::runtime_error("TaskScheduler not accepting tasks");
		tasks_.emplace(std::move(fn));
		cv_.notify_one();
	}

private:
	void workerLoop();

	std::vector<std::thread> threads_;
	std::queue<std::function<void()>> tasks_;
	std::mutex mutex_;
	std::condition_variable cv_;
	bool accepting_ = true;
	bool running_ = false;
	unsigned int threadCount_ = 0;
};

} // namespace core

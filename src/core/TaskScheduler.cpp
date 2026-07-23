#include "../../include/core/TaskScheduler.h"
#include "../../include/core/ServiceManager.h"
#include <iostream>

namespace core {

TaskScheduler::TaskScheduler() {
	threadCount_ = std::max(1u, std::thread::hardware_concurrency());
}

TaskScheduler::~TaskScheduler() {
	try { stop(); } catch(...) {}
}

bool TaskScheduler::init(ServiceManager &services) {
	// Could read config service for thread count; keep default for now
	return true;
}

void TaskScheduler::start() {
	std::lock_guard<std::mutex> lk(mutex_);
	if (running_) return;
	running_ = true;
	accepting_ = true;
	for (unsigned int i = 0; i < threadCount_; ++i) {
		threads_.emplace_back([this](){ workerLoop(); });
	}
	std::cerr << "[TaskScheduler] started with " << threads_.size() << " threads\n";
}

void TaskScheduler::stop() {
	{
		std::lock_guard<std::mutex> lk(mutex_);
		if (!running_) return; // already stopped
		accepting_ = false;
	}
	cv_.notify_all();
	for (auto &t : threads_) {
		if (t.joinable()) t.join();
	}
	threads_.clear();
	// clear remaining tasks
	{
		std::lock_guard<std::mutex> lk(mutex_);
		while (!tasks_.empty()) tasks_.pop();
		running_ = false;
	}
	std::cerr << "[TaskScheduler] stopped\n";
}

void TaskScheduler::workerLoop() {
	for (;;) {
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> lk(mutex_);
			cv_.wait(lk, [this]{ return !accepting_ || !tasks_.empty(); });
			if (!accepting_ && tasks_.empty()) return;
			task = std::move(tasks_.front());
			tasks_.pop();
		}
		try {
			task();
		} catch (const std::exception &e) {
			std::cerr << "[TaskScheduler] task exception: " << e.what() << "\n";
		} catch (...) {
			std::cerr << "[TaskScheduler] unknown task exception\n";
		}
	}
}

} // namespace core

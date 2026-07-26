// src/core/TaskScheduler.cpp
#include "../../include/core/TaskScheduler.h"
#include "../../include/core/ServiceManager.h"
#include <iostream>
#include <algorithm>

namespace core {

	TaskScheduler::TaskScheduler() {
		threadCount_ = std::max(1u, std::thread::hardware_concurrency());
	}

	TaskScheduler::~TaskScheduler() {
		try {
			stop();
		}
		catch (...) {}
	}

	bool TaskScheduler::init(ServiceManager& services) {
		// Здесь в будущем можно будет читать количество потоков из ConfigService
		return true;
	}

	void TaskScheduler::start() {
		std::lock_guard<std::mutex> lk(mutex_);
		if (running_) return;

		running_ = true;
		accepting_ = true;

		threads_.reserve(threadCount_);
		for (unsigned int i = 0; i < threadCount_; ++i) {
			threads_.emplace_back([this]() { workerLoop(); });
		}
		std::cerr << "[TaskScheduler] started with " << threads_.size() << " threads\n";
	}

	void TaskScheduler::stop() {
		{
			std::lock_guard<std::mutex> lk(mutex_);
			if (!running_) return; // Уже остановлен
			accepting_ = false;

			// ЖЕЛЕЗОБЕТОННЫЙ ФИКС ДЕДЛОКА: Очищаем очередь задач СРАЗУ.
			// Теперь условие выхода в workerLoop гарантированно сработает для всех потоков.
			while (!tasks_.empty()) {
				tasks_.pop();
			}
		}

		// Будим все потоки, чтобы они увидели, что принимать задачи больше нельзя и очередь пуста
		cv_.notify_all();

		// Безопасно дожидаемся завершения каждого потока пула
		for (auto& t : threads_) {
			if (t.joinable()) {
				t.join();
			}
		}
		threads_.clear();

		{
			std::lock_guard<std::mutex> lk(mutex_);
			running_ = false;
		}
		std::cerr << "[TaskScheduler] stopped\n";
	}

	void TaskScheduler::workerLoop() {
		for (;;) {
			std::function<void()> task;
			{
				std::unique_lock<std::mutex> lk(mutex_);

				// Ждем, пока либо не придет новая задача, либо пока пул не закроют
				cv_.wait(lk, [this] { return !accepting_ || !tasks_.empty(); });

				// Если пул закрывается и задачи закончились — поток завершает работу
				if (!accepting_ && tasks_.empty()) {
					return;
				}

				task = std::move(tasks_.front());
				tasks_.pop();
			}

			// Выполняем задачу ВНЕ мьютекса, чтобы не блокировать другие потоки пула
			try {
				if (task) task();
			}
			catch (const std::exception& e) {
				std::cerr << "[TaskScheduler] task exception: " << e.what() << "\n";
			}
			catch (...) {
				std::cerr << "[TaskScheduler] unknown task exception\n";
			}
		}
	}

} // namespace core

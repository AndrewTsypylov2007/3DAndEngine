// src/core/ServiceManager.cpp
#include "../../include/core/ServiceManager.h"
#include "../../include/core/IService.h"
#include <algorithm>
#include <typeindex>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace core {

	ServiceManager::~ServiceManager() {
		stopAll();
	}

	void ServiceManager::startAll() {
		std::lock_guard<std::mutex> lk(mutex_);

		std::vector<std::type_index> order;
		std::vector<std::type_index> stack;
		std::unordered_map<std::type_index, int> state; // 0=unvisited, 1=visiting, 2=visited

		std::function<void(const std::type_index&)> dfs = [&](const std::type_index& t) {
			int st = state[t];
			if (st == 2) return; // уже обработан
			if (st == 1) {
				// Обнаружен цикл, собираем путь для вывода ошибки
				auto itpos = std::find(stack.begin(), stack.end(), t);
				std::string cycle;
				if (itpos != stack.end()) {
					for (auto it = itpos; it != stack.end(); ++it) {
						cycle += (*it).name();
						cycle += " -> ";
					}
					cycle += t.name();
				}
				else {
					cycle = t.name();
				}
				throw std::runtime_error("Dependency cycle detected: " + cycle);
			}

			state[t] = 1;
			stack.push_back(t);

			auto it = deps_.find(t);
			if (it != deps_.end()) {
				for (auto& d : it->second) dfs(d);
			}

			stack.pop_back();
			state[t] = 2;
			order.push_back(t); // Базовые (независимые) сервисы попадут в начало
			};

		for (auto& kv : services_) {
			dfs(kv.first);
		}

		// Сохраняем вычисленный топологический порядок для последующей остановки
		activeOrder_ = order;

		// Фаза 1: Инициализация всех сервисов в прямом порядке
		for (auto& typeIdx : activeOrder_) {
			auto svcIt = services_.find(typeIdx);
			if (svcIt != services_.end()) {
				auto svc = svcIt->second;
				if (svc && !svc->init(*this)) {
					throw std::runtime_error("Service initialization failed for: " + std::string(typeIdx.name()));
				}
			}
		}

		// Фаза 2: Запуск всех сервисов в прямом порядке
		for (auto& typeIdx : activeOrder_) {
			auto svcIt = services_.find(typeIdx);
			if (svcIt != services_.end()) {
				auto svc = svcIt->second;
				if (svc) svc->start();
			}
		}

		// Фаза 3: Пост-запуск всех сервисов в прямом порядке
		for (auto& typeIdx : activeOrder_) {
			auto svcIt = services_.find(typeIdx);
			if (svcIt != services_.end()) {
				auto svc = svcIt->second;
				if (svc) svc->postStart();
			}
		}
	}

	void ServiceManager::stopAll() {
		std::lock_guard<std::mutex> lk(mutex_);

		// Останавливаем сервисы строго в обратном порядке от их запуска (LIFO)
		for (auto it = activeOrder_.rbegin(); it != activeOrder_.rend(); ++it) {
			auto svcIt = services_.find(*it);
			if (svcIt != services_.end()) {
				auto svc = svcIt->second;
				if (svc) {
					try {
						svc->stop();
					}
					catch (...) {
						// Игнорируем исключения при остановке, чтобы дать закрыться остальным сервисам
					}
				}
			}
		}
		activeOrder_.clear();
	}

} // namespace core

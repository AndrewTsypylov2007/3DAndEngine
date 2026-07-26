// include/core/ServiceManager.h
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <typeindex>
#include <mutex>
#include <vector>

namespace core {

	class IService;

	class ServiceManager {
	public:
		ServiceManager() = default;
		~ServiceManager();

		// Регистрация сервиса с указанием типов его зависимостей
		template<typename T, typename... Deps>
		void registerService(std::shared_ptr<T> svc) {
			std::lock_guard<std::mutex> lk(mutex_);
			// Безопасно сохраняем как указатель на базовый интерфейс IService
			services_[std::type_index(typeid(T))] = std::static_pointer_cast<IService>(svc);
			std::vector<std::type_index> dv = { std::type_index(typeid(Deps))... };
			deps_[std::type_index(typeid(T))] = dv;
		}

		// Получение сервиса по его типу
		template<typename T>
		std::shared_ptr<T> getService() {
			std::lock_guard<std::mutex> lk(mutex_);
			auto it = services_.find(std::type_index(typeid(T)));
			if (it == services_.end()) return {};
			return std::static_pointer_cast<T>(it->second);
		}

		void startAll();
		void stopAll();

	private:
		std::unordered_map<std::type_index, std::shared_ptr<IService>> services_;
		std::unordered_map<std::type_index, std::vector<std::type_index>> deps_;
		std::vector<std::type_index> activeOrder_; // Хранит правильный порядок для LIFO остановки
		std::mutex mutex_;
	};

} // namespace core

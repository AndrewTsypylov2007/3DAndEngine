// include/core/IService.h
#pragma once

namespace core {

	class ServiceManager;

	class IService {
	public:
		virtual ~IService() = default;

		// Фаза 1: Подготовка сервиса и проверка зависимостей
		virtual bool init(ServiceManager& services) { return true; }

		// Фаза 2: Основной запуск сервиса
		virtual void start() {}

		// Фаза 3: Пост-инициализация (когда запущены абсолютно все сервисы)
		virtual void postStart() {}

		// Остановка сервиса (будет вызываться в обратном порядке)
		virtual void stop() {}
	};

} // namespace core

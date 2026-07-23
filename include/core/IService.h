#pragma once

namespace core {

class ServiceManager; // forward

class IService {
public:
	virtual ~IService() = default;
	// Подготовка сервиса (регистрация зависимостей через ServiceManager доступна)
	virtual bool init(ServiceManager &services) { return true; }
	// Запуск сервиса (выполняется после init всех сервисов)
	virtual void start() {}
	// Вызывается после того, как все сервисы были запущены
	virtual void postStart() {}
	// Остановка сервиса (обратный порядок)
	virtual void stop() {}
};

} // namespace core

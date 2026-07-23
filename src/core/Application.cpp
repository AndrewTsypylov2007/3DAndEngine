#include "../../include/core/Application.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "../../include/core/LoggerService.h"
#include "../../include/core/ConfigService.h"
#include "../../include/core/TaskScheduler.h"

namespace core {

Application::Application() = default;
Application::~Application() = default;

int Application::run() {
	running_ = true;
	std::cerr << "[Core] Application starting...\n";

	// Регистрируем базовый сервис конфигурации сначала
	auto configSvc = std::make_shared<core::ConfigService>();
	services_.registerService<core::ConfigService>(configSvc);

	// Попытка загрузить конфиг синхронно, чтобы регулировать дальнейшую регистрацию сервисов
	bool configLoaded = configSvc->loadFromFile("config.json");

	// На основе конфигурации решаем, регистрировать ли логгер (по умолчанию true)
	bool enableLogger = true;
	try {
		if (configLoaded) {
			auto j = configSvc->json();
			if (j.contains("core") && j["core"].is_object()) {
				enableLogger = j["core"].value("enable_logger", true);
			} else {
				enableLogger = j.value("enable_logger", true);
			}
		}
	} catch (...) { enableLogger = true; }

	if (enableLogger) {
		auto loggerSvc = std::make_shared<core::LoggerService>();
		services_.registerService<core::LoggerService>(loggerSvc);
	}

	// Scheduler optional
	bool enableScheduler = true;
	try {
		if (configLoaded) {
			auto j = configSvc->json();
			if (j.contains("core") && j["core"].is_object()) {
				enableScheduler = j["core"].value("enable_scheduler", true);
			} else {
				enableScheduler = j.value("enable_scheduler", true);
			}
		}
	} catch (...) { enableScheduler = true; }

	if (enableScheduler) {
		auto sched = std::make_shared<core::TaskScheduler>();
		services_.registerService<core::TaskScheduler>(sched);
	}

	// Запустить все сервисы
	services_.startAll();

	// Пример: получить логгер и писать через него (если он зарегистрирован)
	auto lg = services_.getService<core::LoggerService>();
	if (lg) lg->info("Application services started");

	// Здесь можно загрузить конфигурацию/плагины и т.д.
	// Автодискавери плагинов в папке plugins/ (source) и в каталоге выполнения
	// Опциональное автодискавери плагинов (по умолчанию true)
	bool enableDiscovery = true;
	try {
		if (configLoaded) {
			auto j = configSvc->json();
			if (j.contains("core") && j["core"].is_object()) {
				enableDiscovery = j["core"].value("enable_plugin_discovery", true);
			} else {
				enableDiscovery = j.value("enable_plugin_discovery", true);
			}
		}
	} catch (...) { enableDiscovery = true; }

	int found = 0;
	if (enableDiscovery) {
		found += pluginLoader_.discoverAndLoad("plugins");
		// Также пробуем папку рядом с исполняемым файлом
		found += pluginLoader_.discoverAndLoad(".");
	}
	if (found > 0) {
		std::cerr << "[Application] Plugins discovered: " << found << "\n";
		if (!pluginLoader_.initializeAll(services_)) {
			std::cerr << "[Application] Plugin initialization failed — shutting down.\n";
			pluginLoader_.unloadAll();
			services_.stopAll();
			return -1;
		}
	}

	// Простой main loop, который завершится после небольшой задержки (пример)
	int ticks = 0;
	while (running_ && ticks < 5) {
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		++ticks;
		eventBus_.publish("tick");
		if (lg) lg->info(std::string("tick ") + std::to_string(ticks));
		else std::cerr << "[Core] tick " << ticks << "\n";
	}

	std::cerr << "[Core] Shutting down...\n";
	pluginLoader_.unloadAll();
	services_.stopAll();
	return 0;
}

void Application::stop() {
	running_ = false;
}

} // namespace core

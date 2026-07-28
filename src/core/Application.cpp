#include "../../include/core/Application.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "../../include/core/LoggerService.h"
#include "../../include/core/ConfigService.h"
#include "../../include/core/TaskScheduler.h"

namespace core {

	Application::Application() {
		// Гарантированно создаем шину событий при старте объекта
		eventBus_ = std::make_shared<core::EventBus>();
	}

	Application::~Application() = default;

	int Application::run() {
		running_ = true;
		std::cerr << "engine_core starting...\n";
		std::cerr << "[Core] Application starting...\n";

		// 1. Регистрируем базовый сервис конфигурации
		auto configSvc = std::make_shared<core::ConfigService>();
		services_.registerService<core::ConfigService>(configSvc);

		// Попытка загрузить конфиг синхронно
		bool configLoaded = configSvc->loadFromFile("config.json");

		// Дефолтные флаги для управления составом ядра
		bool enableLogger = true;
		bool enableScheduler = true;
		bool enableDiscovery = true;

		if (configLoaded) {
			try {
				auto j = configSvc->json();
				auto coreBlock = (j.contains("core") && j["core"].is_object()) ? j["core"] : j;

				enableLogger = coreBlock.value("enable_logger", true);
				enableScheduler = coreBlock.value("enable_scheduler", true);
				enableDiscovery = coreBlock.value("enable_plugin_discovery", true);
			}
			catch (...) {
				// В случае ошибки парсинга остаемся на безопасных дефолтах
			}
		}

		// 2. Опционально регистрируем Логгер
		if (enableLogger) {
			auto loggerSvc = std::make_shared<core::LoggerService>();
			services_.registerService<core::LoggerService>(loggerSvc);
		}

		// 3. Опционально регистрируем Планировщик задач
		if (enableScheduler) {
			auto sched = std::make_shared<core::TaskScheduler>();
			services_.registerService<core::TaskScheduler>(sched);
		}

		// ==============================================================================
		// КЛЮЧЕВОЕ ОБНОВЛЕНИЕ v0.1.1: Регистрация Шины Событий как именованного сервиса.
		// Без этой строки плагины не смогут поймать событие "tick" или "input".
		// ==============================================================================
		services_.registerService<core::EventBus>(eventBus_);

		// Запускаем граф сервисов (выполняет init, start, postStart)
		services_.startAll();

		auto lg = services_.getService<core::LoggerService>();
		if (lg) lg->info("Application services started successfully");

		int foundDlls = 0;
		if (enableDiscovery) {
			std::vector<std::string> libraryPaths;
			if (configLoaded) {
				try {
					auto j = configSvc->json();
					if (j.contains("plugin_paths") && j["plugin_paths"].is_array()) {
						for (auto& p : j["plugin_paths"]) {
							if (p.is_string()) libraryPaths.push_back(p.get<std::string>());
						}
					}
				}
				catch (...) {
					libraryPaths.clear();
				}
			}

			if (libraryPaths.empty()) {
				libraryPaths.push_back("plugins");
			}

			// Использование обновленного метода discoverAndLoad (2 аргумента)
			for (auto& path : libraryPaths) {
				foundDlls += libraryLoader_.discoverAndLoad(path, services_);
			}
		}

		if (foundDlls > 0) {
			std::cerr << "[Application] Dynamic libraries loaded into memory: " << foundDlls << "\n";
			// Массовая инициализация всех найденных плагинов
			libraryLoader_.initializeAll(services_);
		}

		// Главный цикл приложения
		int ticks = 0;
		while (running_) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			++ticks;

			// Трансляция системного тика в EventBus (для геймпадов и логики)
			if (eventBus_) eventBus_->publish("tick");

			if (lg) {
				lg->info("tick " + std::to_string(ticks));
			}
			else {
				std::cerr << "[Core] tick " << ticks << "\n";
			}

			// Ограничитель для предотвращения вечного цикла без UI (в будущем проверяем окна)
			if (ticks > 5000) break;
		}

		std::cerr << "[Core] Shutting down...\n";
		libraryLoader_.unloadAll();
		services_.stopAll();
		return 0;
	}

	void Application::stop() {
		running_ = false;
	}

} // namespace core

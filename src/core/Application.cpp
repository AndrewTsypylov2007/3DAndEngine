// src/core/Application.cpp
#include "../../include/core/Application.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "../../include/core/LoggerService.h"
#include "../../include/core/ConfigService.h"
#include "../../include/core/TaskScheduler.h"

namespace core {

	Application::Application() {
		// Гарантированно создаем шину событий при старте объекта, убирая nullptr-краши
		eventBus_ = std::make_shared<core::EventBus>();
	}

	Application::~Application() = default;

	int Application::run() {
		running_ = true;
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
				// Умная проверка: ищем настройки в блоке "core", либо в корне JSON
				auto coreBlock = (j.contains("core") && j["core"].is_object()) ? j["core"] : j;

				enableLogger = coreBlock.value("enable_logger", true);
				enableScheduler = coreBlock.value("enable_scheduler", true);
				enableDiscovery = coreBlock.value("enable_plugin_discovery", true); // сохраняем имя ключа для совместимости
			}
			catch (...) {
				// В случае кривого JSON остаемся на безопасных дефолтах (true)
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

		// Запускаем граф сервисов (теперь тут честный 3-фазный запуск и валидация циклов!)
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

			// Безопасный дефолт, если пути в конфиге не заданы
			if (libraryPaths.empty()) {
				libraryPaths.push_back("plugins");
			}

			// Сканируем папки и загружаем DLL в память процесса
			for (auto& path : libraryPaths) {
				foundDlls += libraryLoader_.discoverAndLoad(path);
			}
		}

		if (foundDlls > 0) {
			std::cerr << "[Application] Dynamic libraries loaded into memory: " << foundDlls << "\n";
			// Вызываем во всех DLL функцию инициализации "InitializeModule", если она там экспортирована
			if (!libraryLoader_.initializeAll(services_)) {
				std::cerr << "[Application] Library initialization failed — shutting down.\n";
				libraryLoader_.unloadAll();
				services_.stopAll();
				return -1;
			}
		}

		// Главный цикл приложения (простой симуляционный loop)
		int ticks = 0;
		while (running_ && ticks < 5) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			++ticks;

			// Теперь eventBus_ гарантированно валиден и событие уйдет подписчикам в DLL/сервисы
			if (eventBus_) eventBus_->publish("tick");

			if (lg) {
				lg->info("tick " + std::to_string(ticks));
			}
			else {
				std::cerr << "[Core] tick " << ticks << "\n";
			}
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

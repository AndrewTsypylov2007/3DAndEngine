#include "../../include/core/Application.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "../../include/core/LoggerService.h"
#include "../../include/core/ConfigService.h"
#include "../../include/core/TaskScheduler.h"

namespace core {

	Application::Application() {
		// Гарантированно создаем шину событий при старте объекта ядра
		eventBus_ = std::make_shared<core::EventBus>();
	}

	Application::~Application() = default;

	int Application::run() {
		running_ = true;
		std::cerr << "engine_core starting...\n";
		std::cerr << "[Core] Application starting...\n";

		// 1. РЕГИСТРАЦИЯ БАЗОВЫХ СЕРВИСОВ
		auto configSvc = std::make_shared<core::ConfigService>();
		services_.registerService<core::ConfigService>(configSvc);

		// Попытка загрузить конфиг
		bool configLoaded = configSvc->loadFromFile("config.json");

		// Дефолтные настройки состава ядра
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
				// При ошибках парсинга остаемся на безопасных значениях
			}
		}

		// 2. РЕГИСТРАЦИЯ ОПЦИОНАЛЬНЫХ СИСТЕМ
		if (enableLogger) {
			auto loggerSvc = std::make_shared<core::LoggerService>();
			services_.registerService<core::LoggerService>(loggerSvc);
		}

		if (enableScheduler) {
			auto sched = std::make_shared<core::TaskScheduler>();
			services_.registerService<core::TaskScheduler>(sched);
		}

		// ==============================================================================
		// КЛЮЧЕВОЙ МОМЕНТ ОБНОВЛЕНИЯ v0.1.1:
		// Регистрируем Шину Событий в ServiceManager. 
		// Теперь плагины найдут её через services.getServiceByName("EventBus")
		// ==============================================================================
		services_.registerService<core::EventBus>(eventBus_);

		// Запускаем граф зависимостей (выполняет init, start, postStart)
		services_.startAll();

		auto lg = services_.getService<core::LoggerService>();
		if (lg) lg->info("Application services started successfully");

		// 3. ЗАГРУЗКА ПЛАГИНОВ (DLL)
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
				catch (...) { libraryPaths.clear(); }
			}

			// Если список пуст, ищем в стандартной папке
			if (libraryPaths.empty()) {
				libraryPaths.push_back("plugins");
			}

			// Обновленный вызов discoverAndLoad с передачей контекста сервисов
			for (auto& path : libraryPaths) {
				foundDlls += libraryLoader_.discoverAndLoad(path, services_);
			}
		}

		if (foundDlls > 0) {
			if (lg) lg->info("Dynamic libraries loaded into memory: " + std::to_string(foundDlls));
			// Инициализируем все плагины (вызов InitializeModule)
			libraryLoader_.initializeAll(services_);
		}

		// 4. ГЛАВНЫЙ ЦИКЛ ПРИЛОЖЕНИЯ
		int ticks = 0;
		while (running_) {
			// Ограничиваем частоту "тиков" (примерно 60-100Гц для системных нужд)
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
			++ticks;

			// Рассылка системного события tick. 
			// Плагины (Input, Window) поймают его и обновят свое состояние.
			if (eventBus_) eventBus_->publish("tick");

			// Логируем каждый 100-й тик, чтобы не забивать консоль
			if (ticks % 100 == 0 && lg) {
				lg->info("System heartbeat: tick " + std::to_string(ticks));
			}

			// Ограничитель времени жизни (в будущем заменим на проверку активных окон)
			if (ticks > 100000) break;
		}

		// 5. КОРРЕКТНОЕ ЗАВЕРШЕНИЕ (LIFO ORDER)
		if (lg) lg->info("Shutting down engine_core...");

		libraryLoader_.unloadAll();
		services_.stopAll();

		return 0;
	}

	void Application::stop() {
		running_ = false;
	}

} // namespace core


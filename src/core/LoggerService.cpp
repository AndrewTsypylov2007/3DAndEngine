#include "../../include/core/LoggerService.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>

namespace core {

	LoggerService::LoggerService() : logger_(nullptr) {}

	LoggerService::~LoggerService() {
		// Вызываем stop напрямую для гарантии очистки ресурсов в случае деструкции
		LoggerService::stop();
	}

	bool LoggerService::init(ServiceManager& services) {
		// В версии 0.1.1 подготовка логгера происходит здесь
		if (!logger_) {
			// Проверяем глобальный реестр spdlog на наличие дубликатов
			logger_ = spdlog::get("core");

			if (!logger_) {
				try {
					logger_ = spdlog::stdout_color_mt("core");
					// Настраиваем паттерн: [Время] [Уровень] Сообщение
					spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
				}
				catch (const spdlog::spdlog_ex& e) {
					std::cerr << "[LoggerService] Critical error: Failed to initialize spdlog: " << e.what() << "\n";
					return false;
				}
			}
		}
		return true;
	}

	void LoggerService::start() {
		if (logger_) {
			logger_->info("LoggerService started");
		}
	}

	void LoggerService::stop() {
		if (logger_) {
			logger_->info("LoggerService stopping");
			// Удаляем логгер из глобального реестра spdlog
			spdlog::drop("core");
			logger_.reset();
		}
	}

	// Реализация прокси-методов теперь соответствует заголовочному файлу
	// (Если в .h они объявлены как inline или template, в .cpp их можно удалить, 
	// но мы оставим их здесь для бинарной совместимости)

	void LoggerService::info(const std::string& msg) {
		if (logger_) logger_->info(msg);
	}

	void LoggerService::warn(const std::string& msg) {
		if (logger_) logger_->warn(msg);
	}

	void LoggerService::error(const std::string& msg) {
		if (logger_) logger_->error(msg);
	}

	std::shared_ptr<spdlog::logger> LoggerService::getLogger() {
		return logger_;
	}

} // namespace core

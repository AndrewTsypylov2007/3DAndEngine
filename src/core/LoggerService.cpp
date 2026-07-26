// src/core/LoggerService.cpp
#include "../../include/core/LoggerService.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>

namespace core {

	LoggerService::LoggerService() = default;

	LoggerService::~LoggerService() {
		stop();
	}

	void LoggerService::start() {
		if (!logger_) {
			// Безопасная проверка: ищем, не создан ли уже логгер "core" в глобальном реестре
			logger_ = spdlog::get("core");

			if (!logger_) {
				try {
					logger_ = spdlog::stdout_color_mt("core");
					spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
				}
				catch (const const spdlog::spdlog_ex& e) {
					std::cerr << "[LoggerService] Failed to initialize spdlog: " << e.what() << "\n";
					return;
				}
			}

			logger_->info("LoggerService started");
		}
	}

	void LoggerService::stop() {
		if (logger_) {
			logger_->info("LoggerService stopping");
			// Удаляем имя из глобального реестра, чтобы освободить ресурсы
			spdlog::drop("core");
			logger_.reset();
		}
	}

	void LoggerService::info(const std::string& msg) {
		if (logger_) logger_->info(msg);
	}

	void LoggerService::warn(const std::string& msg) {
		if (logger_) logger_->warn(msg);
	}

	void LoggerService::error(const std::string& msg) {
		if (logger_) logger_->error(msg);
	}

	std::shared_ptr<spdlog::logger> LoggerService::logger() {
		return logger_;
	}

} // namespace core

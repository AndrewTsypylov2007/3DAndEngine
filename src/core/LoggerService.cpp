#include "../../include/core/LoggerService.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace core {

LoggerService::LoggerService() = default;

LoggerService::~LoggerService() {
	stop();
}

void LoggerService::start() {
	if (!logger_) {
		logger_ = spdlog::stdout_color_mt("core");
		spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
		logger_->info("LoggerService started");
	}
}

void LoggerService::stop() {
	if (logger_) {
		logger_->info("LoggerService stopping");
		spdlog::drop("core");
		logger_.reset();
	}
}

void LoggerService::info(const std::string &msg) {
	if (logger_) logger_->info(msg);
}

void LoggerService::warn(const std::string &msg) {
	if (logger_) logger_->warn(msg);
}

void LoggerService::error(const std::string &msg) {
	if (logger_) logger_->error(msg);
}

std::shared_ptr<spdlog::logger> LoggerService::logger() { return logger_; }

} // namespace core

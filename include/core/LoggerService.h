#pragma once

#include "IService.h"
#include <memory>
#include <string>

namespace spdlog { class logger; }

namespace core {

class LoggerService : public IService {
public:
	LoggerService();
	~LoggerService();

	void start() override;
	void stop() override;

	// Уровневые методы
	void info(const std::string &msg);
	void warn(const std::string &msg);
	void error(const std::string &msg);

	std::shared_ptr<spdlog::logger> logger();

private:
	std::shared_ptr<spdlog::logger> logger_;
};

} // namespace core

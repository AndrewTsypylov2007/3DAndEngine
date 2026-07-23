#pragma once

#include "IService.h"
#include <nlohmann/json.hpp>
#include <string>

namespace core {

class ConfigService : public IService {
public:
	ConfigService();
	~ConfigService();

	void start() override;
	void stop() override;

	bool loadFromFile(const std::string &path);
	const nlohmann::json& json() const { return json_; }

private:
	nlohmann::json json_;
	std::string loadedPath_;
};

} // namespace core

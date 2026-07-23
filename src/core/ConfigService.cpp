#include "../../include/core/ConfigService.h"
#include <fstream>
#include <iostream>

namespace core {

ConfigService::ConfigService() = default;

ConfigService::~ConfigService() = default;

void ConfigService::start() {
	// По умолчанию пытаемся загрузить config.json в корне
	if (loadedPath_.empty()) {
		loadFromFile("config.json");
	}
}

void ConfigService::stop() {
	// ничего для остановки
}

bool ConfigService::loadFromFile(const std::string &path) {
	std::ifstream in(path);
	if (!in.is_open()) {
		std::cerr << "[ConfigService] Could not open config file: " << path << "\n";
		return false;
	}
	try {
		in >> json_;
		loadedPath_ = path;
		return true;
	} catch (const std::exception &e) {
		std::cerr << "[ConfigService] JSON parse error: " << e.what() << "\n";
		return false;
	}
}

} // namespace core

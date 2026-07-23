#pragma once

#include "IPlugin.h"
#include <vector>
#include <memory>
#include <string>
#include <functional>

namespace core {

// Платформо-независимый тип хендла библиотеки
using LibHandle = void*;

struct LoadedPlugin {
	IPlugin* instance = nullptr;
	LibHandle lib = nullptr;
	DestroyPluginFn destroyFn = nullptr;
	std::string path;
	bool initialized = false;
};

class PluginLoader {
public:
	PluginLoader() = default;
	~PluginLoader();

	// Попытаться загрузить плагин по пути (динамическая библиотека).
	bool loadPlugin(const std::string &path);
	void unloadAll();
	// Инициализировать все загруженные плагины с передачей ServiceManager.
	// Возвращает true, если все плагины инициализированы успешно. В случае ошибки
	// выполняется откат (rollback) — ранее инициализированные плагины выгружаются.
	bool initializeAll(ServiceManager &services); // no-op patch: header update for tracking
	// Просканировать директорию на предмет плагинов и загрузить их (loadPlugin).
	// Возвращает количество успешно загруженных плагинов.
	int discoverAndLoad(const std::string &directory);

private:
	std::vector<LoadedPlugin> plugins_;
};

} // namespace core

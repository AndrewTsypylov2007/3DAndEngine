#pragma once

#include <memory>
#include <string>

namespace core {

class ServiceManager;

class IPlugin {
public:
	virtual ~IPlugin() = default;
	// Инициализация плагина, получение доступа к ServiceManager
	virtual bool initialize(ServiceManager &services) = 0;
	// Выключение плагина
	virtual void shutdown() = 0;
};

using IPluginPtr = std::unique_ptr<IPlugin>;

// Простая структура манифеста (в коде используем JSON для расширяемости)
struct PluginManifest {
	std::string name;
	std::string version;
	std::string core_version_required; // минимальная версия core
};

// Тип функции создания/уничтожения плагина (обычно экспортируемые C-символьные функции)
extern "C" {
	using CreatePluginFn = IPlugin* (*)();
	using DestroyPluginFn = void (*)(IPlugin*);
	// Возвращает JSON-манифест плагина (нулевой-терминированная строка)
	using GetPluginManifestFn = const char* (*)();
}

} // namespace core

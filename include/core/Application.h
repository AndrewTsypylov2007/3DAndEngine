#pragma once

#include "PluginLoader.h"
#include "ServiceManager.h"
#include "EventBus.h"

namespace core {

class Application {
public:
	Application();
	~Application();

	int run();
	void stop();

	PluginLoader &pluginLoader() { return pluginLoader_; }
	ServiceManager &services() { return services_; }
	EventBus &eventBus() { return eventBus_; }

private:
	bool running_ = false;
	PluginLoader pluginLoader_;
	ServiceManager services_;
	EventBus eventBus_;
};

} // namespace core

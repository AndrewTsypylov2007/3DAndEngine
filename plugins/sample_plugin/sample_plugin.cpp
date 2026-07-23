#include "../../include/core/IPlugin.h"
#include "../../include/core/ServiceManager.h"
#include <iostream>

namespace core {
	// Пустой тестовый плагин
	class SamplePlugin : public IPlugin {
	public:
		bool initialize(ServiceManager &services) override {
			std::cerr << "[SamplePlugin] initialize\n";
			return true;
		}
		void shutdown() override {
			std::cerr << "[SamplePlugin] shutdown\n";
		}
	};
}

extern "C" {
	core::IPlugin* CreatePlugin() {
		return new core::SamplePlugin();
	}

	void DestroyPlugin(core::IPlugin* p) {
		delete p;
	}

	const char* GetPluginManifest() {
		// Простая JSON-строка с информацией о плагине
		return R"({"name":"sample_plugin","version":"0.1.0","core_version_required":"0.1.0"})";
	}
}

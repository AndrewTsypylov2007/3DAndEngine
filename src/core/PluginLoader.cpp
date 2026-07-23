#include "../../include/core/PluginLoader.h"
#include "../../include/core/IPlugin.h"
#include "../../include/core/ServiceManager.h"
#include "../../include/core/SemVer.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>
#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace core {

PluginLoader::~PluginLoader() {
	unloadAll();
}

// Forward declarations for platform helpers (defined below)
static LibHandle openLibrary(const std::string &path);
static void closeLibrary(LibHandle h);
static void* getSymbol(LibHandle h, const char* name);

int PluginLoader::discoverAndLoad(const std::string &directory) {
	namespace fs = std::filesystem;
	int loaded = 0;
	try {
		fs::path dir(directory);
		if (!fs::exists(dir) || !fs::is_directory(dir)) return 0;
		for (auto &entry : fs::recursive_directory_iterator(dir)) {
			if (!entry.is_regular_file()) continue;
			auto p = entry.path();
#if defined(_WIN32)
			if (p.extension() != ".dll") continue;
#elif defined(__APPLE__)
			if (p.extension() != ".dylib") continue;
#else
			if (p.extension() != ".so") continue;
#endif
			// Attempt to read manifest early to respect "enabled" flag without fully initializing
			bool skipByManifest = false;
			// Forward-declare platform helpers are above; call them
			LibHandle lib = nullptr;
			try {
				lib = openLibrary(p.string());
				if (lib) {
					auto manifestFn = (GetPluginManifestFn)getSymbol(lib, "GetPluginManifest");
					if (manifestFn) {
						const char* manifestC = manifestFn();
						if (manifestC) {
							try {
								auto j = nlohmann::json::parse(manifestC);
								bool enabled = j.value("enabled", true);
								if (!enabled) skipByManifest = true;
							} catch (...) {
								// ignore parse errors here; loading will re-check later
							}
						}
					}
				}
			} catch (...) {
				// ignore
			}
			if (lib) closeLibrary(lib);
			if (skipByManifest) {
				std::cerr << "[PluginLoader] Skipping disabled plugin: " << p.string() << "\n";
				continue;
			}

			if (loadPlugin(p.string())) ++loaded;
		}
	} catch (const std::exception &e) {
		std::cerr << "[PluginLoader] discoverAndLoad error: " << e.what() << "\n";
	}
	return loaded;
}

// Forward declarations for platform helpers (defined below)
static LibHandle openLibrary(const std::string &path);
static void closeLibrary(LibHandle h);
static void* getSymbol(LibHandle h, const char* name);

bool PluginLoader::initializeAll(ServiceManager &services) {
	std::vector<LoadedPlugin*> initialized;
	try {
		for (auto &p : plugins_) {
			if (!p.instance) continue;
			bool ok = false;
			try {
				ok = p.instance->initialize(services);
			} catch (const std::exception &e) {
				std::cerr << "[PluginLoader] Exception during initialize of " << p.path << ": " << e.what() << "\n";
				ok = false;
			} catch (...) {
				std::cerr << "[PluginLoader] Unknown exception during initialize of " << p.path << "\n";
				ok = false;
			}
			if (!ok) {
				throw std::runtime_error(std::string("Plugin initialization failed: ") + p.path);
			}
			p.initialized = true;
			initialized.push_back(&p);
			std::cerr << "[PluginLoader] Plugin initialized: " << p.path << "\n";
		}
		return true;
	} catch (const std::exception &e) {
		std::cerr << "[PluginLoader] Initialization failed: " << e.what() << ". Rolling back...\n";
		// rollback: shutdown and destroy initialized plugins in reverse order
		for (auto it = initialized.rbegin(); it != initialized.rend(); ++it) {
			LoadedPlugin *lp = *it;
			if (!lp) continue;
			try {
				if (lp->instance) lp->instance->shutdown();
			} catch (...) {}
			try {
				if (lp->destroyFn && lp->instance) lp->destroyFn(lp->instance);
			} catch (...) {}
			lp->instance = nullptr;
			lp->initialized = false;
			if (lp->lib) {
				closeLibrary(lp->lib);
				lp->lib = nullptr;
			}
		}
		// clear remaining plugins
		for (auto &p : plugins_) {
			if (p.instance) {
				try { if (p.destroyFn) p.destroyFn(p.instance); } catch (...) {}
				p.instance = nullptr;
			}
			if (p.lib) { closeLibrary(p.lib); p.lib = nullptr; }
		}
		plugins_.clear();
		return false;
	}
}

static LibHandle openLibrary(const std::string &path) {
#if defined(_WIN32)
	return (LibHandle)LoadLibraryA(path.c_str());
#else
	return dlopen(path.c_str(), RTLD_NOW);
#endif
}

static void closeLibrary(LibHandle h) {
#if defined(_WIN32)
	if (h) FreeLibrary((HMODULE)h);
#else
	if (h) dlclose(h);
#endif
}

static void* getSymbol(LibHandle h, const char* name) {
#if defined(_WIN32)
	return (void*)GetProcAddress((HMODULE)h, name);
#else
	return dlsym(h, name);
#endif
}

bool PluginLoader::loadPlugin(const std::string &path) {
	std::cerr << "[PluginLoader] Loading plugin: " << path << "\n";
	LibHandle lib = openLibrary(path);
	if (!lib) {
#if defined(_WIN32)
		std::cerr << "[PluginLoader] Failed to load library: " << path << "\n";
#else
		std::cerr << "[PluginLoader] Failed to load library: " << path << " error: " << dlerror() << "\n";
#endif
		return false;
	}

	// Сначала читаем манифест, если он есть
	auto manifestFn = (GetPluginManifestFn)getSymbol(lib, "GetPluginManifest");
	if (manifestFn) {
		const char* manifestC = manifestFn();
		if (manifestC) {
			try {
				auto j = nlohmann::json::parse(manifestC);
				std::string name = j.value("name", "");
				std::string ver = j.value("version", "");
				std::string coreReq = j.value("core_version_required", "");
				bool enabled = j.value("enabled", true);
				if (!enabled) {
					std::cerr << "[PluginLoader] Manifest marks plugin as disabled: " << name << " (" << path << ")\n";
					closeLibrary(lib);
					return false;
				}
				std::cerr << "[PluginLoader] Manifest for plugin '" << name << "' v" << ver << " requires core>=" << coreReq << "\n";
				// Семантическая проверка совместимости: требуемая версия должна быть <= текущей core версии
				const std::string currentCoreVersionStr = "0.1.0";
				core::SemVer currentCoreVersion = core::SemVer::parse(currentCoreVersionStr);
				if (!coreReq.empty()) {
					core::SemVer req = core::SemVer::parse(coreReq);
					if (currentCoreVersion < req) {
						std::cerr << "[PluginLoader] Plugin '" << name << "' requires core " << coreReq << " but current is " << currentCoreVersionStr << ". Skipping.\n";
						closeLibrary(lib);
						return false;
					}
				}
			} catch (const std::exception &e) {
				std::cerr << "[PluginLoader] Failed to parse manifest for " << path << ": " << e.what() << "\n";
				// continue; we'll still try to load if Create/Destroy exist
			}
		}
	}

	auto createFn = (CreatePluginFn)getSymbol(lib, "CreatePlugin");
	auto destroyFn = (DestroyPluginFn)getSymbol(lib, "DestroyPlugin");
	if (!createFn || !destroyFn) {
		std::cerr << "[PluginLoader] Plugin missing CreatePlugin/DestroyPlugin exports: " << path << "\n";
		closeLibrary(lib);
		return false;
	}

	IPlugin *inst = createFn();
	if (!inst) {
		std::cerr << "[PluginLoader] CreatePlugin returned null: " << path << "\n";
		closeLibrary(lib);
		return false;
	}

	LoadedPlugin lp;
	lp.instance = inst;
	lp.lib = lib;
	lp.destroyFn = destroyFn;
	lp.path = path;

	plugins_.push_back(std::move(lp));
	std::cerr << "[PluginLoader] Plugin loaded: " << path << "\n";
	return true;
}

void PluginLoader::unloadAll() {
	for (auto &p : plugins_) {
		if (p.instance) {
			try {
				p.instance->shutdown();
			} catch (...) {}
			if (p.destroyFn) p.destroyFn(p.instance);
			p.instance = nullptr;
		}
		if (p.lib) {
			closeLibrary(p.lib);
			p.lib = nullptr;
		}
	}
	plugins_.clear();
}

} // namespace core

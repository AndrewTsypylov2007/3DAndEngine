// src/core/LibraryLoader.cpp
#include "../../include/core/LibraryLoader.h"
#include "../../include/core/ServiceManager.h"
#include <iostream>
#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace core {

	LibraryLoader::~LibraryLoader() {
		unloadAll();
	}

	// Платформенные хелперы для работы с динамической памятью OS
	static LibHandle openLibrary(const std::string& path) {
#if defined(_WIN32)
		return (LibHandle)LoadLibraryA(path.c_str());
#else
		return dlopen(path.c_str(), RTLD_NOW);
#endif
	}

	static void closeLibrary(LibHandle h) {
		if (!h) return;
#if defined(_WIN32)
		FreeLibrary((HMODULE)h);
#else
		dlclose(h);
#endif
	}

	static void* getSymbol(LibHandle h, const char* name) {
#if defined(_WIN32)
		return (void*)GetProcAddress((HMODULE)h, name);
#else
		return dlsym(h, name);
#endif
	}

	int LibraryLoader::discoverAndLoad(const std::string& directory) {
		namespace fs = std::filesystem;
		int loaded = 0;
		try {
			fs::path dir(directory);
			if (!fs::exists(dir) || !fs::is_directory(dir)) return 0;

			for (auto& entry : fs::recursive_directory_iterator(dir)) {
				if (!entry.is_regular_file()) continue;
				auto p = entry.path();

#if defined(_WIN32)
				if (p.extension() != ".dll") continue;
#elif defined(__APPLE__)
				if (p.extension() != ".dylib") continue;
#else
				if (p.extension() != ".so") continue;
#endif

				if (loadLibrary(p.string())) {
					++loaded;
				}
			}
		}
		catch (const std::exception& e) {
			std::cerr << "[LibraryLoader] discoverAndLoad error: " << e.what() << "\n";
		}
		return loaded;
	}

	bool LibraryLoader::loadLibrary(const std::string& path) {
		std::cerr << "[LibraryLoader] Loading: " << path << "\n";
		LibHandle lib = openLibrary(path);
		if (!lib) {
#if defined(_WIN32)
			std::cerr << "[LibraryLoader] Failed to load binary: " << path << "\n";
#else
			std::cerr << "[LibraryLoader] Failed to load binary: " << path << " error: " << dlerror() << "\n";
#endif
			return false;
		}

		// Сразу фиксируем в векторе для предотвращения утечек при исключениях пуша
		LoadedLibrary ll;
		ll.lib = lib;
		ll.path = path;
		libraries_.push_back(std::move(ll));

		std::cerr << "[LibraryLoader] Successfully loaded into process memory: " << path << "\n";
		return true;
	}

	bool LibraryLoader::initializeAll(ServiceManager& services) {
		// Сигнатура функции, которую мы будем искать в каждой DLL для инициализации модуля
		using InitializeModuleFn = void (*)(ServiceManager&);

		for (auto& libInfo : libraries_) {
			// Каждая сторонняя DLL может экспортировать функцию "InitializeModule"
			auto initFn = (InitializeModuleFn)getSymbol(libInfo.lib, "InitializeModule");
			if (initFn) {
				try {
					std::cerr << "[LibraryLoader] Initializing module via function export: " << libInfo.path << "\n";
					initFn(services);
				}
				catch (const std::exception& e) {
					std::cerr << "[LibraryLoader] Exception during init of " << libInfo.path << ": " << e.what() << "\n";
					return false;
				}
				catch (...) {
					std::cerr << "[LibraryLoader] Unknown crash during init of " << libInfo.path << "\n";
					return false;
				}
			}
			else {
				// Если функции инициализации нет — это нормально, возможно это просто библиотека ресурсов или утилит
				std::cerr << "[LibraryLoader] Optional: No 'InitializeModule' export found in " << libInfo.path << "\n";
			}
		}
		return true;
	}

	void LibraryLoader::unloadAll() {
		// Выгружаем библиотеки строго в обратном порядке (LIFO)
		for (auto it = libraries_.rbegin(); it != libraries_.rend(); ++it) {
			if (it->lib) {
				std::cerr << "[LibraryLoader] Unloading binary: " << it->path << "\n";
				closeLibrary(it->lib);
				it->lib = nullptr;
			}
		}
		libraries_.clear();
	}

} // namespace core

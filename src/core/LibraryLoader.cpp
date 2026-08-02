// src/core/LibraryLoader.cpp — Версия v0.2.0 (True Crossplatform)
#include "../../include/core/LibraryLoader.h"
#include "../../include/core/ServiceManager.h"
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h> // Подключаем ТОЛЬКО на Windows
#else
#include <dlfcn.h>   // Подключаем на Linux, macOS и консолях (POSIX стандарт)
#endif

namespace core {

    LibraryLoader::LibraryLoader(ServiceManager& services) : services_(services) {}
    LibraryLoader::~LibraryLoader() { unloadAll(); }

    bool LibraryLoader::loadPlugin(const std::string& path) {
        void* hModule = nullptr;

        // 1. Загрузка библиотеки в зависимости от ОС
#if defined(_WIN32) || defined(_WIN64)
        hModule = (void*)LoadLibraryA(path.c_str());
#else
        hModule = dlopen(path.c_str(), RTLD_NOW); // POSIX аналог LoadLibrary
#endif

        if (!hModule) {
            std::cerr << "[Core::Loader] CRITICAL: Failed to load module: " << path << std::endl;
#if !defined(_WIN32) && !defined(_WIN64)
            std::cerr << "[Core::Loader] OS Error: " << dlerror() << std::endl; // Вывод ошибки Linux/macOS
#endif
            return false;
        }

        // 2. Поиск фабричной функции CreatePlugin
        typedef core::IService* (*CreatePluginFunc)();
        CreatePluginFunc createPlugin = nullptr;

#if defined(_WIN32) || defined(_WIN64)
        createPlugin = (CreatePluginFunc)GetProcAddress((HMODULE)hModule, "CreatePlugin");
#else
        createPlugin = (CreatePluginFunc)dlsym(hModule, "CreatePlugin"); // POSIX аналог GetProcAddress
#endif

        if (!createPlugin) {
            std::cerr << "[Core::Loader] CRITICAL: Export 'CreatePlugin' missing in " << path << std::endl;
#if defined(_WIN32) || defined(_WIN64)
            FreeLibrary((HMODULE)hModule);
#else
            dlclose(hModule);
#endif
            return false;
        }

        // 3. Активация плагина и безопасная упаковка памяти в кучу ядра
        core::IService* rawPlugin = createPlugin();
        if (rawPlugin) {
            m_loadedModules[path] = hModule;
            services_.registerService(std::unique_ptr<core::IService>(rawPlugin));
            return true;
        }

        return false;
    }

    void LibraryLoader::unloadPlugin(const std::string& path) {
        auto it = m_loadedModules.find(path);
        if (it != m_loadedModules.end()) {
#if defined(_WIN32) || defined(_WIN64)
            FreeLibrary((HMODULE)it->second);
#else
            dlclose(it->second);
#endif
            m_loadedModules.erase(it);
        }
    }

    void LibraryLoader::unloadAll() {
        for (auto& pair : m_loadedModules) {
#if defined(_WIN32) || defined(_WIN64)
            FreeLibrary((HMODULE)pair.second);
#else
            dlclose(pair.second);
#endif
        }
        m_loadedModules.clear();
    }
}

// src/core/LibraryLoader.cpp — Версия v0.1.5
#include "../../include/core/LibraryLoader.h"
#include "../../include/core/ServiceManager.h"
#include <windows.h>
#include <iostream>

namespace core {

    LibraryLoader::LibraryLoader(ServiceManager& services) : services_(services) {}
    LibraryLoader::~LibraryLoader() { unloadAll(); }

    bool LibraryLoader::loadPlugin(const std::string& path) {
        if (m_loadedModules.find(path) != m_loadedModules.end()) {
            unloadPlugin(path);
        }

        HMODULE hModule = LoadLibraryA(path.c_str());
        if (!hModule) {
            DWORD err = GetLastError(); // Точка 6: Логируем системный код сбоя OS
            std::cerr << "[Core::Loader] CRITICAL: Failed to load DLL: " << path << " | System Error Code: " << err << std::endl;
            return false;
        }

        typedef void (*InitFunc)(ServiceManager&);
        InitFunc initializeModule = (InitFunc)GetProcAddress(hModule, "InitializeModule");
        if (!initializeModule) {
            std::cerr << "[Core::Loader] InitializeModule export missing in " << path << std::endl;
            FreeLibrary(hModule);
            return false;
        }

        m_loadedModules[path] = static_cast<void*>(hModule);
        initializeModule(services_);
        return true;
    }

    void LibraryLoader::unloadPlugin(const std::string& path) {
        auto it = m_loadedModules.find(path);
        if (it == m_loadedModules.end()) return;
        FreeLibrary(static_cast<HMODULE>(it->second));
        m_loadedModules.erase(it);
    }

    void LibraryLoader::unloadAll() {
        auto it = m_loadedModules.begin();
        while (it != m_loadedModules.end()) {
            FreeLibrary(static_cast<HMODULE>(it->second));
            it = m_loadedModules.erase(it);
        }
    }
}

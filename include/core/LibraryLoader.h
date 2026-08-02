// include/core/LibraryLoader.h — Версия v0.1.5
#pragma once
#include <string>
#include <map>

namespace core {
    class ServiceManager;

    class LibraryLoader {
    private:
        ServiceManager& services_;
        std::map<std::string, void*> m_loadedModules;

    public:
        LibraryLoader(ServiceManager& services);
        ~LibraryLoader();

        bool loadPlugin(const std::string& path);
        void unloadPlugin(const std::string& path);
        void unloadAll();
    };
}

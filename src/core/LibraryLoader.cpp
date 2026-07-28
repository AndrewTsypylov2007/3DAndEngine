#include "../../include/core/LibraryLoader.h"
#include "../../include/core/ServiceManager.h"
#include <iostream>
#include <filesystem> // Для сканирования папок

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace core {

    LibraryLoader::LibraryLoader() {}

    LibraryLoader::~LibraryLoader() {
        unloadAll();
    }

    void* LibraryLoader::openLibrary(const std::string& path) {
#ifdef _WIN32
        return (void*)LoadLibraryA(path.c_str());
#else
        return dlopen(path.c_str(), RTLD_NOW);
#endif
    }

    void LibraryLoader::closeLibrary(void* handle) {
        if (!handle) return;
#ifdef _WIN32
        FreeLibrary((HMODULE)handle);
#else
        dlclose(handle);
#endif
    }

    bool LibraryLoader::loadLibrary(const std::string& path) {
        // Если библиотека уже загружена, не грузим второй раз
        if (libraries_.find(path) != libraries_.end()) return true;

        void* h = openLibrary(path);
        if (!h) {
            // Не выводим ошибку здесь, так как сканер может натыкаться на не-DLL файлы
            return false;
        }

        LoadedLibrary ll;
        ll.path = path;
        ll.handle = h;
        libraries_[path] = ll;

        std::cerr << "[LibraryLoader] Successfully loaded into process memory: " << path << std::endl;
        return true;
    }

    void* LibraryLoader::getSymbol(const std::string& libPath, const std::string& symbol) {
        auto it = libraries_.find(libPath);
        if (it == libraries_.end()) return nullptr;

#ifdef _WIN32
        return (void*)GetProcAddress((HMODULE)it->second.handle, symbol.c_str());
#else
        return dlsym(it->second.handle, symbol.c_str());
#endif
    }

    // ИСПРАВЛЕНИЕ: Реализация метода, который искал линкер (LNK2019)
    int LibraryLoader::discoverAndLoad(const std::string& path, ServiceManager& services) {
        int found = 0;
        namespace fs = std::filesystem;

        try {
            if (!fs::exists(path)) return 0;

            // Если передана папка — сканируем её
            if (fs::is_directory(path)) {
                for (const auto& entry : fs::directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
#ifdef _WIN32
                        if (ext == ".dll")
#else
                        if (ext == ".so")
#endif
                        {
                            if (loadLibrary(entry.path().string())) found++;
                        }
                    }
                }
            }
            // Если передан прямой путь к файлу
            else if (fs::is_regular_file(path)) {
                if (loadLibrary(path)) found++;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[LibraryLoader] Discovery error: " << e.what() << std::endl;
        }

        return found;
    }

    void LibraryLoader::initializeAll(ServiceManager& services) {
        for (auto& pair : libraries_) {
            auto& libInfo = pair.second;

            // Ищем экспортируемую функцию InitializeModule
            typedef void (*InitFunc)(ServiceManager&);
            InitFunc init = (InitFunc)getSymbol(libInfo.path, "InitializeModule");

            if (init) {
                std::cerr << "[LibraryLoader] Initializing module via function export: " << libInfo.path << std::endl;
                init(services);
            }
        }
    }

    void LibraryLoader::unloadAll() {
        for (auto& pair : libraries_) {
            closeLibrary(pair.second.handle);
        }
        libraries_.clear();
    }

    void LibraryLoader::stop() {
        unloadAll();
    }

} // namespace core

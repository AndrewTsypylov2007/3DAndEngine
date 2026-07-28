#pragma once

#include "IService.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace core {

    // 1. Объявляем структуру, которую не видел компилятор (LoadedLibrary)
    struct LoadedLibrary {
        std::string path;
        void* handle = nullptr;
    };

    class LibraryLoader : public IService {
    public:
        LibraryLoader();
        ~LibraryLoader();

        // Паспорт сервиса v0.1.1
        std::string getServiceName() const override { return "LibraryLoader"; }

        // Методы жизненного цикла (IService)
        bool init(ServiceManager& services) override { return true; }
        void start() override {}
        void stop() override;

        // Основные методы загрузки
        bool loadLibrary(const std::string& path);
        void* getSymbol(const std::string& libPath, const std::string& symbol);

        // Массовые операции (используются в Application.cpp)
        int discoverAndLoad(const std::string& path, ServiceManager& services);
        void initializeAll(ServiceManager& services);
        void unloadAll();

    private:
        // Внутренние системные функции (openLibrary / closeLibrary)
        void* openLibrary(const std::string& path);
        void closeLibrary(void* handle);

        // 2. Тот самый "необъявленный" контейнер (libraries_)
        std::unordered_map<std::string, LoadedLibrary> libraries_;
    };

} // namespace core

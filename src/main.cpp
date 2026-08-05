#include "../include/core/Application.h"
#include "../include/core/PluginLoader.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <memory>

void run_all_core_tests();

int main() {
    std::cout << "=== 3DAndEngine Runtime v2.0 (AAA Architecture) ===\n";

    // 1. Автоматический прогон тестов перед запуском микроядра
    try {
        run_all_core_tests();
    }
    catch (const std::exception& e) {
        std::cerr << "[Fatal Error] Unit tests failed! Engine startup aborted. Reason: " << e.what() << "\n";
        return 1;
    }

    // 2. Инициализация приложения и динамический рекурсивный поиск плагинов
    try {
        core::Application app;

        // Массив для удержания дескрипторов загруженных библиотек в виртуальной памяти
        std::vector<std::unique_ptr<core::LoadedPlugin>> runtime_modules;

        std::filesystem::path plugin_dir = "plugins";

        // Создаем папку, если её физически нет, чтобы не сломать итератор
        if (!std::filesystem::exists(plugin_dir)) {
            std::filesystem::create_directory(plugin_dir);
        }

        std::cout << "[Core] Initializing recursive plugin discovery in: " << plugin_dir.string() << "\n";

        // Определяем системное бинарное расширение модуля
#if defined(_WIN32)
        const std::string target_ext = ".dll";
#elif defined(__APPLE__)
        const std::string target_ext = ".dylib";
#else
        const std::string target_ext = ".so";
#endif

        // РЕКУРСИВНЫЙ ПОИСК: вслепую сканируем папку и все подпапки
        for (const auto& entry : std::filesystem::recursive_directory_iterator(plugin_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == target_ext) {

                std::cout << "[Core] Found potential module: " << entry.path().string() << "\n";

                // Динамически загружаем бинарник в память процесса
                auto plugin = core::PluginLoader::load(entry.path());
                if (plugin) {
                    // Регистрируем контракт плагина в конвейере ядра
                    app.registerPlugin(plugin->getAPI());
                    // Сохраняем умный указатель, чтобы библиотека не выгрузилась до остановки приложения
                    runtime_modules.push_back(std::move(plugin));
                }
            }
        }

        if (runtime_modules.empty()) {
            std::cout << "[Core WARNING] Zero payload active. No runtime modules discovery execution registered.\n";
        }

        std::cout << "[Core] Infrastructure initialized. Entering engine frame loop with "
            << runtime_modules.size() << " active module(s)...\n";

        // 3. Запуск конвейера кадра. Теперь, если рекурсивный поиск нашел окно, ядро НЕ вылетит!
        app.run();

        std::cout << "[Core] Engine runtime executed shutdown successfully.\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[Fatal Error] Core crashed! Reason: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
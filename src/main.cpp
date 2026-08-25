#include "../include/core/Application.h"
#include "../include/core/PluginLoader.h"
#include <iostream>
#include <vector>
#include <memory>
#include <filesystem>

// Декларация функции тестирования ядра
void run_all_core_tests();

int main() {
    std::cout << "=================================================================\n";
    std::cout << "  3DAndEngine Runtime v0.4.0 (Pure Blind AAA Architecture)\n";
    std::cout << "  Engine ABI Version: 0x" << std::hex << core::ENGINE_ABI_VERSION << std::dec << "\n";
    std::cout << "=================================================================\n\n";

    // 1. АВТОМАТИЧЕСКИЙ ПРОГОН ТЕСТОВ (Failsafe Self-Test)
    try {
        run_all_core_tests();
    }
    catch (const std::exception& e) {
        std::cerr << "[Fatal Error] Core self-tests failed! Startup aborted. Reason: " << e.what() << "\n";
        return 1;
    }

    // 2. ИНИЦИАЛИЗАЦИЯ ИНФРАСТРУКТУРЫ И СЛЕПОЙ ПОИСК ПЛАГИНОВ
    try {
        core::EngineConfig config;
        config.fixed_timestep = 1.0 / 60.0;
        config.max_delta_time = 0.1f;

        auto app = std::make_unique<core::Application>(config);

        std::filesystem::path exe_dir = core::platform::get_executable_dir();
        std::filesystem::path primary_plugins_path = exe_dir / "plugins";
        std::filesystem::path fallback_plugins_path = "plugins";

        std::cout << "[Core v0.4.0] Запуск слепого рекурсивного обнаружения модулей...\n";

        std::vector<std::unique_ptr<core::LoadedPlugin>> runtime_modules;

        if (std::filesystem::exists(primary_plugins_path)) {
            runtime_modules = core::PluginManager::discover_recursive(primary_plugins_path);
        }
        else if (std::filesystem::exists(fallback_plugins_path)) {
            runtime_modules = core::PluginManager::discover_recursive(fallback_plugins_path);
        }
        else {
            std::filesystem::create_directories(fallback_plugins_path);
            runtime_modules = core::PluginManager::discover_flat(fallback_plugins_path);
        }

        if (runtime_modules.empty()) {
            std::cout << "[Core WARNING] Активных полезных модулей не обнаружено. Движок запущен в Headless/Idle режиме.\n";
        }
        else {
            std::cout << "[Core] Обнаружено и верифицировано модулей: " << runtime_modules.size() << "\n";
        }

        for (auto& module : runtime_modules) {
            std::cout << "  -> Подключение плагина: [" << module->name()
                << "] (Приоритет: " << module->priority() << ")\n";
            app->registerPlugin(module->get());
        }

        std::cout << "\n[Core] Инициализация завершена. Переход в высокоскоростной кадровый конвейер...\n\n";

        // 3. ЗАПУСК ИГРОВОГО ЦИКЛА
        app->run();

        std::cout << "\n[Core] Движок v0.4.0 завершил работу в штатном режиме. Все ресурсы освобождены.\n";
    }
    catch (const std::exception& e) {
        std::cerr << "\n!!! [FATAL CORE CRASH] !!!\n";
        std::cerr << "Reason: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
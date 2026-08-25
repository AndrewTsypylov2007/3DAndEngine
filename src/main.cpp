#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "../include/core/Application.h"
#include "../include/core/PluginLoader.h"
#include <iostream>
#include <vector>
#include <memory>
#include <filesystem>
#include <cstdlib>
#include <string>

// Декларация функции самотестирования ядра
void run_all_core_tests();

// Безопасный кроссплатформенный хелпер для проверки переменных окружения
inline bool has_env_flag(const char* name) {
#if defined(_MSC_VER)
    char* val = nullptr;
    size_t len = 0;
    if (_dupenv_s(&val, &len, name) == 0 && val != nullptr) {
        free(val);
        return true;
    }
    return false;
#else
    return std::getenv(name) != nullptr;
#endif
}

int main(int argc, char* argv[]) {
#if defined(_WIN32)
    // Установка кодовой страницы консоли Windows в UTF-8
    ::SetConsoleOutputCP(CP_UTF8);
    ::SetConsoleCP(CP_UTF8);
#endif

    std::cout << "=================================================================\n";
    std::cout << "  3DAndEngine Runtime v0.4.0 (Pure Blind AAA Architecture)\n";
    std::cout << "  Engine ABI Version: 0x" << std::hex << core::ENGINE_ABI_VERSION << std::dec << "\n";
    std::cout << "=================================================================\n\n";

    // Автоматическое определение режима CI и автономных тестов
    bool is_ci_mode = has_env_flag("CI") || has_env_flag("GITHUB_ACTIONS");
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test" || arg == "--ci" || arg == "--headless") {
            is_ci_mode = true;
        }
    }

    // 1. АВТОМАТИЧЕСКИЙ ПРОГОН САМОТЕСТИРОВАНИЯ ЯДРА (Failsafe Self-Test)
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

        // В CI или автономном тесте выполняем 30 кадров и штатно выходим
        if (is_ci_mode) {
            config.max_frames = 30;
            std::cout << "[Core CI] Режим автономного тестирования активен (лимит: 30 кадров).\n";
        }

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
            std::cout << "[Core WARNING] Активных полезных модулей не обнаружено. Headless-валидация...\n";
            if (config.max_frames == 0) {
                config.max_frames = 5;
            }
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
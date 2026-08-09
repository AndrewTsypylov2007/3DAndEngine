#include "../include/core/Application.h"
#include "../include/core/PluginLoader.h"
#include <iostream>
#include <vector>
#include <memory>

// Тесты ядра (Unit Tests)
void run_all_core_tests();

int main() {
    std::cout << "=== 3DAndEngine Runtime v0.3.0 (Commercial AAA Architecture) ===\n";

    // 1. АВТОМАТИЧЕСКИЙ ПРОГОН ТЕСТОВ (Failsafe)
    try {
        run_all_core_tests();
    }
    catch (const std::exception& e) {
        std::cerr << "[Fatal Error] Unit tests failed! Engine startup aborted. Reason: " << e.what() << "\n";
        return 1;
    }

    // 2. ИНИЦИАЛИЗАЦИЯ ИНФРАСТРУКТУРЫ
    try {
        // Создаем экземпляр приложения (Материнская плата)
        core::Application app;

        // ПУТЬ К ПЛАГИНАМ: Теперь это единственное, что знает ядро о файлах
        const std::string plugin_path = "plugins";

        // ДИНАМИЧЕСКИЙ ПОИСК И ЗАГРУЗКА (Discovery)
        // PluginManager::discover теперь сам:
        // - Рекурсивно находит файлы
        // - Определяет расширения (.dll / .so)
        // - Сортирует их по Priority (Window(0) -> Renderer(10) -> Game(100))
        auto runtime_modules = core::PluginManager::discover(plugin_path);

        if (runtime_modules.empty()) {
            std::cout << "[Core WARNING] Zero payload modules active. Engine running in headless/idle mode.\n";
        }

        // РЕГИСТРАЦИЯ ПЛАГИНОВ В ЯДРЕ
        for (auto& module : runtime_modules) {
            // При регистрации вызывается on_load, где плагин может:
            // - Получить доступ к ECS/EventBus
            // - Зарегистрировать свои API в Service Locator (RegisterSystem)
            app.registerPlugin(module->get());
        }

        std::cout << "[Core] Infrastructure v0.3.0 initialized. Active modules: "
            << runtime_modules.size() << "\n";

        // 3. ЗАПУСК ИГРОВОГО ЦИКЛА (Frame Loop)
        // Теперь если загружен плагин "UniversalWindow", цикл начнется.
        // Если окно закроется, оно отправит "engine/exit", и app.run() завершится.
        app.run();

        std::cout << "[Core] Engine runtime executed shutdown successfully.\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[Fatal Error] Core crashed! Reason: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

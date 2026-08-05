#include "../include/core/Application.h"
#include "../include/core/PluginLoader.h"
#include <iostream>

// Объявляем функцию тестов, чтобы компилятор знал о ней (Forward Declaration)
void run_all_core_tests();

int main() {
    std::cout << "=== 3DAndEngine v0.2.0 ===\n";

    // Автоматический прогон тестов перед запуском микроядра
    try {
        run_all_core_tests();
    }
    catch (const std::exception& e) {
        std::cerr << "[Fatal Error] Unit tests failed! Engine startup aborted. Reason: " << e.what() << "\n";
        return 1;
    }

    // Запуск самого приложения движка
    try {
        core::Application app;

        std::cout << "[Core] Infrastructure initialized. Entering engine frame loop...\n";

        // Запуск бесконечного Main Loop
        app.run();

        std::cout << "[Core] Engine runtime executed shutdown successfully.\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[Fatal Error] Core crashed! Reason: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

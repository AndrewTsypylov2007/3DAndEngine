// ==============================================================================
// 1. СИСТЕМНЫЕ ИНКЛУДЫ (Строго первыми до заголовочных файлов SDK!)
// ==============================================================================
#include <iostream>
#include <algorithm>
#include <vector>
#include <cassert>
#include <string>
#include <memory>

// ==============================================================================
// 2. ИНКЛУДЫ ПОДСИСТЕМ ДВИЖКА (v0.3.0 Commercial Standard)
// ==============================================================================
#include "../include/core/Types.h"
#include "../include/core/EcsRegistry.h"
#include "../include/core/EventBus.h"
#include "../include/core/JobSystem.h"
#include "../include/core/Application.h"

// Тестовые Data-Oriented компоненты для ECS
struct Transform {
    float x;
    float y;
};

struct Velocity {
    float vx;
    float vy;
};

// ==============================================================================
// 1. ТЕСТ 64-БИТНОГО ХЕШИРОВАНИЯ (FNV-1a Compile-Time Литералы)
// ==============================================================================
void test_commercial_hashing() {
    using namespace core;

    // Проверяем 64-битную точность генерации compile-time идентификаторов
    constexpr uint64_t hash1 = core::hash_str("renderer/main_pass");
    constexpr uint64_t hash2 = "renderer/main_pass"_id;

    assert(hash1 == hash2);
    assert(hash1 > 0xFFFFFFFFull); // Жесткая верификация выхода за границы 32-битного пространства

    std::cout << "[Test Passed] 64-bit FNV-1a Hashing works.\n";
}

// ==============================================================================
// 2. ТЕСТ РЕАКТИВНОГО ECS (Потокобезопасные Listeners & Hooks)
// ==============================================================================
void test_reactive_ecs() {
    core::EcsRegistry registry;
    bool component_added = false;

    // Регистрируем реактивного "слушателя" изменений памяти ECS буфера
    core::EcsListener listener;
    listener.on_component_added = [&](core::Entity [[maybe_unused]] ent, core::ComponentTypeId [[maybe_unused]] id) {
        component_added = true;
        };
    registry.addListener(listener);

    core::Entity e1 = registry.createEntity();
    registry.addComponent<Transform>(e1, Transform{ 100.0f, 200.0f });

    assert(component_added == true); // Проверяем, что шина данных ECS успешно вызвала hook

    Transform* t = registry.getComponent<Transform>(e1);
    assert(t != nullptr);
    assert(t->x == 100.0f);

    std::cout << "[Test Passed] Reactive ECS & Listeners work.\n";
}

// ==============================================================================
// 3. ТЕСТ ГИБРИДНОЙ ШИНЫ СОБЫТИЙ (Синхронные Callbacks и Кольцевой Буфер)
// ==============================================================================
void test_hybrid_eventbus() {
    using namespace core;
    core::EventBus event_bus;
    bool immediate_reacted = false;

    // Подписываемся на мгновенное синхронное прерывание кадра
    event_bus.subscribe("engine/exit"_id, [&](uint64_t [[maybe_unused]] payload) {
        immediate_reacted = true;
        });

    // Отправляем широковещательный пакет в систему
    event_bus.broadcast("engine/exit"_id, 1);

    assert(immediate_reacted == true);     // Проверяем синхронную реакцию в ту же микросекунду
    assert(event_bus.getEventsCount() == 1); // Проверяем параллельное сохранение пакета в буфер кадра

    std::cout << "[Test Passed] Hybrid EventBus (Immediate/Buffered) works.\n";
}

// ==============================================================================
// 4. ТЕСТ SERVICE LOCATOR (Интерфейсная коммутационная матрица плагинов)
// ==============================================================================
void test_service_locator() {
    using namespace core;

    // Имитируем регистрацию API условного плагина в рантийном мосту
    struct MockRenderAPI { int version = 30; } mock_render;

    SystemBridge::RegisterSystem(sys_id::Renderer, &mock_render);

    void* retrieved = SystemBridge::GetSystem(sys_id::Renderer);
    assert(retrieved != nullptr);
    assert(static_cast<MockRenderAPI*>(retrieved)->version == 30);

    std::cout << "[Test Passed] Service Locator Registry works.\n";
}

// ==============================================================================
// ТОЧКА ВХОДА ДЛЯ КОНВЕЙЕРА АВТОМАТИЧЕСКОГО ТЕСТИРОВАНИЯ ЯДРА
// ==============================================================================
void run_all_core_tests() {
    std::cout << "\n=== Starting 3DAndEngine Core Unit Tests v0.3.0 ===\n";

    try {
        test_commercial_hashing();
        test_reactive_ecs();
        test_hybrid_eventbus();
        test_service_locator();

        std::cout << "=== ALL CORE TESTS PASSED SUCCESSFULLY ===\n\n";
    }
    catch (const std::exception& e) {
        std::cerr << "!!! [UNIT TEST FATAL] !!!\n";
        std::cerr << "Reason: " << e.what() << "\n";
        throw;
    }
}

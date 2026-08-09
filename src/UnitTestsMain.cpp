#include "../include/core/Types.h"
#include "../include/core/EcsRegistry.h"
#include "../include/core/EventBus.h"
#include "../include/core/JobSystem.h"
#include "../include/core/Application.h"
#include <iostream>
#include <cassert>  
#include <vector>

// Тестовые компоненты
struct Transform {
    float x, y;
};

struct Velocity {
    float vx, vy;
};

// 1. Тест 64-битного хеширования (Коммерческий стандарт)
void test_commercial_hashing() {
    using namespace core;
    // Проверяем 64-битную точность и compile-time генерацию
    constexpr uint64_t hash1 = core::hash_str("renderer/main_pass");
    constexpr uint64_t hash2 = "renderer/main_pass"_id;

    assert(hash1 == hash2);
    assert(hash1 > 0xFFFFFFFF); // Гарантируем, что хеш вышел за пределы 32 бит

    std::cout << "[Test Passed] 64-bit FNV-1a Hashing works.\n";
}

// 2. Тест реактивного ECS (Hooks & Thread Safety)
void test_reactive_ecs() {
    core::EcsRegistry registry;
    bool component_added = false;

    // Регистрируем "слушателя" (v0.3.0 Feature)
    core::EcsListener listener;
    listener.on_component_added = [&](core::Entity ent, core::ComponentTypeId id) {
        component_added = true;
        };
    registry.addListener(listener);

    core::Entity e1 = registry.createEntity();
    registry.addComponent<Transform>(e1, { 100.0f, 200.0f });

    assert(component_added == true); // Проверяем, что ECS "крикнул" о новом компоненте

    Transform* t = registry.getComponent<Transform>(e1);
    assert(t != nullptr && t->x == 100.0f);

    std::cout << "[Test Passed] Reactive ECS & Listeners work.\n";
}

// 3. Тест гибридной шины событий (Immediate & Buffered)
void test_hybrid_eventbus() {
    using namespace core;
    core::EventBus event_bus;
    bool immediate_reacted = false;

    // Подписываемся на мгновенное событие (v0.3.0 Feature)
    event_bus.subscribe("engine/exit"_id, [&](uint64_t payload) {
        immediate_reacted = true;
        });

    // Отправляем широковещательный сигнал
    event_bus.broadcast("engine/exit"_id, 1);

    assert(immediate_reacted == true); // Реакция должна быть мгновенной
    assert(event_bus.getEventsCount() == 1); // И событие должно сохраниться в буфере для истории

    std::cout << "[Test Passed] Hybrid EventBus (Immediate/Buffered) works.\n";
}

// 4. Тест Service Locator (Материнская плата ядра)
void test_service_locator() {
    using namespace core;
    // Эмулируем регистрацию системы
    struct MockRenderAPI { int version = 30; } mock_render;

    system_bridge::RegisterSystem(sys_id::Renderer, &mock_render);

    void* retrieved = system_bridge::GetSystem(sys_id::Renderer);
    assert(retrieved != nullptr);
    assert(static_cast<MockRenderAPI*>(retrieved)->version == 30);

    std::cout << "[Test Passed] Service Locator Registry works.\n";
}

// ГЛАВНЫЙ ВХОД ДЛЯ ТЕСТОВ ЯДРА
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
        throw; // Пробрасываем выше, чтобы остановить загрузку ядра
    }
}

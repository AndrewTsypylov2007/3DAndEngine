#include "../include/core/Types.h"
#include "../include/core/EcsRegistry.h"
#include "../include/core/EventBus.h"
#include "../include/core/JobSystem.h"
#include <iostream>
#include <cassert>  
#include <vector>

struct Transform {
    float x;
    float y;
};

void test_constexpr_hashing() {
    using namespace core;
    constexpr uint32_t hash1 = core::hash_str("physics/collision");
    constexpr uint32_t hash2 = "physics/collision"_id;
    assert(hash1 == hash2);
    std::cout << "[Test Passed] Hashing works.\n";
}

void test_ecs_dense_arrays() {
    core::EcsRegistry registry;

    core::Entity e1 = registry.createEntity();
    core::Entity e2 = registry.createEntity();

    registry.addComponent<Transform>(e1, Transform{ 10.0f, 20.0f });
    registry.addComponent<Transform>(e2, Transform{ 30.0f, 40.0f });

    Transform* t1 = registry.getComponent<Transform>(e1);
    assert(t1 != nullptr);
    assert(t1->x == 10.0f);

    std::cout << "[Test Passed] ECS works.\n";
}

void test_lockfree_eventbus() {
    using namespace core;
    core::EventBus event_bus;
    constexpr core::EventId move_id = "input/move"_id;

    event_bus.broadcast(move_id, 42);
    assert(event_bus.getEventsCount() == 1);

    std::cout << "[Test Passed] EventBus works.\n";
}

// ИСПРАВЛЕНО: Вместо конфликтующего main() теперь просто функция запуска тестов
void run_all_core_tests() {
    std::cout << "=== Running 3DAndEngine Core Unit Tests ===\n";

    test_constexpr_hashing();
    test_ecs_dense_arrays();
    test_lockfree_eventbus();

    std::cout << "=== ALL CORE TESTS PASSED SUCCESSFULLY ===\n";
}

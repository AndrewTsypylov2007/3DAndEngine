#include "../include/core/Types.h"
#include "../include/core/EcsRegistry.h"
#include "../include/core/EventBus.h"
#include "../include/core/JobSystem.h"

#include <iostream>
#include <vector>
#include <cassert>
#include <string>
#include <string_view>
#include <cstdint>

// ==============================================================================
// ТЕСТОВЫЕ СТРУКТУРЫ ДАННЫХ ДЛЯ ТЕСТОВ
// ==============================================================================
struct TestTransform {
    float x{ 0.0f };
    float y{ 0.0f };
};

struct TestVelocity {
    float vx{ 0.0f };
    float vy{ 0.0f };
};

// ==============================================================================
// 1. ТЕСТ 64-БИТНОГО FNV-1a ХЕШИРОВАНИЯ
// ==============================================================================
void test_commercial_hashing() {
    using namespace core;

    constexpr uint64_t hash1 = core::hash_str("renderer/main_pass");
    constexpr uint64_t hash2 = "renderer/main_pass"_id;

    assert(hash1 == hash2);
    assert(hash1 > 0xFFFFFFFFull);

    std::cout << "[Test Passed] 64-bit FNV-1a Hashing works.\n";
}

// ==============================================================================
// 2. ТЕСТ РЕАКТИВНОГО PAGED SPARSE-SET ECS
// ==============================================================================
void test_reactive_ecs() {
    using namespace core;

    EcsRegistry registry;
    bool component_added = false;

    EcsListener listener;
    listener.on_component_added = [&](Entity ent, ComponentTypeId id) {
        (void)ent;
        (void)id;
        component_added = true;
        };
    registry.addListener(listener);

    Entity e1 = registry.createEntity();
    registry.addComponent<TestTransform>(e1, TestTransform{ 100.0f, 200.0f });

    assert(component_added == true);

    TestTransform* t = registry.getComponent<TestTransform>(e1);
    assert(t != nullptr);
    assert(t->x == 100.0f);
    assert(t->y == 200.0f);

    std::cout << "[Test Passed] Reactive Paged Sparse-Set ECS & Listeners work.\n";
}

// ==============================================================================
// 3. ТЕСТ ГИБРИДНОЙ ШИНЫ СОБЫТИЙ
// ==============================================================================
void test_hybrid_eventbus() {
    using namespace core;

    EventBus event_bus;
    bool immediate_reacted = false;

    event_bus.subscribe("engine/exit"_id, [&](uint64_t payload) {
        (void)payload;
        immediate_reacted = true;
        });

    event_bus.dispatchImmediate("engine/exit"_id, 1);

    assert(immediate_reacted == true);
    assert(event_bus.eventCount() == 1);

    std::cout << "[Test Passed] Hybrid EventBus (Immediate/Buffered) works.\n";
}

// ==============================================================================
// 4. ТЕСТ МНОГОПОТОЧНОЙ JOB SYSTEM
// ==============================================================================
void test_job_system() {
    using namespace core;

    JobSystem job_system;
    job_system.initialize(2);

    std::atomic<int> counter{ 0 };
    JobCounter jc;

    for (int i = 0; i < 100; ++i) {
        job_system.run([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
            }, &jc);
    }

    job_system.wait(&jc);
    assert(counter.load() == 100);

    job_system.shutdown();
    std::cout << "[Test Passed] Multithreaded Lock-Free Job System works.\n";
}

// ==============================================================================
// ТОЧКА ВХОДА ТЕСТОВ (Вызывается из main.cpp)
// ==============================================================================
void run_all_core_tests() {
    std::cout << "\n=== Starting 3DAndEngine Core Unit Tests v0.4.0 ===\n";

    try {
        test_commercial_hashing();
        test_reactive_ecs();
        test_hybrid_eventbus();
        test_job_system();

        std::cout << "=== ALL CORE TESTS PASSED SUCCESSFULLY ===\n\n";
    }
    catch (const std::exception& e) {
        std::cerr << "!!! [UNIT TEST FATAL] !!!\n";
        std::cerr << "Reason: " << e.what() << "\n";
        throw;
    }
}
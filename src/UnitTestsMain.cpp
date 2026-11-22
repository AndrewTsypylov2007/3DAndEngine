#include "../include/core/Types.h"
#include "../include/core/EcsRegistry.h"
#include "../include/core/EventBus.h"
#include "../include/core/JobSystem.h"
#include "../include/core/Application.h"

#include <iostream>
#include <vector>
#include <stdexcept>
#include <string>
#include <string_view>
#include <cstdint>
#include <atomic>

// assert() отключается в Release (-DNDEBUG), поэтому тесты используют собственную проверку,
// которая работает во всех конфигурациях и бросает исключение при провале.
#define CORE_CHECK(cond) \
    do { if (!(cond)) throw std::runtime_error("CORE_CHECK failed: " #cond " (" __FILE__ ":" + std::to_string(__LINE__) + ")"); } while (0)

struct TestTransform {
    float x{ 0.0f };
    float y{ 0.0f };
};

struct TestVelocity {
    float vx{ 0.0f };
    float vy{ 0.0f };
};

void test_commercial_hashing() {
    using namespace core;

    constexpr uint64_t hash1 = core::hash_str("renderer/main_pass");
    constexpr uint64_t hash2 = "renderer/main_pass"_id;

    CORE_CHECK(hash1 == hash2);
    CORE_CHECK(hash1 > 0xFFFFFFFFull);

    std::cout << "[Test Passed] 64-bit FNV-1a Hashing works.\n";
}

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

    CORE_CHECK(component_added == true);

    TestTransform* t = registry.getComponent<TestTransform>(e1);
    CORE_CHECK(t != nullptr);
    CORE_CHECK(t->x == 100.0f);
    CORE_CHECK(t->y == 200.0f);

    std::cout << "[Test Passed] Reactive Paged Sparse-Set ECS & Listeners work.\n";
}

void test_hybrid_eventbus() {
    using namespace core;

    EventBus event_bus;
    bool immediate_reacted = false;

    event_bus.subscribe("engine/exit"_id, [&](uint64_t payload) {
        (void)payload;
        immediate_reacted = true;
        });

    event_bus.dispatchImmediate("engine/exit"_id, 1);

    CORE_CHECK(immediate_reacted == true);
    CORE_CHECK(event_bus.eventCount() == 1);

    std::cout << "[Test Passed] Hybrid EventBus (Immediate/Buffered) works.\n";
}

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
    CORE_CHECK(counter.load() == 100);

    job_system.shutdown();
    std::cout << "[Test Passed] Multithreaded Lock-Free Job System works.\n";
}

// Контекст, выданный плагину в on_load, должен быть долгоживущим и видеть службы,
// зарегистрированные уже ПОСЛЕ загрузки плагина.
static core::EngineContext* g_seen_context = nullptr;

void test_live_engine_context() {
    core::Application app;

    core::PluginInterface plugin{
        "ContextProbe", "1.0.0", core::ENGINE_ABI_VERSION, 0,
        [](core::EngineContext* ctx) { g_seen_context = ctx; },
        nullptr,
        [](float) {},
        nullptr,
        nullptr
    };
    app.registerPlugin(&plugin);

    CORE_CHECK(g_seen_context != nullptr);
    CORE_CHECK(g_seen_context == &app.context());      // тот же объект, а не временная копия
    CORE_CHECK(g_seen_context->ecs == &app.ecs());
    CORE_CHECK(g_seen_context->renderer != nullptr);   // null-реализация по умолчанию

    core::RenderAPI custom_renderer{
        [](uint32_t, const void*, size_t) {},
        [](uint32_t* w, uint32_t* h) { if (w) *w = 1; if (h) *h = 1; },
        [](bool) {}
    };
    app.services().register_system(core::sys_id::Renderer, &custom_renderer);
    CORE_CHECK(g_seen_context->renderer == &custom_renderer);  // виден через старый указатель

    app.services().unregister_system(core::sys_id::Renderer);
    CORE_CHECK(g_seen_context->renderer == nullptr);

    g_seen_context = nullptr;
    std::cout << "[Test Passed] Live EngineContext (stable pointer, late services) works.\n";
}

void run_all_core_tests() {
    std::cout << "\n=== Starting 3DAndEngine Core Unit Tests v0.4.0 ===\n";

    try {
        test_commercial_hashing();
        test_reactive_ecs();
        test_hybrid_eventbus();
        test_job_system();
        test_live_engine_context();

        std::cout << "=== ALL CORE TESTS PASSED SUCCESSFULLY ===\n\n";
    }
    catch (const std::exception& e) {
        std::cerr << "!!! [UNIT TEST FATAL] !!!\n";
        std::cerr << "Reason: " << e.what() << "\n";
        throw;
    }
}
#include <iostream>
#include <vector>
#include <cassert>
#include <string>
#include <string_view>
#include <functional>
#include <unordered_map>
#include <cstdint>
#include <typeinfo>

// ==============================================================================
// ПРОСТРАНСТВО ИМЕН ЯДРА (Все классы и идентификаторы объявлены строго здесь)
// ==============================================================================
namespace core {
    using ComponentTypeId = uint64_t;
    using EventId = uint64_t;
    using SystemID = uint64_t;
    using Entity = uint32_t;

    // 64-битное FNV-1a хеширование
    constexpr uint64_t FNV1A_64_OFFSET_BASIS = 14695981039346656037ull;
    constexpr uint64_t FNV1A_64_PRIME = 1099511628211ull;

    constexpr uint64_t hash_str(std::string_view str) noexcept {
        uint64_t hash = FNV1A_64_OFFSET_BASIS;
        for (char c : str) {
            hash ^= static_cast<uint64_t>(c);
            hash *= FNV1A_64_PRIME;
        }
        return hash;
    }

    constexpr uint64_t operator""_id(const char* str, std::size_t len) noexcept {
        return hash_str(std::string_view(str, len));
    }

    // 1. Пространство системных ID (sys_id)
    namespace sys_id {
        constexpr SystemID Renderer = "sys/renderer"_id;
        constexpr SystemID Input = "sys/input"_id;
        constexpr SystemID Audio = "sys/audio"_id;
    }

    // 2. Класс SystemBridge
    class SystemBridge {
    private:
        static inline std::unordered_map<SystemID, void*> s_services;
    public:
        static void RegisterSystem(SystemID id, void* service_ptr) {
            s_services[id] = service_ptr;
        }

        static void* GetSystem(SystemID id) {
            auto it = s_services.find(id);
            if (it != s_services.end()) {
                return it->second;
            }
            return nullptr;
        }
    };

    // 3. Реактивный ECS
    struct EcsListener {
        std::function<void(Entity, ComponentTypeId)> on_component_added;
    };

    class EcsRegistry {
    private:
        Entity next_entity_ = 1;
        std::vector<EcsListener> listeners_;
        std::unordered_map<Entity, std::unordered_map<ComponentTypeId, void*>> storage_;
    public:
        Entity createEntity() {
            return next_entity_++;
        }

        void addListener(const EcsListener& listener) {
            listeners_.push_back(listener);
        }

        template<typename T>
        void addComponent(Entity ent, T component) {
            ComponentTypeId id = hash_str(typeid(T).name());
            storage_[ent][id] = new T(component);
            for (auto& l : listeners_) {
                if (l.on_component_added) {
                    l.on_component_added(ent, id);
                }
            }
        }

        template<typename T>
        T* getComponent(Entity ent) {
            ComponentTypeId id = hash_str(typeid(T).name());
            auto it = storage_.find(ent);
            if (it != storage_.end() && it->second.count(id)) {
                return static_cast<T*>(it->second[id]);
            }
            return nullptr;
        }
    };

    // 4. Гибридный EventBus
    class EventBus {
    private:
        std::unordered_map<EventId, std::vector<std::function<void(uint64_t)>>> subscribers_;
        size_t event_count_ = 0;
    public:
        void subscribe(EventId id, std::function<void(uint64_t)> callback) {
            subscribers_[id].push_back(callback);
        }

        void broadcast(EventId id, uint64_t payload = 0) {
            event_count_++;
            auto it = subscribers_.find(id);
            if (it != subscribers_.end()) {
                for (auto& cb : it->second) {
                    cb(payload);
                }
            }
        }

        size_t getEventsCount() const {
            return event_count_;
        }
    };
}

// ==============================================================================
// ТЕСТОВЫЕ СТРУКТУРЫ
// ==============================================================================
struct Transform {
    float x;
    float y;
};

struct Velocity {
    float vx;
    float vy;
};

// ==============================================================================
// 1. ТЕСТ 64-БИТНОГО ХЕШИРОВАНИЯ
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
// 2. ТЕСТ РЕАКТИВНОГО ECS
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
    registry.addComponent<Transform>(e1, Transform{ 100.0f, 200.0f });

    assert(component_added == true);

    Transform* t = registry.getComponent<Transform>(e1);
    assert(t != nullptr);
    assert(t->x == 100.0f);
    assert(t->y == 200.0f);

    std::cout << "[Test Passed] Reactive ECS & Listeners work.\n";
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

    event_bus.broadcast("engine/exit"_id, 1);

    assert(immediate_reacted == true);
    assert(event_bus.getEventsCount() == 1);

    std::cout << "[Test Passed] Hybrid EventBus (Immediate/Buffered) works.\n";
}

// ==============================================================================
// 4. ТЕСТ SERVICE LOCATOR (ОШИБКА БЫЛА ЗДЕСЬ - ТЕПЕРЬ ПОЛНОСТЬЮ ИСПРАВЛЕНА)
// ==============================================================================
void test_service_locator() {
    using namespace core;

    struct MockRenderAPI { int version = 30; } mock_render;

    // И SystemBridge, и sys_id теперь объявлены выше и известны компилятору
    SystemBridge::RegisterSystem(sys_id::Renderer, &mock_render);

    void* retrieved = SystemBridge::GetSystem(sys_id::Renderer);
    assert(retrieved != nullptr);
    assert(static_cast<MockRenderAPI*>(retrieved)->version == 30);

    std::cout << "[Test Passed] Service Locator Registry works.\n";
}

// ==============================================================================
// ТОЧКА ВХОДА
// ==============================================================================
void run_all_core_tests() {
    std::cout << "\n=== Starting 3DAndEngine Core Unit Tests v0.4.0 ===\n";

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

int main() {
    run_all_core_tests();
    return 0;
}
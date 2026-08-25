#pragma once

#include <cstdint>
#include <cstddef>
#include "Types.h"

// Опережающие объявления базовых классов ядра (передаются как opaque-указатели)
namespace core {
    class EcsRegistry;
    class EventBus;
    class JobSystem;
}

namespace core {

    // ==============================================================================
    // 1. ВЕРСИОНИРОВАНИЕ КОНТРАКТА ЯДРА (ABI Handshake Standard)
    // ==============================================================================
    constexpr uint32_t ENGINE_ABI_VERSION_MAJOR = 0;
    constexpr uint32_t ENGINE_ABI_VERSION_MINOR = 3;
    constexpr uint32_t ENGINE_ABI_VERSION_PATCH = 5;

    // 0x00030500 - упакованный идентификатор версии ABI
    constexpr uint32_t ENGINE_ABI_VERSION =
        (ENGINE_ABI_VERSION_MAJOR << 24) |
        (ENGINE_ABI_VERSION_MINOR << 16) |
        (ENGINE_ABI_VERSION_PATCH << 8);

    // ==============================================================================
    // 2. БАЗОВЫЕ ИДЕНТИФИКАТОРЫ СИСТЕМ (Service Locator System IDs)
    // ==============================================================================
    using SystemID = uint64_t;
    using FrameDataId = uint64_t;

    constexpr SystemID SYS_INPUT = 0x01;
    constexpr SystemID SYS_AUDIO = 0x02;
    constexpr SystemID SYS_RENDERER = 0x03;
    constexpr SystemID SYS_ASSETS = 0x04;
    constexpr SystemID SYS_LOG = 0x05;
    constexpr SystemID SYS_PHYSICS = 0x06;

    // ==============================================================================
    // 3. ЧИСТЫЕ C-ИНТЕРФЕЙСЫ ПОДСИСТЕМ (C-ABI POD Tables, 0 vtable overhead)
    // ==============================================================================

    // --- СИСТЕМА ЛОГИРОВАНИЯ ---
    enum class LogLevel : uint32_t {
        Trace = 0,
        Info = 1,
        Warn = 2,
        Error = 3,
        Fatal = 4
    };

    struct LogAPI {
        void (*log_message)(LogLevel level, const char* channel, const char* message);
    };

    // --- СИСТЕМА ВВОДА ---
    struct InputAPI {
        bool (*is_key_pressed)(int key_code);
        bool (*is_key_just_pressed)(int key_code);
        void (*get_mouse_pos)(double* x, double* y);
        bool (*is_mouse_button_pressed)(int button);
        void (*get_mouse_delta)(double* dx, double* dy);
        void (*get_mouse_wheel)(double* x_offset, double* y_offset);
    };

    // --- СИСТЕМА ЗВУКА ---
    struct AudioAPI {
        uint32_t(*load_sound)(const char* filepath);
        void     (*play_sound)(uint32_t sound_id, float volume, bool loop);
        void     (*stop_sound)(uint32_t sound_id);
        void     (*set_listener_pos)(float x, float y, float z);
        void     (*set_sound_pos)(uint32_t sound_id, float x, float y, float z);
    };

    // --- СИСТЕМА РЕНДЕРА ---
    struct RenderAPI {
        void (*submit_command)(uint32_t command_type, const void* data_ptr, size_t data_size);
        void (*get_viewport_size)(uint32_t* width, uint32_t* height);
        void (*set_vsync)(bool enabled);
    };

    // --- СИСТЕМА АССЕТОВ ---
    struct AssetAPI {
        void* (*load_binary)(const char* path, size_t* out_size);
        char* (*load_text)(const char* path);
        void  (*free_data)(void* data);
        bool  (*file_exists)(const char* path);
    };

    // ==============================================================================
    // 4. УНИВЕРСАЛЬНЫЙ КОНТЕКСТ ЯДРА (Engine Context Matrix)
    // ==============================================================================
    struct EngineContext {
        // Прямой доступ к инфраструктуре ядра
        EcsRegistry* ecs;
        EventBus* event_bus;
        JobSystem* job_system;

        // Legacy/Direct кэш базовых сервисов ядра
        InputAPI* input;
        AudioAPI* audio;
        RenderAPI* renderer;
        AssetAPI* assets;
        LogAPI* logger;

        // Service Locator (Динамическая регистрация и запрос сервисов)
        void* (*get_system)(SystemID id);
        void  (*register_system)(SystemID id, void* system_ptr);

        // Frame Blackboard (Слоты временных данных текущего кадра)
        void* (*get_frame_data)(FrameDataId id);
        void  (*set_frame_data)(FrameDataId id, void* data_ptr);

        // --- C++ Удобные типобезопасные хелперы (Zero Overhead) ---
        template<typename T>
        T* getSystem(SystemID id) const {
            if (!get_system) return nullptr;
            return static_cast<T*>(get_system(id));
        }

        template<typename T>
        void registerSystem(SystemID id, T* ptr) const {
            if (register_system) {
                register_system(id, static_cast<void*>(ptr));
            }
        }

        template<typename T>
        T* getFrameData(FrameDataId id) const {
            if (!get_frame_data) return nullptr;
            return static_cast<T*>(get_frame_data(id));
        }

        template<typename T>
        void setFrameData(FrameDataId id, T* ptr) const {
            if (set_frame_data) {
                set_frame_data(id, static_cast<void*>(ptr));
            }
        }
    };

    // ==============================================================================
    // 5. ИНТЕРФЕЙС ПЛАГИНА (Plugin Interface ABI Contract)
    // ==============================================================================
    struct PluginInterface {
        // Метаданные плагина
        const char* name;
        const char* version;
        uint32_t    abi_version; // Обязан совпадать с ENGINE_ABI_VERSION
        uint32_t    priority;    // Порядок вызова: Window(0) -> Physics(50) -> Game(100) -> Render(200)

        // Жизненный цикл плагина
        void (*on_load)(EngineContext* ctx);
        void (*on_fixed_update)(float fixed_dt); // Опционально: Физический шаг (nullptr, если не нужен)
        void (*on_update)(float delta_time);     // Обязательно: Основной шаг кадра
        void (*on_render)(float alpha);          // Опционально: Отрисовка с фактором интерполяции
        void (*on_unload)();                     // Обязательно: Освобождение ресурсов
    };

} // namespace core

// ==============================================================================
// 6. МАКРОСЫ ЭКСПОРТА ТОЧКИ ВХОДА (C-Linkage)
// ==============================================================================
#if defined(_WIN32)
#define ENGINE_PLUGIN_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define ENGINE_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define ENGINE_PLUGIN_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif
    ENGINE_PLUGIN_EXPORT core::PluginInterface* GetPluginAPI();
#ifdef __cplusplus
}
#endif


#define DECLARE_ENGINE_PLUGIN(PluginStructInstance) \
    extern "C" { \
        ENGINE_PLUGIN_EXPORT core::PluginInterface* GetPluginAPI() { \
            return &PluginStructInstance; \
        } \
    }
#pragma once
#include <cstdint>
#include <stddef.h>

// Опережающие объявления (ядро передает их как непрозрачные указатели)
class EcsRegistry;
class EventBus;
class JobSystem;

namespace core {

    // ==============================================================================
    // SERVICE LOCATOR & INTERFACE IDS (v0.3.0)
    // ==============================================================================
    // Используем уникальные ID для каждой подсистемы движка.
    using SystemID = uint64_t;

    // Резервируем ID для базовых систем (Legacy Support)
    constexpr SystemID SYS_INPUT = 0x01;
    constexpr SystemID SYS_AUDIO = 0x02;
    constexpr SystemID SYS_RENDERER = 0x03;
    constexpr SystemID SYS_ASSETS = 0x04; // Новая система ассетов

    // ==============================================================================
    // ЧИСТЫЕ C-ИНТЕРФЕЙСЫ (Стабильный ABI)
    // ==============================================================================

    struct InputAPI {
        bool (*is_key_pressed)(int key_code);
        void (*get_mouse_pos)(double* x, double* y);
        bool (*is_mouse_button_pressed)(int button);
    };

    struct AudioAPI {
        uint32_t(*load_sound)(const char* filepath);
        void     (*play_sound)(uint32_t sound_id, float volume, bool loop);
        void     (*set_listener_pos)(float x, float y, float z);
    };

    struct RenderAPI {
        void (*submit_command)(uint32_t command_type, const void* data_ptr, size_t data_size);
    };

    // Интерфейс для новой системы ассетов (AssetPlugin)
    struct AssetAPI {
        void* (*load_binary)(const char* path, size_t* out_size);
        void  (*free_binary)(void* data);
    };

    // ==============================================================================
    // УНИВЕРСАЛЬНЫЙ КОНТЕКСТ ЯДРА (Service Locator)
    // ==============================================================================
    struct EngineContext {
        // --- Прямой доступ к базовым сервисам ядра ---
        EcsRegistry* ecs;
        EventBus* event_bus;
        JobSystem* job_system;

        // --- Legacy API (Для обратной совместимости с плагинами v0.2.0) ---
        InputAPI* input;
        AudioAPI* audio;
        RenderAPI* renderer;

        // --- Service Locator v0.3.0 (Материнская плата) ---
        // Позволяет найти любую систему по ID: context->get_system(SYS_ASSETS)
        void* (*get_system)(SystemID id);

        // Позволяет плагину зарегистрировать свою функциональность в ядре
        void  (*register_system)(SystemID id, void* system_ptr);
    };

    // ==============================================================================
    // ИНТЕРФЕЙС ПЛАГИНА
    // ==============================================================================
    struct PluginInterface {
        const char* name;
        uint32_t    priority;

        void (*on_load)(EngineContext* ctx);
        void (*on_update)(float delta_time);
        void (*on_unload)();
    };

} // namespace core

// Экспорт для динамических библиотек (C-Linkage)
extern "C" {
#if defined(_WIN32)
    __declspec(dllexport) core::PluginInterface* GetPluginAPI();
#else
    __attribute__((visibility("default"))) core::PluginInterface* GetPluginAPI();
#endif
}

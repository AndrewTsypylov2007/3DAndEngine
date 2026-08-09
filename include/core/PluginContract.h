#pragma once
#include <cstdint>
<<<<<<< HEAD
#include <stddef.h>
=======
>>>>>>> 28d53b185e478edcb19a5b99a606b2a5a10e3a5a

// Опережающие объявления (ядро передает их как непрозрачные указатели)
class EcsRegistry;
class EventBus;
class JobSystem;

namespace core {

    // ==============================================================================
<<<<<<< HEAD
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

=======
    // ЧИСТЫЕ C-ИНТЕРФЕЙСЫ (Без vtable-зависимостей компиляторов)
    // ==============================================================================

    // Вместо виртуальных классов используем структуры с плоскими указателями на функции.
    // Это гарантирует, что MSVC, GCC и Clang поймут их абсолютно одинаково.

>>>>>>> 28d53b185e478edcb19a5b99a606b2a5a10e3a5a
    struct InputAPI {
        bool (*is_key_pressed)(int key_code);
        void (*get_mouse_pos)(double* x, double* y);
        bool (*is_mouse_button_pressed)(int button);
    };

    struct AudioAPI {
<<<<<<< HEAD
        uint32_t(*load_sound)(const char* filepath);
=======
        uint32_t (*load_sound)(const char* filepath);
>>>>>>> 28d53b185e478edcb19a5b99a606b2a5a10e3a5a
        void     (*play_sound)(uint32_t sound_id, float volume, bool loop);
        void     (*set_listener_pos)(float x, float y, float z);
    };

    struct RenderAPI {
<<<<<<< HEAD
        void (*submit_command)(uint32_t command_type, const void* data_ptr, size_t data_size);
    };

    // Интерфейс для новой системы ассетов (AssetPlugin)
    struct AssetAPI {
        void* (*load_binary)(const char* path, size_t* out_size);
        void  (*free_binary)(void* data);
    };

    // ==============================================================================
    // УНИВЕРСАЛЬНЫЙ КОНТЕКСТ ЯДРА (Service Locator)
=======
        // Передача команд. Для безопасности void* data_ptr заменен на явный размер данных data_size
        void (*submit_command)(uint32_t command_type, const void* data_ptr, size_t data_size);
    };

    // ==============================================================================
    // ИСПРАВЛЕННЫЙ ЕДИНЫЙ КОНТЕКСТ ЯДРА
>>>>>>> 28d53b185e478edcb19a5b99a606b2a5a10e3a5a
    // ==============================================================================
    struct EngineContext {
        // --- Прямой доступ к базовым сервисам ядра ---
        EcsRegistry* ecs;
        EventBus*    event_bus;
        JobSystem*   job_system;

<<<<<<< HEAD
        // --- Legacy API (Для обратной совместимости с плагинами v0.2.0) ---
        InputAPI* input;
        AudioAPI* audio;
        RenderAPI* renderer;

        // --- Service Locator v0.3.0 (Материнская плата) ---
        // Позволяет найти любую систему по ID: context->get_system(SYS_ASSETS)
        void* (*get_system)(SystemID id);

        // Позволяет плагину зарегистрировать свою функциональность в ядре
        void  (*register_system)(SystemID id, void* system_ptr);
=======
        // Таблицы функций для подсистем (если плагин не загружен — указатели внутри будут nullptr)
        InputAPI*    input;
        AudioAPI*    audio;
        RenderAPI*   renderer; 
>>>>>>> 28d53b185e478edcb19a5b99a606b2a5a10e3a5a
    };

    // ==============================================================================
    // ИНТЕРФЕЙС ПЛАГИНА
    // ==============================================================================
    struct PluginInterface {
        const char* name;
        uint32_t    priority;

<<<<<<< HEAD
=======
        // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Передача контекста строго по указателю (EngineContext*)
>>>>>>> 28d53b185e478edcb19a5b99a606b2a5a10e3a5a
        void (*on_load)(EngineContext* ctx);
        void (*on_update)(float delta_time);
        void (*on_unload)();
    };

} // namespace core

<<<<<<< HEAD
// Экспорт для динамических библиотек (C-Linkage)
=======
// Экспорт для динамических библиотек
>>>>>>> 28d53b185e478edcb19a5b99a606b2a5a10e3a5a
extern "C" {
#if defined(_WIN32)
    // Для Windows (MSVC / MinGW)
    __declspec(dllexport) core::PluginInterface* GetPluginAPI();
#else
<<<<<<< HEAD
=======
    // Для macOS (Clang) и Linux (GCC)
    // Используем стандартный атрибут видимости для Unix систем
>>>>>>> 28d53b185e478edcb19a5b99a606b2a5a10e3a5a
    __attribute__((visibility("default"))) core::PluginInterface* GetPluginAPI();
#endif
}

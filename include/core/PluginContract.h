#pragma once
#include <cstdint>

// Опережающие объявления (ядро передает их как непрозрачные указатели)
class EcsRegistry;
class EventBus;
class JobSystem;

namespace core {

    // ==============================================================================
    // ЧИСТЫЕ C-ИНТЕРФЕЙСЫ (Без vtable-зависимостей компиляторов)
    // ==============================================================================

    // Вместо виртуальных классов используем структуры с плоскими указателями на функции.
    // Это гарантирует, что MSVC, GCC и Clang поймут их абсолютно одинаково.

    struct InputAPI {
        bool (*is_key_pressed)(int key_code);
        void (*get_mouse_pos)(double* x, double* y);
        bool (*is_mouse_button_pressed)(int button);
    };

    struct AudioAPI {
        uint32_t (*load_sound)(const char* filepath);
        void     (*play_sound)(uint32_t sound_id, float volume, bool loop);
        void     (*set_listener_pos)(float x, float y, float z);
    };

    struct RenderAPI {
        // Передача команд. Для безопасности void* data_ptr заменен на явный размер данных data_size
        void (*submit_command)(uint32_t command_type, const void* data_ptr, size_t data_size);
    };

    // ==============================================================================
    // ИСПРАВЛЕННЫЙ ЕДИНЫЙ КОНТЕКСТ ЯДРА
    // ==============================================================================
    struct EngineContext {
        EcsRegistry* ecs;
        EventBus*    event_bus;
        JobSystem*   job_system;

        // Таблицы функций для подсистем (если плагин не загружен — указатели внутри будут nullptr)
        InputAPI*    input;
        AudioAPI*    audio;
        RenderAPI*   renderer; 
    };

    // ==============================================================================
    // ИНТЕРФЕЙС ПЛАГИНА
    // ==============================================================================
    struct PluginInterface {
        const char* name;
        uint32_t    priority;

        // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Передача контекста строго по указателю (EngineContext*)
        void (*on_load)(EngineContext* ctx);
        void (*on_update)(float delta_time);
        void (*on_unload)();
    };

} // namespace core

// Экспорт для динамических библиотек
extern "C" {
#if defined(_WIN32)
    __declspec(dllexport) core::PluginInterface* GetPluginAPI();
#else
    __declspec(dllexport) core::PluginInterface* GetPluginAPI(); // Для Linux/macOS
#endif
}

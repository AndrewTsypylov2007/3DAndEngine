#pragma once
#include "EcsRegistry.h"
#include "EventBus.h"
#include "JobSystem.h"
#include <cstdint>
#include "RenderCommand.h"

namespace core {

    // ==============================================================================
    // СЕРВИСНЫЕ ИНТЕРФЕЙСЫ НА БУДУЩЕЕ (Слепые абстрактные субстраты)
    // ==============================================================================

    // Субстрат ввода: мышь, клавиатура, геймпады
    class IInputSubsystem {
    public:
        virtual ~IInputSubsystem() = default;
        virtual bool isKeyPressed(int key_code) const = 0;
        virtual void getMousePos(double& x, double& y) const = 0;
        virtual bool isMouseButtonPressed(int button) const = 0;
    };

    // Субстрат аудио: проигрывание звуков, эффектов и 3D-аудиоисточников
    class LAudioSubsystem {
    public:
        virtual ~LAudioSubsystem() = default;
        virtual uint32_t loadSound(const char* filepath) = 0;
        virtual void playSound(uint32_t sound_id, float volume = 1.0f, bool loop = false) = 0;
        virtual void setListenerPosition(float x, float y, float z) = 0;
    };

    // Мост рендеринга (Тот самый "Кент"): Сюда плагины шлют draw-команды кадра
    class IRenderBridge {
    public:
        virtual ~IRenderBridge() = default;
        // Передача сырых графических данных (ImGui, буферы мешей, костные матрицы) вслепую
        virtual void submitRenderCommand(uint32_t command_type, void* data_ptr) = 0;
    };

    // ==============================================================================
    // ОБНОВЛЕННЫЙ ЕДИНЫЙ КОНТЕКСТ ЯДРА ДВИЖКА
    // ==============================================================================
    struct EngineContext {
        EcsRegistry* ecs;
        EventBus* event_bus;
        JobSystem* job_system;

        // AAA-Расширение: Сервисы-посредники. Если плагин не загружен — указатель равен nullptr
        IInputSubsystem* input;      // Заполнит WindowPlugin / InputPlugin
        LAudioSubsystem* audio;      // Заполнит AudioPlugin (OpenAL / FMOD)
        IRenderBridge* renderer;   // Заполнит RenderVulkanPlugin ("Кент")
    };

    struct PluginInterface {
        const char* name;
        uint32_t    priority;

        void (*on_load)(EngineContext ctx);
        void (*on_update)(float delta_time);
        void (*on_unload)();
    };

} // namespace core

extern "C" {
#if defined(_WIN32)
    __declspec(dllexport) core::PluginInterface* GetPluginAPI();
#else
    core::PluginInterface* GetPluginAPI();
#endif
}

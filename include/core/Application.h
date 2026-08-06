#pragma once
#include "EcsRegistry.h"
#include "EventBus.h"
#include "JobSystem.h"
#include "PluginContract.h"
#include <vector>
#include <algorithm>
#include <chrono>
#include <atomic> 

namespace core {

    // ==============================================================================
    // AAA NULL OBJECTS: Безопасные "глухие" заглушки для работы без плагинов
    // ==============================================================================
    class NullInputSubsystem : public IInputSubsystem {
    public:
        bool isKeyPressed(int) const override { return false; }
        void getMousePos(double& x, double& y) const override { x = 0.0; y = 0.0; }
        bool isMouseButtonPressed(int) const override { return false; }
    };

    class NullAudioSubsystem : public LAudioSubsystem {
    public:
        uint32_t loadSound(const char*) override { return 0; }
        void playSound(uint32_t, float, bool) override {}
        void setListenerPosition(float, float, float) override {}
    };

    class NullRenderBridge : public IRenderBridge {
    public:
        void submitRenderCommand(uint32_t, void*) override {}
    };

    class Application {
    private:
        EcsRegistry     ecs_;
        EventBus        event_bus_;
        JobSystem       job_system_;

        std::atomic<bool> is_running_{ false };
        std::vector<PluginInterface*> plugins_;

        // Создаем экземпляры заглушек в памяти ядра
        NullInputSubsystem null_input_;
        NullAudioSubsystem null_audio_;
        NullRenderBridge   null_renderer_;

        // Инициализируем указатели сервисов ссылками на заглушки
        IInputSubsystem* input_service_ = &null_input_;
        LAudioSubsystem* audio_service_ = &null_audio_;
        IRenderBridge* render_service_ = &null_renderer_;

    public:
        void registerPlugin(PluginInterface* plugin) {
            if (!plugin) return;
            plugins_.push_back(plugin);

            std::sort(plugins_.begin(), plugins_.end(), [](PluginInterface* a, PluginInterface* b) {
                return a->priority < b->priority;
                });

            // ИСПРАВЛЕНО: Явная и полная инициализация всех 6 полей структуры EngineContext.
            // Больше никаких missing-field-initializers на серверах GCC/Linux!
            EngineContext ctx{
                &ecs_,
                &event_bus_,
                &job_system_,
                input_service_,
                audio_service_,
                render_service_
            };

            plugin->on_load(ctx);

            // Если плагин прописал реальный сервис, ядро заменяет заглушку боевой системой
            if (ctx.input && ctx.input != &null_input_)       input_service_ = ctx.input;
            if (ctx.audio && ctx.audio != &null_audio_)       audio_service_ = ctx.audio;
            if (ctx.renderer && ctx.renderer != &null_renderer_) render_service_ = ctx.renderer;
        }

        void run() {
            job_system_.initialize();
            is_running_.store(true, std::memory_order_release);

            auto last_time = std::chrono::high_resolution_clock::now();

            while (is_running_.load(std::memory_order_acquire)) {
                auto current_time = std::chrono::high_resolution_clock::now();
                float dt = std::chrono::duration<float>(current_time - last_time).count();
                last_time = current_time;

                // ИСПРАВЛЕНО: Полная инициализация контекста внутри каждого такта игрового цикла
                EngineContext ctx{
                    &ecs_,
                    &event_bus_,
                    &job_system_,
                    input_service_,
                    audio_service_,
                    render_service_
                };

                for (auto* plugin : plugins_) {
                    if (plugin->on_update) {
                        plugin->on_update(dt);
                    }
                }

                event_bus_.clear();

                if (plugins_.empty()) {
                    is_running_.store(false, std::memory_order_release);
                }
            }

            for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
                if ((*it)->on_unload) {
                    (*it)->on_unload();
                }
            }
            job_system_.shutdown();
        }

        void stop() {
            is_running_.store(false, std::memory_order_release);
        }
    };

} // namespace core

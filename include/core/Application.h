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
    // AAA NULL OBJECTS: Теперь они написаны в новом C-Style ABI
    // ==============================================================================
    
    // Заглушки теперь — это просто статические функции, а не виртуальные классы
    namespace null_subsystems {
        inline bool isKeyPressed(int) { return false; }
        inline void getMousePos(double* x, double* y) { if(x) *x = 0.0; if(y) *y = 0.0; }
        inline bool isMouseButtonPressed(int) { return false; }

        inline uint32_t loadSound(const char*) { return 0; }
        inline void playSound(uint32_t, float, bool) {}
        inline void setListenerPosition(float, float, float) {}

        inline void submitRenderCommand(uint32_t, const void*, size_t) {}
    }

    class Application {
    private:
        EcsRegistry     ecs_;
        EventBus        event_bus_;
        JobSystem       job_system_;

        std::atomic<bool> is_running_{ false };
        std::vector<PluginInterface*> plugins_;

        // Таблицы функций для заглушек (живут в памяти ядра)
        InputAPI null_input_api_{ null_subsystems::isKeyPressed, null_subsystems::getMousePos, null_subsystems::isMouseButtonPressed };
        AudioAPI null_audio_api_{ null_subsystems::loadSound, null_subsystems::playSound, null_subsystems::setListenerPosition };
        RenderAPI null_render_api_{ null_subsystems::submitRenderCommand };

        // Рабочие указатели на активные интерфейсы (по умолчанию указывают на заглушки)
        InputAPI*  input_service_  = &null_input_api_;
        AudioAPI*  audio_service_  = &null_audio_api_;
        RenderAPI* render_service_ = &null_render_api_;

    public:
        void registerPlugin(PluginInterface* plugin) {
            if (!plugin) return;
            plugins_.push_back(plugin);

            std::sort(plugins_.begin(), plugins_.end(), [](PluginInterface* a, PluginInterface* b) {
                return a->priority < b->priority;
            });

            // Создаем контекст ядра
            EngineContext ctx{
                &ecs_,
                &event_bus_,
                &job_system_,
                input_service_,
                audio_service_,
                render_service_
            };

            // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Передаем адрес контекста (&ctx). 
            // Теперь плагин пишет напрямую в эту структуру!
            plugin->on_load(&ctx);

            // Если плагин подменил указатели в контексте — обновляем сервисы ядра
            if (ctx.input && ctx.input != &null_input_api_)       input_service_  = ctx.input;
            if (ctx.audio && ctx.audio != &null_audio_api_)       audio_service_  = ctx.audio;
            if (ctx.renderer && ctx.renderer != &null_render_api_) render_service_ = ctx.renderer;
        }

        void run() {
            job_system_.initialize();
            is_running_.store(true, std::memory_order_release);

            auto last_time = std::chrono::high_resolution_clock::now();

            while (is_running_.load(std::memory_order_acquire)) {
                auto current_time = std::chrono::high_resolution_clock::now();
                float dt = std::chrono::duration<float>(current_time - last_time).count();
                last_time = current_time;

                // Пересобираем актуальный контекст на этот такт кадра
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

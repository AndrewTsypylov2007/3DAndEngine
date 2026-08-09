#pragma once
#include "EcsRegistry.h"
#include "EventBus.h"
#include "JobSystem.h"
#include "PluginContract.h"
#include <unordered_map>
#include <algorithm>
#include <iostream>

namespace core {

<<<<<<< HEAD
    // Глобальный реестр систем
    static std::unordered_map<SystemID, void*> g_SystemRegistry;

    namespace system_bridge {
        inline void* GetSystem(SystemID id) {
            auto it = g_SystemRegistry.find(id);
            return (it != g_SystemRegistry.end()) ? it->second : nullptr;
        }
        inline void RegisterSystem(SystemID id, void* ptr) {
            g_SystemRegistry[id] = ptr;
        }
=======
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
>>>>>>> 28d53b185e478edcb19a5b99a606b2a5a10e3a5a
    }

    class Application {
    private:
        EcsRegistry     ecs_;
        EventBus        event_bus_;
        JobSystem       job_system_;
        std::atomic<bool> is_running_ = false; // Явная инициализация
        std::vector<PluginInterface*> plugins_;

<<<<<<< HEAD
        // Явная инициализация функциональных указателей заглушек
        InputAPI  null_in_ = { [](int) {return false;}, [](double* x, double* y) {if (x)*x = 0;if (y)*y = 0;}, [](int) {return false;} };
        RenderAPI null_rd_ = { [](uint32_t, const void*, size_t) {} };
        AudioAPI  null_au_ = { [](const char*) {return 0u;}, [](uint32_t, float, bool) {}, [](float, float, float) {} };
=======
        // Таблицы функций для заглушек (живут в памяти ядра)
        InputAPI null_input_api_{ null_subsystems::isKeyPressed, null_subsystems::getMousePos, null_subsystems::isMouseButtonPressed };
        AudioAPI null_audio_api_{ null_subsystems::loadSound, null_subsystems::playSound, null_subsystems::setListenerPosition };
        RenderAPI null_render_api_{ null_subsystems::submitRenderCommand };

        // Рабочие указатели на активные интерфейсы (по умолчанию указывают на заглушки)
        InputAPI*  input_service_  = &null_input_api_;
        AudioAPI*  audio_service_  = &null_audio_api_;
        RenderAPI* render_service_ = &null_render_api_;
>>>>>>> 28d53b185e478edcb19a5b99a606b2a5a10e3a5a

    public:
        Application() {
            system_bridge::RegisterSystem(SYS_INPUT, &null_in_);
            system_bridge::RegisterSystem(SYS_RENDERER, &null_rd_);
            system_bridge::RegisterSystem(SYS_AUDIO, &null_au_);
        }

        void registerPlugin(PluginInterface* plugin) {
            if (!plugin) return;
            plugins_.push_back(plugin);

            // Сортировка при регистрации
            std::sort(plugins_.begin(), plugins_.end(), [](PluginInterface* a, PluginInterface* b) {
                return a->priority < b->priority;
            });

<<<<<<< HEAD
            EngineContext ctx = createCtx();
            plugin->on_load(&ctx);
        }

        void run() {
            is_running_.store(true);
            // ... логика цикла ...
=======
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
>>>>>>> 28d53b185e478edcb19a5b99a606b2a5a10e3a5a
        }

        void stop() { is_running_.store(false); }

    private:
        EngineContext createCtx() {
            EngineContext ctx;
            ctx.ecs = &ecs_;
            ctx.event_bus = &event_bus_;
            ctx.job_system = &job_system_;
            ctx.get_system = system_bridge::GetSystem;
            ctx.register_system = system_bridge::RegisterSystem;

            // Синхронизируем Legacy указатели
            ctx.input = (InputAPI*)system_bridge::GetSystem(SYS_INPUT);
            ctx.renderer = (RenderAPI*)system_bridge::GetSystem(SYS_RENDERER);
            ctx.audio = (AudioAPI*)system_bridge::GetSystem(SYS_AUDIO);

            return ctx;
        }
    };
}

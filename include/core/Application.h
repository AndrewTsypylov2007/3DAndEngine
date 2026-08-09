#pragma once
#include "EcsRegistry.h"
#include "EventBus.h"
#include "JobSystem.h"
#include "PluginContract.h"
#include <unordered_map>
#include <algorithm>
#include <iostream>

namespace core {

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
    }

    class Application {
    private:
        EcsRegistry     ecs_;
        EventBus        event_bus_;
        JobSystem       job_system_;
        std::atomic<bool> is_running_ = false; // Явная инициализация
        std::vector<PluginInterface*> plugins_;

        // Явная инициализация функциональных указателей заглушек
        InputAPI  null_in_ = { [](int) {return false;}, [](double* x, double* y) {if (x)*x = 0;if (y)*y = 0;}, [](int) {return false;} };
        RenderAPI null_rd_ = { [](uint32_t, const void*, size_t) {} };
        AudioAPI  null_au_ = { [](const char*) {return 0u;}, [](uint32_t, float, bool) {}, [](float, float, float) {} };

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

            EngineContext ctx = createCtx();
            plugin->on_load(&ctx);
        }

        void run() {
            is_running_.store(true);
            // ... логика цикла ...
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

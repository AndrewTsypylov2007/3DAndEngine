#pragma once

// 1. СИСТЕМНЫЕ ИНКЛУДЫ
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <iostream>

// 2. ИНКЛУДЫ ПОДСИСТЕМ ДВИЖКА
#include "EcsRegistry.h"
#include "EventBus.h"
#include "JobSystem.h"
#include "PluginContract.h"

namespace core {

    // Глобальная таблица коммутации систем движка
    inline std::unordered_map<SystemID, void*> g_SystemRegistry;

    // Слой моста для плоских C-указателей функций
    class SystemBridge {
    public:
        static void* GetSystem(SystemID id) {
            auto it = g_SystemRegistry.find(id);
            return (it != g_SystemRegistry.end()) ? it->second : nullptr;
        }

        static void RegisterSystem(SystemID id, void* ptr) {
            g_SystemRegistry[id] = ptr;
            std::cout << "[Core v0.3.0] Служба подключена к шине: 0x" << std::hex << id << std::dec << std::endl;
        }
    };

    // ==============================================================================
    // MAIN APPLICATION MACHINE
    // ==============================================================================
    class Application {
    private:
        EcsRegistry                   ecs_;
        EventBus                      event_bus_;
        JobSystem                     job_system_;
        std::atomic<bool>             is_running_{ false };
        std::vector<PluginInterface*> plugins_;

        // Абсолютно чистые лямбда-заглушки с явным указанием возвращаемых типов (MSVC Strict)
        InputAPI  null_input_api_{ [](int) -> bool { return false; }, [](double* x, double* y) { if (x)*x = 0.0; if (y)*y = 0.0; }, [](int) -> bool { return false; } };
        AudioAPI  null_audio_api_{ [](const char*) -> uint32_t { return 0u; }, [](uint32_t, float, bool) {}, [](float, float, float) {} };
        RenderAPI null_render_api_{ [](uint32_t, const void*, size_t) {} };

    public:
        Application() {
            SystemBridge::RegisterSystem(SYS_INPUT, &null_input_api_);
            SystemBridge::RegisterSystem(SYS_AUDIO, &null_audio_api_);
            SystemBridge::RegisterSystem(SYS_RENDERER, &null_render_api_);
        }

        ~Application() {
            stop();
        }

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void registerPlugin(PluginInterface* plugin) {
            if (!plugin) return;

            plugins_.push_back(plugin);

            std::sort(plugins_.begin(), plugins_.end(), [](const PluginInterface* a, const PluginInterface* b) {
                return a->priority < b->priority;
                });

            EngineContext ctx = createCtx();
            plugin->on_load(&ctx);
        }

        void run() {
            job_system_.initialize();
            is_running_.store(true, std::memory_order_release);

            auto last_time = std::chrono::high_resolution_clock::now();
            std::cout << "[Core] Игровой цикл движка v0.3.0 запущен.\n";

            while (is_running_.load(std::memory_order_acquire)) {
                auto current_time = std::chrono::high_resolution_clock::now();
                float dt = std::chrono::duration<float>(current_time - last_time).count();
                last_time = current_time;

                if (dt > 0.1f) dt = 0.1f;

                EngineContext ctx = createCtx();

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

    private:
        EngineContext createCtx() {
            EngineContext ctx;
            ctx.ecs = &ecs_;
            ctx.event_bus = &event_bus_;
            ctx.job_system = &job_system_;

            // Прямое приведение статических методов к C-указателям функций
            ctx.get_system = &SystemBridge::GetSystem;
            ctx.register_system = &SystemBridge::RegisterSystem;

            ctx.input = static_cast<InputAPI*>(SystemBridge::GetSystem(SYS_INPUT));
            ctx.audio = static_cast<AudioAPI*>(SystemBridge::GetSystem(SYS_AUDIO));
            ctx.renderer = static_cast<RenderAPI*>(SystemBridge::GetSystem(SYS_RENDERER));

            return ctx;
        }
    };

} // namespace core

#pragma once

#include <unordered_map>
#include <vector>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <iostream>
#include <shared_mutex>
#include <mutex>
#include <memory>
#include <cstdint>
#include <iomanip>

#include "Types.h"
#include "EcsRegistry.h"
#include "EventBus.h"
#include "JobSystem.h"
#include "PluginContract.h"

namespace core {

    // ==============================================================================
    // THREAD-SAFE SERVICE REGISTRY (AAA Interface Matrix)
    // ==============================================================================
    class ServiceRegistry {
    private:
        static constexpr size_t FAST_SLOT_COUNT = 16;
        void* fast_slots_[FAST_SLOT_COUNT] = { nullptr };

        std::unordered_map<SystemID, void*> dynamic_services_;
        mutable std::shared_mutex           mutex_;

    public:
        ServiceRegistry() = default;
        ~ServiceRegistry() = default;

        ServiceRegistry(const ServiceRegistry&) = delete;
        ServiceRegistry& operator=(const ServiceRegistry&) = delete;

        void register_system(SystemID id, void* ptr) {
            std::unique_lock lock(mutex_);
            if (id > 0 && id < FAST_SLOT_COUNT) {
                fast_slots_[id] = ptr;
            }
            else {
                dynamic_services_[id] = ptr;
            }
        }

        void* get_system(SystemID id) const {
            if (id > 0 && id < FAST_SLOT_COUNT) {
                return fast_slots_[id];
            }

            std::shared_lock lock(mutex_);
            auto it = dynamic_services_.find(id);
            return (it != dynamic_services_.end()) ? it->second : nullptr;
        }

        void clear() {
            std::unique_lock lock(mutex_);
            for (size_t i = 0; i < FAST_SLOT_COUNT; ++i) {
                fast_slots_[i] = nullptr;
            }
            dynamic_services_.clear();
        }
    };

    struct EngineConfig {
        double   fixed_timestep = 1.0 / 60.0; // 60 Гц фиксированная физика
        float    max_delta_time = 0.1f;        // 100 мс максимум (защита от лагов)
        uint32_t worker_threads = 0;           // 0 = автоопределение ядер CPU
    };

    struct FrameStats {
        uint64_t total_frames{ 0 };
        float    fps{ 0.0f };
        float    frame_time_ms{ 0.0f };
    };

    // ==============================================================================
    // MAIN APPLICATION MACHINE (v0.4.0 AAA Blind Engine Driver)
    // ==============================================================================
    class Application {
    private:
        EcsRegistry                   ecs_;
        EventBus                      event_bus_;
        JobSystem                     job_system_;
        ServiceRegistry               services_;

        std::atomic<bool>             is_running_{ false };
        EngineConfig                  config_;
        FrameStats                    stats_;

        std::vector<PluginInterface*> plugins_;

        // --- C-ABI NULL-ЗАГЛУШКИ ПО УМОЛЧАНИЮ ---
        InputAPI null_input_api_{
            [](int) -> bool { return false; },
            [](int) -> bool { return false; },
            [](double* x, double* y) { if (x)*x = 0.0; if (y)*y = 0.0; },
            [](int) -> bool { return false; },
            [](double* dx, double* dy) { if (dx)*dx = 0.0; if (dy)*dy = 0.0; },
            [](double* x, double* y) { if (x)*x = 0.0; if (y)*y = 0.0; }
        };

        AudioAPI null_audio_api_{
            [](const char*) -> uint32_t { return 0u; },
            [](uint32_t, float, bool) {},
            [](uint32_t) {},
            [](float, float, float) {},
            [](uint32_t, float, float, float) {}
        };

        RenderAPI null_render_api_{
            [](uint32_t, const void*, size_t) {},
            [](uint32_t* w, uint32_t* h) { if (w)*w = 1280; if (h)*h = 720; },
            [](bool) {}
        };

        AssetAPI null_asset_api_{
            [](const char*, size_t* out_size) -> void* { if (out_size) *out_size = 0; return nullptr; },
            [](const char*) -> char* { return nullptr; },
            [](void*) {},
            [](const char*) -> bool { return false; }
        };

        LogAPI null_log_api_{
            [](LogLevel, const char* channel, const char* msg) {
                std::cout << "[" << (channel ? channel : "Core") << "] "
                          << (msg ? msg : "") << std::endl;
            }
        };

        static void* BridgeGetSystem(SystemID id);
        static void  BridgeRegisterSystem(SystemID id, void* ptr);
        inline static Application* s_active_instance = nullptr;

    public:
        explicit Application(const EngineConfig& config = EngineConfig())
            : config_(config) {
            s_active_instance = this;

            // Регистрация базовых заглушек через токены sys_id
            services_.register_system(sys_id::Input, &null_input_api_);
            services_.register_system(sys_id::Audio, &null_audio_api_);
            services_.register_system(sys_id::Renderer, &null_render_api_);
            services_.register_system(sys_id::Assets, &null_asset_api_);
            services_.register_system(sys_id::Log, &null_log_api_);

            // Подписка на системные события выхода
            event_bus_.subscribe("engine/exit"_id, [this](uint64_t) { stop(); });
            event_bus_.subscribe("app/quit"_id, [this](uint64_t) { stop(); });
        }

        ~Application() {
            stop();
            if (s_active_instance == this) {
                s_active_instance = nullptr;
            }
        }

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void registerPlugin(PluginInterface* plugin) {
            if (!plugin) return;

            plugins_.push_back(plugin);

            std::sort(plugins_.begin(), plugins_.end(),
                [](const PluginInterface* a, const PluginInterface* b) {
                    return a->priority < b->priority;
                }
            );

            EngineContext ctx = createEngineContext();
            if (plugin->on_load) {
                plugin->on_load(&ctx);
            }
        }

        void run() {
            job_system_.initialize(config_.worker_threads);
            is_running_.store(true, std::memory_order_release);

            using Clock = std::chrono::high_resolution_clock;
            auto last_time = Clock::now();
            double accumulator = 0.0;

            auto fps_timer = Clock::now();
            uint32_t frames_in_second = 0;

            while (is_running_.load(std::memory_order_acquire)) {
                auto current_time = Clock::now();
                float frame_dt = std::chrono::duration<float>(current_time - last_time).count();
                last_time = current_time;

                if (frame_dt > config_.max_delta_time) {
                    frame_dt = config_.max_delta_time;
                }

                accumulator += frame_dt;

                // 1. ФИКСИРОВАННЫЙ ШАГ ФИЗИКИ (Fixed Timestep 60 Гц)
                while (accumulator >= config_.fixed_timestep) {
                    float fixed_dt = static_cast<float>(config_.fixed_timestep);
                    for (auto* plugin : plugins_) {
                        if (plugin && plugin->on_fixed_update) {
                            plugin->on_fixed_update(fixed_dt);
                        }
                    }
                    accumulator -= config_.fixed_timestep;
                }

                // 2. ГЕЙМПЛЕЙНЫЙ ШАГ КАДРА (Variable Update)
                for (auto* plugin : plugins_) {
                    if (plugin && plugin->on_update) {
                        plugin->on_update(frame_dt);
                    }
                }

                // 3. ФАЗА ОТРИСОВКИ С ИНТЕРПОЛЯЦИЕЙ (Render Step)
                float alpha = static_cast<float>(accumulator / config_.fixed_timestep);
                for (auto* plugin : plugins_) {
                    if (plugin && plugin->on_render) {
                        plugin->on_render(alpha);
                    }
                }

                // 4. ОЧИСТКА БУФЕРА СОБЫТИЙ КАДРА
                event_bus_.clear();

                // 5. ТЕЛЕМЕТРИЯ
                stats_.total_frames++;
                frames_in_second++;
                stats_.frame_time_ms = frame_dt * 1000.0f;

                auto now = Clock::now();
                if (std::chrono::duration<float>(now - fps_timer).count() >= 1.0f) {
                    stats_.fps = static_cast<float>(frames_in_second);
                    frames_in_second = 0;
                    fps_timer = now;
                }

                if (plugins_.empty()) {
                    is_running_.store(false, std::memory_order_release);
                }
            }

            // БЕЗОПАСНАЯ ВЫГРУЗКА
            for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
                if (*it && (*it)->on_unload) {
                    try {
                        (*it)->on_unload();
                    }
                    catch (const std::exception& e) {
                        std::cerr << "[Core Error] Ошибка при выгрузке: " << e.what() << std::endl;
                    }
                }
            }

            plugins_.clear();
            event_bus_.unsubscribeAll();
            services_.clear();
            job_system_.shutdown();
        }

        void stop() {
            is_running_.store(false, std::memory_order_release);
        }

        EcsRegistry& ecs() { return ecs_; }
        EventBus& eventBus() { return event_bus_; }
        JobSystem& jobSystem() { return job_system_; }
        ServiceRegistry& services() { return services_; }
        const FrameStats& stats() const { return stats_; }

    private:
        EngineContext createEngineContext() {
            EngineContext ctx;
            ctx.ecs = &ecs_;
            ctx.event_bus = &event_bus_;
            ctx.job_system = &job_system_;

            ctx.input = static_cast<InputAPI*>(services_.get_system(sys_id::Input));
            ctx.audio = static_cast<AudioAPI*>(services_.get_system(sys_id::Audio));
            ctx.renderer = static_cast<RenderAPI*>(services_.get_system(sys_id::Renderer));
            ctx.assets = static_cast<AssetAPI*>(services_.get_system(sys_id::Assets));
            ctx.logger = static_cast<LogAPI*>(services_.get_system(sys_id::Log));

            ctx.get_system = &Application::BridgeGetSystem;
            ctx.register_system = &Application::BridgeRegisterSystem;

            ctx.get_frame_data = nullptr;
            ctx.set_frame_data = nullptr;

            return ctx;
        }
    };

    inline void* Application::BridgeGetSystem(SystemID id) {
        return s_active_instance ? s_active_instance->services().get_system(id) : nullptr;
    }

    inline void Application::BridgeRegisterSystem(SystemID id, void* ptr) {
        if (s_active_instance) {
            s_active_instance->services().register_system(id, ptr);
        }
    }

} // namespace core
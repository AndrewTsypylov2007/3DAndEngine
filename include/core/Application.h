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
#include <string>
#include <string_view>
#include <sstream>

#include "Types.h"
#include "EcsRegistry.h"
#include "EventBus.h"
#include "JobSystem.h"
#include "PluginContract.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324) // Подавление информационного предупреждения C4324 о padding для alignas(64)
#endif

namespace core {

    // ==============================================================================
    // 1. ДВУХУРОВНЕВЫЙ THREAD-SAFE SERVICE REGISTRY (O(1) Fast Slots + Dynamic Hash)
    // ==============================================================================
    class ServiceRegistry {
    private:
        static constexpr size_t FAST_SLOT_COUNT = 16;
        void* fast_slots_[FAST_SLOT_COUNT] = { nullptr };

        std::unordered_map<SystemID, void*> dynamic_services_;
        mutable std::shared_mutex           mutex_;

    public:
        ServiceRegistry() = default;
        ~ServiceRegistry() {
            clear();
        }

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

        bool has_system(SystemID id) const {
            return get_system(id) != nullptr;
        }

        void unregister_system(SystemID id) {
            std::unique_lock lock(mutex_);
            if (id > 0 && id < FAST_SLOT_COUNT) {
                fast_slots_[id] = nullptr;
            }
            else {
                dynamic_services_.erase(id);
            }
        }

        void clear() {
            std::unique_lock lock(mutex_);
            for (size_t i = 0; i < FAST_SLOT_COUNT; ++i) {
                fast_slots_[i] = nullptr;
            }
            dynamic_services_.clear();
        }
    };

    // ==============================================================================
    // 2. МАТРИЦА ПОКАДРОВОГО ОБМЕНА ДАННЫМИ (Frame Data Exchange Matrix)
    // ==============================================================================
    class FrameDataBus {
    private:
        std::unordered_map<FrameDataId, void*> data_slots_;
        mutable std::shared_mutex              mutex_;

    public:
        FrameDataBus() = default;

        void set_data(FrameDataId id, void* ptr) {
            std::unique_lock lock(mutex_);
            data_slots_[id] = ptr;
        }

        void* get_data(FrameDataId id) const {
            std::shared_lock lock(mutex_);
            auto it = data_slots_.find(id);
            return (it != data_slots_.end()) ? it->second : nullptr;
        }

        void clear_frame() {
            std::unique_lock lock(mutex_);
            data_slots_.clear();
        }
    };

    // ==============================================================================
    // 3. КОНФИГУРАЦИЯ И ТЕЛЕМЕТРИЯ ДВИЖКА
    // ==============================================================================
    struct EngineConfig {
        double      fixed_timestep = 1.0 / 60.0; // 60 Гц тактовая частота физики
        float       max_delta_time = 0.1f;        // 100 мс макс. скачок (защита от spiral of death)
        uint32_t    worker_threads = 0;           // 0 = автоопределение потоков CPU
        uint64_t    max_frames = 0;           // 0 = бесконечный цикл; >0 = ограничение (для CI / тестов)
        bool        auto_stop_on_empty = true;        // Завершать работу, если нет активных плагинов
        std::string app_name = "3DAndEngine Runtime v0.4.0";
    };

    struct FrameStats {
        uint64_t total_frames{ 0 };
        float    fps{ 0.0f };
        float    frame_time_ms{ 0.0f };
        float    min_frame_time_ms{ 9999.0f };
        float    max_frame_time_ms{ 0.0f };
        float    physics_time_ms{ 0.0f };
        float    render_time_ms{ 0.0f };
    };

    // ==============================================================================
    // 4. ГЛАВНОЕ МИКРОЯДРО ДВИЖКА (AAA Microkernel Machine)
    // ==============================================================================
    class Application {
    private:
        EcsRegistry                   ecs_;
        EventBus                      event_bus_;
        JobSystem                     job_system_;
        ServiceRegistry               services_;
        FrameDataBus                  frame_data_;

        std::atomic<bool>             is_running_{ false };
        EngineConfig                  config_;
        FrameStats                    stats_;

        std::vector<PluginInterface*> plugins_;

        // --- C-ABI NULL-РЕАЛИЗАЦИИ БАЗОВЫХ ПОДСИСТЕМ ---
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
            [](LogLevel level, const char* channel, const char* msg) {
                const char* lvl_str = "INFO";
                switch (level) {
                    case LogLevel::Trace: lvl_str = "TRACE"; break;
                    case LogLevel::Info:  lvl_str = "INFO";  break;
                    case LogLevel::Warn:  lvl_str = "WARN";  break;
                    case LogLevel::Error: lvl_str = "ERROR"; break;
                    case LogLevel::Fatal: lvl_str = "FATAL"; break;
                }
                std::cout << "[" << lvl_str << "][" << (channel ? channel : "Core") << "] "
                          << (msg ? msg : "") << "\n";
            }
        };

        // Статический мост для C-функций
        static void* BridgeGetSystem(SystemID id);
        static void  BridgeRegisterSystem(SystemID id, void* ptr);
        static void* BridgeGetFrameData(FrameDataId id);
        static void  BridgeSetFrameData(FrameDataId id, void* ptr);

        inline static Application* s_active_instance = nullptr;

    public:
        explicit Application(const EngineConfig& config = EngineConfig())
            : config_(config) {
            s_active_instance = this;

            // Регистрация систем по умолчанию
            services_.register_system(sys_id::Input, &null_input_api_);
            services_.register_system(sys_id::Audio, &null_audio_api_);
            services_.register_system(sys_id::Renderer, &null_render_api_);
            services_.register_system(sys_id::Assets, &null_asset_api_);
            services_.register_system(sys_id::Log, &null_log_api_);

            // Подписка на системные сигналы завершения
            event_bus_.subscribe("engine/exit"_id, [this](uint64_t) { stop(); });
            event_bus_.subscribe("app/quit"_id, [this](uint64_t) { stop(); });
        }

        ~Application() {
            // 1. Немедленно отключаем глобальный указатель моста, предотвращая Use-After-Free
            if (s_active_instance == this) {
                s_active_instance = nullptr;
            }

            stop();
            job_system_.shutdown();

            // 2. Безопасная выгрузка плагинов по локальной копии списка
            auto plugins_to_unload = plugins_;
            plugins_.clear();

            for (auto it = plugins_to_unload.rbegin(); it != plugins_to_unload.rend(); ++it) {
                if (*it && (*it)->on_unload) {
                    try {
                        (*it)->on_unload();
                    }
                    catch (...) {
                        // Защита от сбоя деструктора плагина
                    }
                }
            }

            event_bus_.unsubscribeAll();
            services_.clear();
        }

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        // =========================================================================
        // РЕГИСТРАЦИЯ И ЗАГРУЗКА ПЛАГИНОВ
        // =========================================================================
        void registerPlugin(PluginInterface* plugin) {
            if (!plugin) return;

            // Проверка ABI-совместимости
            if (plugin->abi_version != ENGINE_ABI_VERSION) {
                std::cerr << "[Core Error] Плагин '" << (plugin->name ? plugin->name : "Unknown")
                    << "' несовместим по ABI! (Плагин: 0x" << std::hex << plugin->abi_version
                    << ", Ядро: 0x" << ENGINE_ABI_VERSION << std::dec << ")\n";
                return;
            }

            plugins_.push_back(plugin);

            // Сортировка по приоритету выполнения (меньшее число = раньше запуск)
            std::sort(plugins_.begin(), plugins_.end(),
                [](const PluginInterface* a, const PluginInterface* b) {
                    return a->priority < b->priority;
                }
            );

            EngineContext ctx = createEngineContext();
            if (plugin->on_load) {
                try {
                    plugin->on_load(&ctx);
                }
                catch (const std::exception& e) {
                    std::cerr << "[Core Error] Исключение в on_load плагина '"
                        << (plugin->name ? plugin->name : "") << "': " << e.what() << "\n";
                }
                catch (...) {
                    std::cerr << "[Core Error] Неизвестное исключение в on_load плагина\n";
                }
            }
        }

        // =========================================================================
        // ГЛАВНЫЙ ИГРОВОЙ ЦИКЛ КАДРОВОГО КОНВЕЙЕРА (Main Loop)
        // =========================================================================
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

                // --- 1. ФИКСИРОВАННЫЙ ШАГ ФИЗИКИ (Fixed Timestep 60 Hz) ---
                auto physics_start = Clock::now();
                while (accumulator >= config_.fixed_timestep) {
                    float fixed_dt = static_cast<float>(config_.fixed_timestep);
                    size_t p_count = plugins_.size();
                    for (size_t i = 0; i < p_count && i < plugins_.size(); ++i) {
                        auto* plugin = plugins_[i];
                        if (plugin && plugin->on_fixed_update) {
                            try {
                                plugin->on_fixed_update(fixed_dt);
                            }
                            catch (const std::exception& e) {
                                std::cerr << "[Core Loop] Ошибка в on_fixed_update: " << e.what() << "\n";
                            }
                            catch (...) {}
                        }
                    }
                    accumulator -= config_.fixed_timestep;
                }
                stats_.physics_time_ms = std::chrono::duration<float, std::milli>(Clock::now() - physics_start).count();

                // --- 2. ГЕЙМПЛЕЙНЫЙ ШАГ КАДРА (Variable Update) ---
                size_t p_count = plugins_.size();
                for (size_t i = 0; i < p_count && i < plugins_.size(); ++i) {
                    auto* plugin = plugins_[i];
                    if (plugin && plugin->on_update) {
                        try {
                            plugin->on_update(frame_dt);
                        }
                        catch (const std::exception& e) {
                            std::cerr << "[Core Loop] Ошибка в on_update: " << e.what() << "\n";
                        }
                        catch (...) {}
                    }
                }

                // --- 3. ФАЗА ОТРИСОВКИ С ИНТЕРПОЛЯЦИЕЙ (Render Step) ---
                auto render_start = Clock::now();
                float alpha = static_cast<float>(accumulator / config_.fixed_timestep);
                for (size_t i = 0; i < p_count && i < plugins_.size(); ++i) {
                    auto* plugin = plugins_[i];
                    if (plugin && plugin->on_render) {
                        try {
                            plugin->on_render(alpha);
                        }
                        catch (const std::exception& e) {
                            std::cerr << "[Core Loop] Ошибка в on_render: " << e.what() << "\n";
                        }
                        catch (...) {}
                    }
                }
                stats_.render_time_ms = std::chrono::duration<float, std::milli>(Clock::now() - render_start).count();

                // --- 4. ПОСТ-КАДРОВАЯ ОБРАБОТКА СОБЫТИЙ И ОЧИСТКА ---
                event_bus_.processEvents();
                frame_data_.clear_frame();

                // --- 5. ТЕЛЕМЕТРИЯ ---
                stats_.total_frames++;
                frames_in_second++;
                stats_.frame_time_ms = frame_dt * 1000.0f;
                stats_.min_frame_time_ms = std::min(stats_.min_frame_time_ms, stats_.frame_time_ms);
                stats_.max_frame_time_ms = std::max(stats_.max_frame_time_ms, stats_.frame_time_ms);

                auto now = Clock::now();
                if (std::chrono::duration<float>(now - fps_timer).count() >= 1.0f) {
                    stats_.fps = static_cast<float>(frames_in_second);
                    frames_in_second = 0;
                    fps_timer = now;
                }

                // --- 6. ПРОВЕРКА УСЛОВИЙ ВЫХОДА ДЛЯ CI / HEADLESS ---
                if (config_.max_frames > 0 && stats_.total_frames >= config_.max_frames) {
                    is_running_.store(false, std::memory_order_release);
                    break;
                }

                if (plugins_.empty() && config_.auto_stop_on_empty) {
                    is_running_.store(false, std::memory_order_release);
                    break;
                }
            }

            // =====================================================================
            // БЕЗОПАСНАЯ ВЫГРУЗКА И ДЕИНИЦИАЛИЗАЦИЯ
            // =====================================================================
            auto plugins_to_unload = plugins_;
            plugins_.clear();

            for (auto it = plugins_to_unload.rbegin(); it != plugins_to_unload.rend(); ++it) {
                if (*it && (*it)->on_unload) {
                    try {
                        (*it)->on_unload();
                    }
                    catch (const std::exception& e) {
                        std::cerr << "[Core Error] Ошибка при выгрузке плагина: " << e.what() << "\n";
                    }
                    catch (...) {}
                }
            }

            job_system_.shutdown();
            event_bus_.unsubscribeAll();
            services_.clear();
        }

        void stop() {
            is_running_.store(false, std::memory_order_release);
        }

        bool isRunning() const {
            return is_running_.load(std::memory_order_acquire);
        }

        // =========================================================================
        // ДОСТУП К ПОДСИСТЕМАМ
        // =========================================================================
        EcsRegistry& ecs() { return ecs_; }
        EventBus& eventBus() { return event_bus_; }
        JobSystem& jobSystem() { return job_system_; }
        ServiceRegistry& services() { return services_; }
        FrameDataBus& frameData() { return frame_data_; }
        const FrameStats& stats() const { return stats_; }
        const EngineConfig& config() const { return config_; }

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

            ctx.get_frame_data = &Application::BridgeGetFrameData;
            ctx.set_frame_data = &Application::BridgeSetFrameData;

            return ctx;
        }
    };

    // ==============================================================================
    // СТАТИЧЕСКИЕ РЕАЛИЗАЦИИ МОСТОВ К C-ABI
    // ==============================================================================
    inline void* Application::BridgeGetSystem(SystemID id) {
        return s_active_instance ? s_active_instance->services().get_system(id) : nullptr;
    }

    inline void Application::BridgeRegisterSystem(SystemID id, void* ptr) {
        if (s_active_instance) {
            s_active_instance->services().register_system(id, ptr);
        }
    }

    inline void* Application::BridgeGetFrameData(FrameDataId id) {
        return s_active_instance ? s_active_instance->frameData().get_data(id) : nullptr;
    }

    inline void Application::BridgeSetFrameData(FrameDataId id, void* ptr) {
        if (s_active_instance) {
            s_active_instance->frameData().set_data(id, ptr);
        }
    }

} // namespace core

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
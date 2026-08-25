#pragma once

#include "Types.h"
#include <cstdint>
#include <cstddef>

namespace core {

    class EcsRegistry;
    class EventBus;
    class JobSystem;

    constexpr uint32_t ENGINE_ABI_VERSION = 0x00040000;

    using FrameDataId = uint64_t;

    enum class LogLevel : uint32_t {
        Trace = 0,
        Info = 1,
        Warn = 2,
        Error = 3,
        Fatal = 4
    };

    struct LogAPI {
        void (*log_message)(LogLevel level, const char* channel, const char* message);
    };

    struct InputAPI {
        bool (*is_key_pressed)(int key_code);
        bool (*is_key_just_pressed)(int key_code);
        void (*get_mouse_pos)(double* x, double* y);
        bool (*is_mouse_button_pressed)(int button);
        void (*get_mouse_delta)(double* dx, double* dy);
        void (*get_mouse_wheel)(double* x_offset, double* y_offset);
    };

    struct AudioAPI {
        uint32_t(*load_sound)(const char* filepath);
        void     (*play_sound)(uint32_t sound_id, float volume, bool loop);
        void     (*stop_sound)(uint32_t sound_id);
        void     (*set_listener_pos)(float x, float y, float z);
        void     (*set_sound_pos)(uint32_t sound_id, float x, float y, float z);
    };

    struct RenderAPI {
        void (*submit_command)(uint32_t command_type, const void* data_ptr, size_t data_size);
        void (*get_viewport_size)(uint32_t* width, uint32_t* height);
        void (*set_vsync)(bool enabled);
    };

    struct AssetAPI {
        void* (*load_binary)(const char* path, size_t* out_size);
        char* (*load_text)(const char* path);
        void  (*free_data)(void* data);
        bool  (*file_exists)(const char* path);
    };

    struct EngineContext {
        EcsRegistry* ecs = nullptr;
        EventBus* event_bus = nullptr;
        JobSystem* job_system = nullptr;

        InputAPI* input = nullptr;
        AudioAPI* audio = nullptr;
        RenderAPI* renderer = nullptr;
        AssetAPI* assets = nullptr;
        LogAPI* logger = nullptr;

        void* (*get_system)(SystemID id) = nullptr;
        void  (*register_system)(SystemID id, void* system_ptr) = nullptr;

        void* (*get_frame_data)(FrameDataId id) = nullptr;
        void  (*set_frame_data)(FrameDataId id, void* data_ptr) = nullptr;

        template<typename T>
        T* getSystem(SystemID id) const {
            if (!get_system) return nullptr;
            return static_cast<T*>(get_system(id));
        }

        template<typename T>
        void registerSystem(SystemID id, T* ptr) const {
            if (register_system) {
                register_system(id, static_cast<void*>(ptr));
            }
        }

        template<typename T>
        T* getFrameData(FrameDataId id) const {
            if (!get_frame_data) return nullptr;
            return static_cast<T*>(get_frame_data(id));
        }

        template<typename T>
        void setFrameData(FrameDataId id, T* data_ptr) const {
            if (set_frame_data) {
                set_frame_data(id, static_cast<void*>(data_ptr));
            }
        }
    };

    struct PluginInterface {
        const char* name;
        const char* version;
        uint32_t    abi_version;
        uint32_t    priority;

        void (*on_load)(EngineContext* ctx);
        void (*on_fixed_update)(float fixed_dt);
        void (*on_update)(float delta_time);
        void (*on_render)(float alpha);
        void (*on_unload)();
    };

} // namespace core

#if defined(_WIN32)
#define ENGINE_PLUGIN_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define ENGINE_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define ENGINE_PLUGIN_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif
    ENGINE_PLUGIN_EXPORT core::PluginInterface* GetPluginAPI();
#ifdef __cplusplus
}
#endif

#define DECLARE_ENGINE_PLUGIN(PluginStructInstance) extern "C" { ENGINE_PLUGIN_EXPORT core::PluginInterface* GetPluginAPI() { return &PluginStructInstance; } }
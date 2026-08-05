#pragma once
#include "EcsRegistry.h"
#include "EventBus.h"
#include "JobSystem.h"

namespace core {

    struct EngineContext {
        EcsRegistry* ecs;
        EventBus* event_bus;
        JobSystem* job_system;
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

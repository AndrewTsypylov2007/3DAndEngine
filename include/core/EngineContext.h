#pragma once

#include "Types.h"
#include <cstddef>
#include <cstdint>

namespace core {
    // Опережающие объявления базовых подсистем
    class EcsRegistry;
    class EventBus;
    class JobSystem;
}

namespace core {

    // Идентификатор слотов данных кадра
    using FrameDataId = uint64_t;

    /**
     * @brief EngineContext (Pure Decoupled C-ABI Standard)
     * Тотальная декуплизация: плагины общаются с ядром через единую коммутационную матрицу.
     */
    struct EngineContext {
        // =====================================================================
        // 1. БАЗОВЫЕ ПОДСИСТЕМЫ ЯДРА (Core Managers)
        // =====================================================================
        EcsRegistry* ecs = nullptr;
        EventBus* event_bus = nullptr;
        JobSystem* job_system = nullptr;

        // =====================================================================
        // 2. SERVICE LOCATOR (Долгоживущие интерфейсы служб и подсистем)
        // =====================================================================
        void* (*get_system)(SystemID id) = nullptr;
        void  (*register_system)(SystemID id, void* system_ptr) = nullptr;

        // =====================================================================
        // 3. FRAME BLACKBOARD (Временные данные и команды текущего кадра)
        // =====================================================================
        void* (*get_frame_data)(FrameDataId id) = nullptr;
        void  (*set_frame_data)(FrameDataId id, void* data_ptr) = nullptr;

        // =====================================================================
        // 4. ТИПОБЕЗОПАСНЫЕ C++ ХЕЛПЕРЫ (Zero-overhead Inline Helpers)
        // =====================================================================

        template<typename T>
        T* getSystem(SystemID id) const {
            if (!get_system) return nullptr;
            return static_cast<T*>(get_system(id));
        }

        template<typename T>
        void registerSystem(SystemID id, T* system_ptr) const {
            if (register_system) {
                register_system(id, static_cast<void*>(system_ptr));
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

} // namespace core
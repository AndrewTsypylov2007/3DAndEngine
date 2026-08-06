#pragma once
#include <cstdint>

namespace core {

    // Сквозные типы графических команд для любого API (Vulkan, DX12, OpenGL)
    enum eRenderCommandType : uint32_t {
        RENDER_CMD_IMGUI_UI = 0,
        RENDER_CMD_3D_MESH = 1,
        RENDER_CMD_SKIN_ANIM = 2,
        RENDER_CMD_SET_CAMERA = 3
    };

    // Универсальный DOD-пакет команды кадра
    struct RenderCommand {
        uint32_t type;
        void* data;
    };

} // namespace core

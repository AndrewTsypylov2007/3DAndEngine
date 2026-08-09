#pragma once
#include <cstdint>

namespace core {

    // ==============================================================================
    // ГРАФИЧЕСКИЙ КОНТРАКТ (v0.3.0)
    // ==============================================================================

    // Типы команд для Graphics Pipeline
    enum eRenderCommandType : uint32_t {
        RENDER_CMD_SET_CAMERA = 0, // Установка матриц View/Projection
        RENDER_CMD_DRAW_MESH = 1, // Отрисовка статического меша
        RENDER_CMD_DRAW_SKINNED = 2, // Отрисовка персонажа с анимацией
        RENDER_CMD_DRAW_UI = 3, // Команды для Dear ImGui
        RENDER_CMD_SET_PIPELINE = 4  // Смена материала/шейдера
    };

    // --- Структуры данных для команд (Data Oriented Design) ---

    struct CameraCommand {
        float view[16];       // Матрица вида
        float projection[16]; // Матрица проекции
        float position[3];    // Позиция камеры для шейдеров освещения
    };

    struct MeshCommand {
        uint64_t mesh_id;      // Какой меш рисовать (из AssetPlugin)
        uint64_t material_id;  // Какой материал использовать
        float    transform[16]; // Матрица перемещения объекта
    };

    struct UICommand {
        void* imgu_draw_data;  // Указатель на структуру ImDrawData
    };

    // Универсальный пакет команды
    struct RenderCommand {
        uint32_t type;         // eRenderCommandType
        const void* data;      // Указатель на одну из структур выше
        size_t data_size;      // Размер для безопасного копирования в буфер GPU
    };

} // namespace core

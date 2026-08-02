// include/core/InputEvent.h — Версия v0.1.5 (Ultimate Data-Driven Input)
#pragma once
#include <cstdint>

namespace core {

    // СВЕРХБЫСТРЫЙ ХЭШЕР СТРОК НА ЭТАПЕ КОМПИЛЯЦИИ (Алгоритм FNV-1a)
    // Переводит любую строку типа "wheel:gas_pedal" в уникальное число uint32_t за 0 тактов процессора в рантайме!
    constexpr uint32_t hash(const char* str, uint32_t val = 0x811C9DC5) {
        return (*str == '\0') ? val : hash(str + 1, (val ^ uint32_t(*str)) * 0x01000193);
    }

    enum class EventType : uint8_t {
        Button, // Для любых кнопок (клавиатура, мышь, геймпад, руль)
        Axis    // Для любых осей (мышь, стики, курки, рулевое колесо, педали)
    };

    // БЕСКОНЕЧНО РАСШИРЯЕМАЯ СТРУКТУРА (Весит всего 12 байт!)
    struct InputEvent {
        uint32_t  channelId; // Уникальное число (Хэш от "keyboard:w", "gamepad:lx", "wheel:clutch")
        EventType type;      // Button или Axis
        float     value;     // Чистый float (1.0/0.0 или диапазон -1.0...1.0)
    };

    enum class EditorContentType : uint8_t {
        SceneView,
        SettingsView,
        InputDebugView
    };
}

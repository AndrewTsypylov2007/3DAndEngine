// include/core/WindowHandle.h — Версия v0.1.5 (Zero-Knowledge Platform Abstraction)
#pragma once
#include <cstdint>

namespace core {

    // Тот самый сверхбыстрый хэшер строк (FNV-1a), который работает на этапе компиляции
    constexpr uint32_t platform_hash(const char* str, uint32_t val = 0x811C9DC5) {
        return (*str == '\0') ? val : platform_hash(str + 1, (val ^ uint32_t(*str)) * 0x01000193);
    }

    // ТЕПЕРЬ ТУТ НЕТ НИ ОДНОГО ENUM И НИ ОДНОГО УПОМИНАНИЯ WINDOWS/LINUX/SWITCH!
    struct WindowHandleContext {
        uint32_t platformId;       // Хэш от строки, например: hash("Windows_Win32") или hash("Nintendo_Switch3")
        void* nativeWindowHandle;   // Сырой указатель на окно ОС или контекст экрана консоли
        void* nativeDisplayHandle;  // Сырой указатель на дисплей (нужен для некоторых платформ)
    };
}

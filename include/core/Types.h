#pragma once
#include <cstdint>
#include <string_view>

namespace core {

    // Легковесные типы-дескрипторы для AAA-конвейера
    using Entity = uint32_t;
    using ComponentTypeId = uint32_t;
    using EventId = uint32_t;

    constexpr Entity NullEntity = 0;

    // Высокоскоростной компиляционный хэш FNV-1a (Zero Runtime Overhead)
    constexpr uint32_t hash_str(std::string_view str) {
        uint32_t hash = 2166136261u;
        for (char c : str) {
            hash ^= static_cast<uint32_t>(c);
            hash *= 16777619u;
        }
        return hash;
    }

    // Литерал для мгновенного синтаксиса: "physics/collision"_id
    constexpr uint32_t operator""_id(const char* str, std::size_t len) {
        return hash_str(std::string_view(str, len));
    }

} // namespace core

#pragma once
#include <cstdint>
#include <string_view>

namespace core {

    // ==============================================================================
    // AAA DESCRIPTORS (v0.3.0 - 64-bit Expansion)
    // ==============================================================================

    // Entity оставляем 32-битным (4 миллиарда сущностей достаточно для любой сцены, 
    // и это экономит память в ECS-пулах)
    using Entity = uint32_t;

    // Остальные ID переводим на 64 бит для исключения коллизий в больших системах
    using ComponentTypeId = uint64_t;
    using EventId = uint64_t;
    using SystemID = uint64_t;
    using ResourceId = uint64_t;

    constexpr Entity NullEntity = 0;

    // ==============================================================================
    // HIGH-SPEED FNV-1a 64-BIT HASH (Compile-time)
    // ==============================================================================
    // Обеспечивает нулевые затраты в рантайме и уникальность имен систем/событий.

    constexpr uint64_t hash_str(std::string_view str) {
        uint64_t hash = 14695981039346656037ull; // FNV offset basis 64-bit
        for (char c : str) {
            hash ^= static_cast<uint64_t>(c);
            hash *= 1099511628211ull; // FNV prime 64-bit
        }
        return hash;
    }

    // Литерал для мгновенного использования: "renderer"_id или "input/mouse_click"_id
    constexpr uint64_t operator""_id(const char* str, std::size_t len) {
        return hash_str(std::string_view(str, len));
    }

    // ==============================================================================
    // COMMON SYSTEM IDS
    // ==============================================================================
    // Заранее определенные хеши для базовых систем (v0.3.0 Interface Registry)

    namespace sys_id {
        constexpr SystemID Renderer = "renderer"_id;
        constexpr SystemID Input = "input"_id;
        constexpr SystemID Audio = "audio"_id;
        constexpr SystemID Assets = "assets"_id;
        constexpr SystemID Physics = "physics"_id;
        constexpr SystemID Scripts = "scripts"_id;
    }

} // namespace core

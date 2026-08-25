#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>

namespace core {

    // ==============================================================================
    // 1. СТРОГИЕ ИДЕНТИФИКАТОРЫ СИСТЕМ И РЕСУРСОВ (64-bit Expansion)
    // ==============================================================================

    using ComponentTypeId = uint64_t;
    using EventId = uint64_t;
    using SystemID = uint64_t;
    using TypeId = uint64_t;
    using FrameDataId = uint64_t;
    using ResourceId = uint64_t;
    using SubscriptionId = uint64_t;
    using PluginId = uint64_t;

    // ==============================================================================
    // 2. GENERATIONAL ENTITY (32-bit: 22 бита Index + 10 бит Generation)
    // ==============================================================================
    // 4 194 304 одновременных сущностей и 1024 поколения защиты от Use-After-Free

    using Entity = uint32_t;

    constexpr Entity NullEntity = 0;

    namespace entity_traits {
        constexpr uint32_t INDEX_BITS = 22;
        constexpr uint32_t INDEX_MASK = (1u << INDEX_BITS) - 1u;
        constexpr uint32_t GENERATION_BITS = 10;
        constexpr uint32_t GENERATION_MASK = (1u << GENERATION_BITS) - 1u;

        inline constexpr uint32_t index(Entity e) noexcept {
            return e & INDEX_MASK;
        }

        inline constexpr uint32_t generation(Entity e) noexcept {
            return (e >> INDEX_BITS) & GENERATION_MASK;
        }

        inline constexpr Entity make(uint32_t idx, uint32_t gen) noexcept {
            return (idx & INDEX_MASK) | ((gen & GENERATION_MASK) << INDEX_BITS);
        }
    }

    // ==============================================================================
    // 3. HIGH-SPEED COMPILE-TIME & RUNTIME FNV-1a 64-BIT HASH
    // ==============================================================================
    // Нулевой оверхед в рантайме при использовании литерала ""_id

    constexpr uint64_t FNV1A_64_OFFSET_BASIS = 14695981039346656037ull;
    constexpr uint64_t FNV1A_64_PRIME = 1099511628211ull;

    constexpr uint64_t hash_str(std::string_view str) noexcept {
        uint64_t hash = FNV1A_64_OFFSET_BASIS;
        for (char c : str) {
            hash ^= static_cast<uint64_t>(c);
            hash *= FNV1A_64_PRIME;
        }
        return hash;
    }

    constexpr uint64_t hash_str(const char* str) noexcept {
        uint64_t hash = FNV1A_64_OFFSET_BASIS;
        while (*str) {
            hash ^= static_cast<uint64_t>(*str++);
            hash *= FNV1A_64_PRIME;
        }
        return hash;
    }

    // Пользовательский строковый литерал: "sys/renderer"_id
    constexpr uint64_t operator""_id(const char* str, std::size_t len) noexcept {
        return hash_str(std::string_view(str, len));
    }

    // ==============================================================================
    // 4. СТАНДАРТНЫЕ ИДЕНТИФИКАТОРЫ СИСТЕМ (Predefined System Tokens)
    // ==============================================================================
    namespace sys_id {
        constexpr SystemID Window = "sys/window"_id;
        constexpr SystemID Renderer = "sys/renderer"_id;
        constexpr SystemID Input = "sys/input"_id;
        constexpr SystemID Audio = "sys/audio"_id;
        constexpr SystemID Assets = "sys/assets"_id;
        constexpr SystemID Physics = "sys/physics"_id;
        constexpr SystemID Scripts = "sys/scripts"_id;
        constexpr SystemID Log = "sys/log"_id;
    }

    // ==============================================================================
    // 5. СТАНДАРТНЫЕ ИДЕНТИФИКАТОРЫ СОБЫТИЙ ДВИЖКА (Common Event Tokens)
    // ==============================================================================
    namespace event_id {
        constexpr EventId EngineExit = "engine/exit"_id;
        constexpr EventId AppQuit = "app/quit"_id;
        constexpr EventId WindowClose = "window/close"_id;
        constexpr EventId WindowResize = "window/resize"_id;
        constexpr EventId WindowFocus = "window/focus"_id;
    }

} // namespace core
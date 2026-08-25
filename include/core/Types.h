#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>

namespace core {

    // ==============================================================================
    // 1. УНИВЕРСАЛЬНЫЕ 64-БИТНЫЕ СЛЕПЫЕ ИДЕНТИФИКАТОРЫ (Opaque Identifiers)
    // ==============================================================================

    using SystemID = uint64_t; // Идентификатор службы/сервиса
    using EventId = uint64_t; // Идентификатор события
    using ComponentTypeId = uint64_t; // Идентификатор типа компонента
    using FrameDataId = uint64_t; // Идентификатор слота данных кадра
    using TypeId = uint64_t; // Общий тип
    using ChannelId = uint64_t; // Канал передачи данных
    using ResourceId = uint64_t; // Идентификатор ресурса/ассета
    using CommandId = uint64_t; // Идентификатор команды конвейера
    using SubscriptionId = uint64_t; // Идентификатор подписки

    // Непрозрачный платформенный дескриптор
    using OpaqueHandle = void*;

    // ==============================================================================
    // 2. GENERATIONAL ENTITY (32-bit: 22 бита Index + 10 бит Generation)
    // ==============================================================================
    // До 4 194 304 одновременных сущностей и 1024 поколения защиты от Use-After-Free

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

        inline constexpr bool is_valid(Entity e) noexcept {
            return e != NullEntity;
        }
    }

    // ==============================================================================
    // 3. ZERO-COST COMPILE-TIME & RUNTIME FNV-1a 64-BIT HASH ENGINE
    // ==============================================================================
    // Гарантирует одинаковый хэш на всех платформах и компиляторах

    constexpr uint64_t FNV1A_64_OFFSET_BASIS = 14695981039346656037ull;
    constexpr uint64_t FNV1A_64_PRIME = 1099511628211ull;

    constexpr uint64_t hash_str(std::string_view str) noexcept {
        uint64_t hash = FNV1A_64_OFFSET_BASIS;
        for (char c : str) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
            hash *= FNV1A_64_PRIME;
        }
        return hash;
    }

    constexpr uint64_t hash_str(const char* str) noexcept {
        uint64_t hash = FNV1A_64_OFFSET_BASIS;
        while (*str) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(*str++));
            hash *= FNV1A_64_PRIME;
        }
        return hash;
    }

    // Пользовательские литералы для строгой слепой адресации
    constexpr uint64_t operator""_id(const char* str, std::size_t len) noexcept {
        return hash_str(std::string_view(str, len));
    }

    constexpr uint64_t operator""_sys(const char* str, std::size_t len) noexcept {
        return hash_str(std::string_view(str, len));
    }

    constexpr uint64_t operator""_evt(const char* str, std::size_t len) noexcept {
        return hash_str(std::string_view(str, len));
    }

    // ==============================================================================
    // 4. СТАНДАРТНЫЕ СЛЕПЫЕ ТОКЕНЫ СИСТЕМ (Стандартные строковые хэши)
    // ==============================================================================
    namespace sys_id {
        constexpr SystemID Window = "sys/window"_id;
        constexpr SystemID Renderer = "sys/renderer"_id;
        constexpr SystemID Input = "sys/input"_id;
        constexpr SystemID Audio = "sys/audio"_id;
        constexpr SystemID Assets = "sys/assets"_id;
        constexpr SystemID Physics = "sys/physics"_id;
        constexpr SystemID Scripts = "sys/scripts"_id;
        constexpr SystemID UI = "sys/ui"_id;
        constexpr SystemID Network = "sys/network"_id;
        constexpr SystemID Log = "sys/log"_id;
    }

    // ==============================================================================
    // 5. СТАНДАРТНЫЕ СЛЕПЫЕ ТОКЕНЫ СОБЫТИЙ (Blind Event Bus Tokens)
    // ==============================================================================
    namespace event_id {
        constexpr EventId EngineExit = "engine/exit"_id;
        constexpr EventId AppQuit = "app/quit"_id;
        constexpr EventId WindowClose = "window/close"_id;
        constexpr EventId WindowResize = "window/resize"_id;
        constexpr EventId WindowFocus = "window/focus"_id;
        constexpr EventId SceneLoaded = "scene/loaded"_id;
        constexpr EventId HotReload = "engine/hot_reload"_id;
    }

    // ==============================================================================
    // 6. УНИВЕРСАЛЬНЫЙ C-ABI СРЕЗ ПАМЯТИ (Non-Owning Memory Span)
    // ==============================================================================
    struct SpanView {
        const void* data = nullptr;
        size_t      size_bytes = 0;

        template<typename T>
        const T* as() const noexcept {
            return static_cast<const T*>(data);
        }
    };

    // Универсальный делегат выполнения слепой команды
    using GenericCommandFn = void (*)(void* user_data, uint64_t command_id, SpanView payload);

} // namespace core
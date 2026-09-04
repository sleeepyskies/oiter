#pragma once

#include "2iREN/core/assert.hpp"

#include <stdexcept>
#include <string_view>

namespace oiter {
struct MethodKind {
    enum Value {
        DepthPeeling,
        DualDepthPeeling,
        ABuffer,
        ScreenDoor,
    } value;

    constexpr MethodKind(const Value value) : value(value) {}
    constexpr operator Value() const { return value; }

    [[nodiscard]]
    constexpr auto to_string() const -> std::string_view {
        switch (value) {
            case DepthPeeling: return "DepthPeeling";
            case DualDepthPeeling: return "DualDepthPeeling";
            case ABuffer: return "ABuffer";
            case ScreenDoor: return "ScreenDoor";
            default: UNREACHABLE();
        }
    }

    [[nodiscard]] static auto from_string(const std::string_view value) -> MethodKind {
        if (value == "ddp" || value == "DualDepthPeeling") {
            return DualDepthPeeling;
        }
        if (value == "dp" || value == "DepthPeeling") {
            return DepthPeeling;
        }
        if (value == "ab" || value == "ABuffer") {
            return ABuffer;
        }
        if (value == "sd" || value == "ScreenDoor") {
            return ScreenDoor;
        }

        throw std::invalid_argument("Invalid OIT method");
    }

    [[nodiscard]] static auto default_kind() -> MethodKind { return DepthPeeling; }
};
} // namespace oiter

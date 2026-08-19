#pragma once

#include <libassert/assert.hpp>

#include <string>
#include <string_view>
#include <stdexcept>

namespace oiter {
struct MethodKind {
    enum Value {
        DualDepthPeeling,
        DepthPeeling,
        KBuffer,
        ABuffer,
    } value;

    constexpr MethodKind(const Value value) : value(value) {}
    constexpr operator Value() const { return value; }

    [[nodiscard]] constexpr auto to_string() const -> std::string {
        switch (value) {
            case DualDepthPeeling: return "DualDepthPeeling";
            case DepthPeeling: return "DepthPeeling";
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
        if (value == "kb" || value == "KBuffer") {
            return KBuffer;
        }
        if (value == "ab" || value == "ABuffer") {
            return ABuffer;
        }
        throw std::invalid_argument("Invalid OIT method");
    }
};
} // namespace oiter

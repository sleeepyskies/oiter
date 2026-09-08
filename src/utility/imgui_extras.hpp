#pragma once

#include <imgui.h>

#include "2iREN/math/bounded.hpp"

namespace oiter::ImGuiExtra {

template <siren::u32 Min, siren::u32 Max, typename BoundsPolicy>
auto SliderBoundedU32(
    const std::string_view label,
    siren::BoundedU32<Min, Max, BoundsPolicy>* value
) -> bool {
    auto val = static_cast<siren::i32>(value->get());
    if (ImGui::SliderInt(label.data(), &val, Min, Max)) {
        value->set(val);
        return true;
    }

    return false;
}

} // namespace oiter::ImGuiExtra

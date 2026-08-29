#pragma once

#include "2iREN/base.hpp"
#include "methods/method_kind.hpp"

#include <glm/vec3.hpp>

namespace oiter {
/**
 * @brief Mutable state for running in interactive mode.
 */
struct InteractiveState {
    MethodKind oit_method = MethodKind::default_kind();
    glm::vec3 camera_position;
    bool debug_menu_visible       = true;
    bool skybox_visible           = true;
};

/**
 * @brief Per frame general stats.
 */
struct FrameStats {
    siren::u32 full_frame_ms = 0;
    siren::u32 oit_render_ms = 0;
    siren::u64 frame         = 0;
    siren::f32 fps           = 0.f;
};
} // namespace oiter

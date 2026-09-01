#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>

#include "2iREN/utility/log.hpp"
#include "methods/method_kind.hpp"

namespace oiter {

/// @brief Mutable state for running in interactive mode.
struct InteractiveState {
    MethodKind oit_method = MethodKind::default_kind();
    glm::vec3 camera_position;
    bool debug_menu_visible = true;
    bool skybox_visible     = true;
};

/// @brief Per frame general stats.
struct FrameStats {
    siren::u32 full_frame_ms = 0;
    siren::u32 oit_render_ms = 0;
    siren::u64 frame         = 0;
    siren::f32 fps           = 0.f;
};

/// @brief Options for starting oiter interactive.
struct InteractiveAppOptions {
    siren::log::Level log_level = siren::log::Level::Info;
    std::string scene_path      = "oiter://assets/meshes/stresstest.glb";
    MethodKind method           = MethodKind::default_kind();
    glm::vec3 camera_position   = glm::vec3{0.f, 0.f, 2.f};
    glm::vec3 camera_lookat     = glm::vec3{-1.f, 0.f, 2.f};
};

/// @brief The Oiter interactive application. Handles launching the
/// demo/renderer and inits the 2iREN framework.
class InteractiveApp {
public:
    explicit InteractiveApp(const InteractiveAppOptions& options);
    ~InteractiveApp();

    /// @brief Launches the oiter interactive mode.
    auto run() -> void;

private:
    InteractiveState m_interactive_state;
    FrameStats m_frame_stats;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace oiter

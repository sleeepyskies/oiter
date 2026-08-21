#pragma once

#include "2iren/base.hpp"
#include "interactive_state.hpp"
#include "methods/method_kind.hpp"

#include <glm/vec3.hpp>

#include <string>

namespace oiter {
/**
 * @brief Owns an OIT application invocation.
 */
struct AppOptions {
    std::string scene_path    = "oiter://assets/meshes/stresstest.glb";
    MethodKind initial_method = MethodKind::ABuffer;
    glm::vec3 camera_position = glm::vec3{0.f, 0.f, 0.f};
    // glm::vec3 camera_lookat   = glm::vec3{0.f, 0.f, 0.f};
};

/**
 * @brief The Oiter application. Handles launching the demo/renderer and inits the 2iREN framework.
 */
class App {
public:
    explicit App(AppOptions options);

    /** @brief Launches the oiter interactive mode. */
    auto run_interactive() -> void;

    /** @brief Launches the oiter render mode. */
    auto run_render() -> void;

private:
    std::string m_scene_path;
    InteractiveState m_interactive_state;
    FrameStats m_frame_stats;
};
} // namespace oiter

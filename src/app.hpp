#pragma once

#include "2iren/base.hpp"
#include "methods/method_kind.hpp"

#include <glm/vec3.hpp>

#include <memory>
#include <string>

namespace oiter {
/**
 * @brief Owns an OIT application invocation.
 *
 * This type owns runtime state. It deliberately does not retain the parsed CLI
 * command that initialized it.
 */
struct AppConfig {
    std::string scene_path    = "oiter://assets/meshes/stresstest.glb";
    MethodKind oit_method     = MethodKind::DualDepthPeeling;
    glm::vec3 camera_position = glm::vec3{siren::f32{0}};
};

/**
 * @brief The Oiter application. Handles launching the demo/renderer and inits the 2iREN framework.
 */
class App {
public:
    explicit App(AppConfig config);
    ~App();

    /** @brief Launches the oiter interactive mode. */
    auto run_interactive() -> void;

    /** @brief Launches the oiter render mode. */
    auto run_render() -> void;

private:
    struct State;
    std::unique_ptr<State> m_state;
};
} // namespace oiter

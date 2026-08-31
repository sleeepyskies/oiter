#pragma once

#include <memory>
#include <string>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "2iREN/utility/log.hpp"

#include "methods/method_kind.hpp"

namespace oiter {

/// @brief Options for starting Oiter in interactive mode.
struct InteractiveAppOptions {
    siren::log::Level log_level = siren::log::Level::Info;
    std::string scene_path      = "oiter://assets/meshes/stresstest.glb";
    MethodKind method           = MethodKind::default_kind();
    glm::vec3 camera_position   = {0.f, 0.f, 2.f};
    glm::vec3 camera_lookat     = {-1.f, 0.f, 2.f};
};

/// @brief Options for rendering a single image.
struct RenderAppOptions {
    siren::log::Level log_level = siren::log::Level::Info;
    std::string scene_path      = "oiter://assets/meshes/stresstest.glb";
    MethodKind method           = MethodKind::default_kind();
    glm::vec3 camera_position   = {0.f, 0.f, 0.f};
    glm::vec3 camera_lookat     = {1.f, 0.f, 0.f};
    std::string output_path;
    glm::uvec2 dimensions = {1280, 720};
};

/// @brief Runs the interactive OIT renderer.
class InteractiveApp {
public:
    explicit InteractiveApp(const InteractiveAppOptions& options);
    ~InteractiveApp();

    InteractiveApp(const InteractiveApp&)                    = delete;
    auto operator=(const InteractiveApp&) -> InteractiveApp& = delete;

    /// @brief Runs the interactive frame loop until the window is closed.
    auto run() -> void;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

/// @brief Renders a single image and writes it to disk.
class RenderApp {
public:
    explicit RenderApp(const RenderAppOptions& options);
    ~RenderApp();

    RenderApp(const RenderApp&)                    = delete;
    auto operator=(const RenderApp&) -> RenderApp& = delete;

    /// @brief Renders and writes the configured image.
    auto run() -> void;

private:
    struct State;
    std::unique_ptr<State> m_state;
};
} // namespace oiter

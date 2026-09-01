#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>

#include "2iREN/utility/log.hpp"
#include "methods/method_kind.hpp"

namespace oiter {

/// @brief Options for starting oiter render.
struct RenderAppOptions {
    siren::log::Level log_level = siren::log::Level::Info;
    std::string scene_path      = "oiter://assets/meshes/stresstest.glb";
    MethodKind method           = MethodKind::default_kind();
    glm::vec3 camera_position   = glm::vec3{0.f, 0.f, 0.f};
    glm::vec3 camera_lookat     = glm::vec3{1.f, 0.f, 0.f};
    glm::uvec2 dimensions       = glm::uvec2{1280, 720};
    std::string output_path     = "./out.png";
};

/// @brief The Oiter render application. Handles launching rendering an image
/// and saving it to disk.
class RenderApp {
public:
    explicit RenderApp(const RenderAppOptions& options);
    ~RenderApp();

    /// @brief Launches the oiter render mode.
    auto run() -> void;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace oiter

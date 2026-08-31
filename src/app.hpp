#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/context.hpp"
#include "2iREN/graphics/device.hpp"
#include "2iREN/graphics/swapchain.hpp"
#include "2iREN/scene/camera.hpp"
#include "2iREN/utility/log.hpp"
#include "2iREN/window.hpp"
#include "interactive_state.hpp"
#include "methods/method_kind.hpp"
#include "methods/oit_method.hpp"
#include "skybox.hpp"

namespace oiter {
/// @brief Options for starting oiter interactive.
struct InteractiveAppOptions {
    /// @brief The log output level.
    siren::log::Level log_level = siren::log::Level::Info;
    /// @brief Path to the scene file to render. May either be a virtual or physical path.
    std::string scene_path = "oiter://assets/meshes/stresstest.glb";
    /// @brief The method to use. Note that this can be changed on the fly when running interactive
    /// mode.
    MethodKind method = MethodKind::default_kind();
    /// @brief The initial position of the camera.
    glm::vec3 camera_position = glm::vec3{0.f, 0.f, 2.f};
    /// @brief The initial lookat of the camera.
    glm::vec3 camera_lookat = glm::vec3{-1.f, 0.f, 2.f};
};

/// @brief Options for starting oiter render.
struct RenderAppOptions {
    /// @brief The log output level.
    siren::log::Level log_level = siren::log::Level::Info;
    /// @brief Path to the scene file to render. May either be a virtual or physical path.
    std::string scene_path = "oiter://assets/meshes/stresstest.glb";
    /// @brief The method to use. Note that this can be changed on the fly when running interactive
    /// mode.
    MethodKind method = MethodKind::default_kind();
    /// @brief The initial position of the camera.
    glm::vec3 camera_position = glm::vec3{0.f, 0.f, 0.f};
    /// @brief The initial lookat of the camera.
    glm::vec3 camera_lookat = glm::vec3{1.f, 0.f, 0.f};
    /// @brief The directory to save the output image into.
    std::string output_path;
    /// @brief The dimensions to render the image as.
    glm::uvec2 dimensions = glm::uvec2{1280, 720};
};

/// @brief The Oiter interactive application. Handles launching the demo/renderer and inits the
/// 2iREN framework.
class InteractiveApp {
public:
    explicit InteractiveApp(const InteractiveAppOptions& options);
    /// @brief Launches the oiter interactive mode.
    auto run() -> void;

private:
    InteractiveAppOptions m_options;
    InteractiveState m_interactive_state;
    FrameStats m_frame_stats;

    struct Resources {
        siren::Context ctx;
        siren::Window window;
        std::unique_ptr<siren::Device> device;
        siren::AssetServer assets;
        siren::Swapchain swapchain;
        std::unique_ptr<OitMethod> oit_method;
        siren::PerspectiveCamera camera;
        siren::PerspectiveCameraController controller;
        oiter::Skybox skybox;
        oiter::BakedScene scene;
    };

    std::unique_ptr<Resources> m_resources = nullptr;

    auto handle_input() -> void;
    auto create_resources() -> void;
};

/// @brief The Oiter render application. Handles launching rendering an image and saving it to disk.
class RenderApp {
public:
    explicit RenderApp(const RenderAppOptions& options);
    /// @brief Launches the oiter render mode.
    auto run() -> void;

private:
    RenderAppOptions m_options;
};
} // namespace oiter

#pragma once

#include <memory>
#include <string>

#include "2iREN/math/extent.hpp"
#include "2iREN/math/point.hpp"
#include "2iREN/utility/log.hpp"
#include "methods/method_kind.hpp"

namespace oiter {

/// @brief Options for starting oiter render.
struct RenderAppOptions {
    siren::log::Level log_level    = siren::log::Level::Info;
    std::string scene_path         = "oiter://assets/meshes/stresstest.glb";
    MethodKind method              = MethodKind::default_kind();
    siren::Point3f camera_position = siren::Point3f{0.f, 0.f, 0.f};
    siren::Point3f camera_lookat   = siren::Point3f{1.f, 0.f, 0.f};
    siren::Extent2u dimensions     = siren::Extent2u{1280, 720};
    std::string output_path        = "./out.png";
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

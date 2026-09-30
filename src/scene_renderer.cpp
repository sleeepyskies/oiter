#include "scene_renderer.hpp"

#include <memory>
#include <utility>

#include "2iREN/asset/asset_server.hpp"

#include "2iREN/graphics/commands.hpp"
#include "2iREN/graphics/fwd.hpp"
#include "2iREN/graphics/graphics_pipeline.hpp"
#include "2iREN/graphics/image.hpp"
#include "2iREN/graphics/layout.hpp"

#include "methods/a_buffer/a_buffer.hpp"
#include "methods/method_kind.hpp"
#include "methods/oit_method.hpp"
// #include "methods/screen_door/screen_door.hpp"
// #include "methods/depth_peeling/depth_peeling.hpp"
// #include "methods/dual_depth_peeling/dual_depth_peeling.hpp"
#include "utility/bake.hpp"

using namespace siren;

namespace {
auto create_method(
    const oiter::MethodKind kind,
    Device& device,
    AssetServer& assets,
    const Extent2 extent
) -> std::unique_ptr<oiter::OitMethod> {
    switch (kind) {
        case oiter::MethodKind::ABuffer:
            return std::make_unique<oiter::ABuffer>(device, extent, assets);
        /*
        case oiter::MethodKind::ScreenDoor:
            return std::make_unique<oiter::ScreenDoor>(device, extent, assets);
        case oiter::MethodKind::DepthPeeling:
            return std::make_unique<oiter::DepthPeeling>(device, extent, assets);
        case oiter::MethodKind::DualDepthPeeling:
            return std::make_unique<oiter::DualDepthPeeling>(device, extent, assets);
        */
        default: PANIC("invalid method selected");
    }
}
} // namespace

namespace oiter {
SceneRenderer::SceneRenderer(
    Device& device,
    AssetServer& assets,
    const std::string& scene_path,
    const MethodKind kind,
    const Extent2 extent
) :
    m_device(device),
    m_assets(assets),
    m_extent(extent),
    m_method(create_method(kind, device, assets, extent)) {
    // scene
    m_scene_asset = m_assets.load<Gltf>(scene_path);
    m_assets.wait_until_loaded(m_scene_asset);
    m_scene = bake_scene(m_scene_asset, m_assets);
}

auto SceneRenderer::render(
    siren::CommandBuffer& cmds,
    const siren::ImageHandle output,
    const siren::Camera& camera
) -> void {
    m_method->render(cmds, output, camera, m_scene);
}

auto SceneRenderer::method() const noexcept -> OitMethod& {
    return *m_method;
}

auto SceneRenderer::set_method(const MethodKind kind) -> void {
    m_method = create_method(kind, m_device, m_assets, m_extent);
}

auto SceneRenderer::resize(const Extent2 extent) -> void {
    m_extent = extent;
    m_method->resize(extent);
}

auto SceneRenderer::reload_shaders() -> void {
    m_method->reload_shaders();
}

} // namespace oiter

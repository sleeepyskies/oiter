#include "scene_renderer.hpp"

#include <stdexcept>
#include <utility>

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/graphics/image.hpp"
#include "2iREN/scene/camera.hpp"

#include "methods/a_buffer/a_buffer.hpp"
#include "methods/depth_peeling/depth_peeling.hpp"
#include "methods/dual_depth_peeling/dual_depth_peeling.hpp"
#include "methods/k_buffer/k_buffer.hpp"
#include "methods/oit_method.hpp"

namespace {
[[nodiscard]] auto create_method(
    const oiter::MethodKind kind,
    siren::Device& device,
    const glm::uvec2 extent,
    siren::AssetServer& assets
) -> std::unique_ptr<oiter::OitMethod> {
    switch (kind) {
        case oiter::MethodKind::DepthPeeling:
            return std::make_unique<oiter::DepthPeeling>(device, extent, assets);
        case oiter::MethodKind::DualDepthPeeling:
            return std::make_unique<oiter::DualDepthPeeling>(device, extent, assets);
        case oiter::MethodKind::ABuffer:
            return std::make_unique<oiter::ABuffer>(device, extent, assets);
        case oiter::MethodKind::KBuffer:
            return std::make_unique<oiter::KBuffer>(device, extent, assets);
        default:
            throw std::runtime_error("OIT method (" + kind.to_string() + ") is not supported.");
    }
}
} // namespace

namespace oiter {
SceneRenderer::SceneRenderer(
    siren::Device& device,
    siren::AssetServer& assets,
    const std::string& scene_path,
    const MethodKind method_kind,
    const glm::uvec2 extent
) : m_device(device), m_assets(assets), m_extent(extent), m_method_kind(method_kind) {
    m_scene_asset = m_assets.load<siren::Gltf>(scene_path);
    m_assets.wait_until_loaded(m_scene_asset);
    m_scene  = bake_scene(m_scene_asset, m_assets);
    m_method = create_method(m_method_kind, m_device, m_extent, m_assets);
}

SceneRenderer::~SceneRenderer() = default;

auto SceneRenderer::render(const siren::PerspectiveCamera& camera) -> const siren::Image& {
    return m_method->render(camera, m_scene);
}

auto SceneRenderer::resize(const glm::uvec2 extent) -> void {
    m_method->resize(extent);
    m_extent = extent;
}

auto SceneRenderer::change_method(const MethodKind method_kind) -> void {
    if (method_kind.value == m_method_kind.value) {
        return;
    }

    auto method   = create_method(method_kind, m_device, m_extent, m_assets);
    m_method      = std::move(method);
    m_method_kind = method_kind;
}

auto SceneRenderer::reload_shaders() -> void { m_method->reload_shaders(); }

auto SceneRenderer::method() noexcept -> OitMethod& { return *m_method; }

auto SceneRenderer::method_kind() const noexcept -> MethodKind { return m_method_kind; }
} // namespace oiter

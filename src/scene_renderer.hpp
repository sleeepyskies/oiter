#pragma once

#include <memory>
#include <utility>

#include "2iREN/asset/asset_handle.hpp"
#include "2iREN/asset/asset_server.hpp"
#include "2iREN/asset/gltf.hpp"
#include "2iREN/asset/shader.hpp"
#include "2iREN/graphics/device.hpp"
#include "2iREN/graphics/fwd.hpp"
#include "2iREN/graphics/image.hpp"
#include "2iREN/graphics/sampler.hpp"
#include "2iREN/math/extent.hpp"
#include "2iREN/scene/camera.hpp"

#include "methods/oit_method.hpp"
#include "utility/bake.hpp"

namespace oiter {

class SceneRenderer {
public:
    SceneRenderer(
        siren::Device& device,
        siren::AssetServer& assets,
        const std::string& scene_path,
        const MethodKind kind,
        const siren::Extent2 extent
    );

    auto render(
        siren::CommandBuffer& cmds,
        const siren::ImageHandle output,
        const siren::Camera& camera
    ) -> void;

    [[nodiscard]]
    auto method() const noexcept -> OitMethod&;

    auto set_method(MethodKind kind) -> void;
    auto resize(siren::Extent2 extent) -> void;
    auto reload_shaders() -> void;

private:
    siren::Device& m_device;
    siren::AssetServer& m_assets;
    siren::Extent2 m_extent;
    std::unique_ptr<OitMethod> m_method = nullptr;

    siren::StrongHandle<siren::Gltf> m_scene_asset = siren::NullHandle;
    BakedScene m_scene;
};

} // namespace oiter

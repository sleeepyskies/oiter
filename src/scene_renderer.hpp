#pragma once

#include <memory>
#include <string>
#include <utility>

#include "2iREN/asset/asset_handle.hpp"
#include "2iREN/asset/asset_server.hpp"
#include "2iREN/asset/gltf.hpp"
#include "2iREN/asset/shader.hpp"
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
        const siren::Extent2u extent
    );

    [[nodiscard]]
    auto render(const siren::Camera& camera) -> const siren::Image&;

    [[nodiscard]]
    auto method() noexcept -> OitMethod&;

    auto set_method(MethodKind kind) -> void;
    auto resize(siren::Extent2u extent) -> void;
    auto reload_shaders() -> void;

private:
    auto convert_format(const siren::Image& image, siren::GraphicsPipelineHandle pipeline_handle)
        -> const siren::Image&;

private:
    struct FormatConverter {
        std::unique_ptr<siren::GraphicsPipeline> pipeline = nullptr;
        siren::StrongHandle<siren::ShaderAsset> shader    = siren::NullHandle;
    };

    enum class ImageFormatGroup : siren::u8 {
        SingleChannel,
        DualChannel,
        TripleChannel,

        DepthChannel,

        MAX,
    };

    std::array<FormatConverter, std::to_underlying(ImageFormatGroup::MAX)> m_format_pipelines;

    siren::Device& m_device;
    siren::AssetServer& m_assets;
    siren::Extent2u m_extent;
    std::unique_ptr<OitMethod> m_method = nullptr;

    siren::StrongHandle<siren::Gltf> m_scene_asset = siren::NullHandle;
    BakedScene m_scene;

    std::unique_ptr<siren::Sampler> m_sampler    = nullptr;
    std::unique_ptr<siren::Sampler> m_usampler   = nullptr;
    std::unique_ptr<siren::Image> m_output_image = nullptr;
};

} // namespace oiter

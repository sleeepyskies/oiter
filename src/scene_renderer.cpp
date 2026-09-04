#include "scene_renderer.hpp"

#include <memory>
#include <utility>

#include "2iREN/asset/asset_server.hpp"

#include "2iREN/graphics/fwd.hpp"
#include "2iREN/graphics/graphics_pipeline.hpp"
#include "2iREN/graphics/image.hpp"
#include "2iREN/graphics/layout.hpp"
#include "2iREN/graphics/render_command.hpp"
#include "2iREN/graphics/render_target.hpp"
#include "methods/a_buffer/a_buffer.hpp"
#include "methods/depth_peeling/depth_peeling.hpp"
#include "methods/dual_depth_peeling/dual_depth_peeling.hpp"
#include "methods/method_kind.hpp"
#include "methods/oit_method.hpp"
#include "methods/screen_door/screen_door.hpp"
#include "utility/bake.hpp"

namespace {
auto create_method(
    const oiter::MethodKind kind,
    siren::Device& device,
    siren::AssetServer& assets,
    const siren::Extent2u extent
) -> std::unique_ptr<oiter::OitMethod> {
    switch (kind) {
        case oiter::MethodKind::DepthPeeling:
            return std::make_unique<oiter::DepthPeeling>(device, extent, assets);
        case oiter::MethodKind::DualDepthPeeling:
            return std::make_unique<oiter::DualDepthPeeling>(device, extent, assets);
        case oiter::MethodKind::ABuffer:
            return std::make_unique<oiter::ABuffer>(device, extent, assets);
        case oiter::MethodKind::ScreenDoor:
            return std::make_unique<oiter::ScreenDoor>(device, extent, assets);
        default: PANIC("invalid method selected");
    }
}
} // namespace

namespace oiter {
SceneRenderer::SceneRenderer(
    siren::Device& device,
    siren::AssetServer& assets,
    const std::string& scene_path,
    const MethodKind kind,
    const siren::Extent2u extent
) :
    m_device(device), m_assets(assets), m_method(create_method(kind, device, assets, extent)),
    m_extent(extent) {
    // samplers
    m_sampler = std::make_unique<siren::Sampler>(m_device.create_sampler({}));

    // images
    create_images();

    // scene
    m_scene_asset = m_assets.load<siren::Gltf>(scene_path);
    m_assets.wait_until_loaded(m_scene_asset);
    m_scene = bake_scene(m_scene_asset, m_assets);

    // shaders
    struct Info {
        std::string path;
        std::string label;
        ImageFormatGroup group;
    };
    std::vector<Info> infos = {
        {
            "oiter://assets/shaders/convert/r_to_rgba.sshg",
            "R to RGBA GraphicsPipeline",
            ImageFormatGroup::SingleChannel,
        },
        {
            "oiter://assets/shaders/convert/rg_to_rgba.sshg",
            "RG to RGBA GraphicsPipeline",
            ImageFormatGroup::DualChannel,
        },
        {
            "oiter://assets/shaders/convert/rgb_to_rgba.sshg",
            "RGB to RGBA GraphicsPipeline",
            ImageFormatGroup::TripleChannel,
        },
        {
            "oiter://assets/shaders/convert/depth_to_rgba.sshg",
            "Depth to RGBA GraphicsPipeline",
            ImageFormatGroup::DepthChannel,
        },
        {
            "oiter://assets/shaders/convert/ui_to_rgba.sshg",
            "Ui32 to RGBA GraphicsPipeline",
            ImageFormatGroup::UnsignedIntChannel,
        },
    };

    for (const auto& [path, label, group] : infos) {
        auto shaderhandle = m_assets.load<siren::ShaderAsset>(path);
        m_assets.wait_until_loaded(shaderhandle);
        auto& shader                                  = m_assets.get_unsafe(shaderhandle);
        m_format_pipelines[std::to_underlying(group)] = FormatConverter{
            .pipeline =
                std::make_unique<siren::GraphicsPipeline>(m_device.create_graphics_pipeline({
                    .label  = label,
                    .layout = siren::FULLSCREEN_VERTEX_LAYOUT,
                    .shader = shader.shader.handle(),
                })),
            .shader = shaderhandle,
        };
    }
}

auto SceneRenderer::render(const siren::Camera& camera) -> const siren::Image& {
    auto& image = m_method->render(camera, m_scene);

    const auto format = image.descriptor().format;

    switch (format) {
        case siren::ImageFormat::R8:
            return convert_format(
                image,
                m_format_pipelines[std::to_underlying(ImageFormatGroup::SingleChannel)]
                    .pipeline->handle()
            );

        case siren::ImageFormat::RG32f:
            return convert_format(
                image,
                m_format_pipelines[std::to_underlying(ImageFormatGroup::DualChannel)]
                    .pipeline->handle()
            );

        case siren::ImageFormat::RGB16f: // TODO: <- maybe we want a shader for this one
        case siren::ImageFormat::sRGB8:
        case siren::ImageFormat::RGB8:
            return convert_format(
                image,
                m_format_pipelines[std::to_underlying(ImageFormatGroup::TripleChannel)]
                    .pipeline->handle()
            );

        case siren::ImageFormat::Depth32f:
        case siren::ImageFormat::Depth24Stencil8:
            return convert_format(
                image,
                m_format_pipelines[std::to_underlying(ImageFormatGroup::DepthChannel)]
                    .pipeline->handle()
            );

        case siren::ImageFormat::R32UI:
            return convert_format(
                image,
                m_format_pipelines[std::to_underlying(ImageFormatGroup::UnsignedIntChannel)]
                    .pipeline->handle()
            );

        case siren::ImageFormat::RGBA16f:
        case siren::ImageFormat::sRGBA8:
        case siren::ImageFormat::RGBA8: break; // format is already fine :D

        default: PANIC("Format {} is not supported for output!", format);
    }

    return image;
}

auto SceneRenderer::convert_format(
    const siren::Image& image,
    siren::GraphicsPipelineHandle pipeline_handle
) -> const siren::Image& {
    m_device.render_pass(
        siren::RenderPassDescriptor{
            .label = "Convert Format Pass",
            .target =
                siren::RenderTarget{
                    .colors = {siren::ColorAttachment{
                        .image           = m_output_image->handle(),
                        .begin_operation = siren::BeginOperation::Clear,
                        .clear_color     = siren::Rgba::ZERO(),
                    }}
                },
        },
        [&](siren::RenderPassRecorder& pass) {
            pass.bind_graphics_pipeline(pipeline_handle);
            pass.bind_sampled_image(image.handle(), m_sampler->handle(), 0);
            pass.draw_fullscreen();
        }
    );

    return *m_output_image;
}

auto SceneRenderer::create_images() -> void {
    m_output_image = std::make_unique<siren::Image>(m_device.create_image({
        .label         = "SceneRenderer Output Image",
        .format        = siren::ImageFormat::RGBA8,
        .extent        = m_extent.to_extent3(),
        .dimension     = siren::ImageDimension::D2,
        .mipmap_levels = 1,
    }));
}

auto SceneRenderer::method() noexcept -> OitMethod& {
    return *m_method;
}

auto SceneRenderer::set_method(const MethodKind kind) -> void {
    m_method = create_method(kind, m_device, m_assets, m_extent);
}

auto SceneRenderer::resize(const siren::Extent2u extent) -> void {
    m_extent = extent;
    m_method->resize(extent);
    create_images();
}

auto SceneRenderer::reload_shaders() -> void {
    m_method->reload_shaders();
}

} // namespace oiter

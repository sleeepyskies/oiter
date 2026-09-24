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
    // samplers
    m_sampler = std::make_unique<Sampler>(m_device.make_sampler({}));

    // images
    create_images();

    // scene
    m_scene_asset = m_assets.load<Gltf>(scene_path);
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
        auto shaderhandle = m_assets.load<ShaderAsset>(path);
        m_assets.wait_until_loaded(shaderhandle);
        auto& shader                                  = m_assets.get_unsafe(shaderhandle);
        m_format_pipelines[std::to_underlying(group)] = FormatConverter{
            .pipeline = std::make_unique<GraphicsPipeline>(m_device.make_graphics_pipeline({
                .label         = label,
                .shader        = shader.shader.handle(),
                .layout        = FULLSCREEN_VERTEX_LAYOUT,
                .topology      = PrimitiveTopology::Triangles,
                .colors        = {},
                .depth_stencil = std::nullopt,
                .cull_mode     = CullMode::None,
            })),
            .shader   = shaderhandle,
        };
    }
}

auto SceneRenderer::render(
    siren::CommandBuffer& cmds,
    const siren::ImageHandle output,
    const siren::Camera& camera
) -> void {
    m_method->render(cmds, output, camera, m_scene);

    const auto format = m_device.image_descriptor(output).format;

    // TODO: use texture views here instead?
    switch (format) {
        case ImageFormat::R8:
            convert_format(
                output,
                m_format_pipelines[std::to_underlying(ImageFormatGroup::SingleChannel)]
                    .pipeline->handle()
            );

        case ImageFormat::RG32f:
            convert_format(
                output,
                m_format_pipelines[std::to_underlying(ImageFormatGroup::DualChannel)]
                    .pipeline->handle()
            );

        case ImageFormat::Depth32f:
        case ImageFormat::Depth24Stencil8:
            convert_format(
                output,
                m_format_pipelines[std::to_underlying(ImageFormatGroup::DepthChannel)]
                    .pipeline->handle()
            );

        case ImageFormat::R32UI:
            convert_format(
                output,
                m_format_pipelines[std::to_underlying(ImageFormatGroup::UnsignedIntChannel)]
                    .pipeline->handle()
            );

        case ImageFormat::BGRA8:
        case ImageFormat::RGBA16f:
        case ImageFormat::sRGBA8:
        case ImageFormat::RGBA8: break; // format is already fine :D

        default: PANIC("Format {} is not supported for output!", format);
    }
}

auto SceneRenderer::convert_format(
    const ImageHandle imagehandle,
    const GraphicsPipelineHandle pipeline_handle
) -> ImageHandle {
    auto cmds = m_device.make_command_buffer();
    cmds->render_pass(
        RenderPassDescriptor{
            .label = "Convert Format Pass",
            .target =
                RenderTarget{
                    .colors =
                        TargetColorAttachments{
                            TargetColorAttachment{
                                .image           = m_output_image->handle(),
                                .clear_color     = Rgba::ZERO(),
                                .begin_operation = BeginOperation::Clear,
                                .end_operation   = EndOperation::Store,
                            },
                        },
                    .depth_stencil = std::nullopt,
                },
        },
        [&](RenderCommandEncoder& pass) {
            pass.bind_graphics_pipeline(pipeline_handle);
            pass.bind_sampler(m_sampler->handle(), Slot{0});
            pass.bind_image(imagehandle, Slot{0});
            pass.draw(3);
        }
    );

    return m_output_image->handle();
}

auto SceneRenderer::create_images() -> void {
    m_output_image = std::make_unique<Image>(m_device.make_image({
        .label         = "SceneRenderer Output Image",
        .format        = ImageFormat::RGBA8,
        .extent        = m_extent.to_extent3(),
        .dimension     = ImageDimension::D2,
        .mipmap_levels = 1,
        .flags         = ImageFlags::make(ImageFlag::RenderAttachment),
    }));
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
    create_images();
}

auto SceneRenderer::reload_shaders() -> void {
    m_method->reload_shaders();
}

} // namespace oiter

#include "skybox.hpp"

#include <vector>

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/graphics/commands.hpp"
#include "2iREN/graphics/device.hpp"
#include "2iREN/graphics/graphics_pipeline.hpp"

namespace oiter {

using namespace siren;

namespace {

// clang-format off
const auto cube_vertices = ByteBuffer::make<f32>({
    -1.f,  1.f, -1.f,
    -1.f, -1.f, -1.f,
     1.f, -1.f, -1.f,
     1.f,  1.f, -1.f,

    -1.f,  1.f,  1.f,
    -1.f, -1.f,  1.f,
     1.f, -1.f,  1.f,
     1.f,  1.f,  1.f,
});

const auto cube_indices = ByteBuffer::make<i32>({
    0, 1, 2, 2, 3, 0,
    4, 6, 5, 6, 4, 7,
    4, 5, 1, 1, 0, 4,
    3, 2, 6, 6, 7, 3,
    0, 3, 7, 7, 4, 0,
    1, 5, 6, 6, 2, 1,
});
// clang-format on

const auto cube_layout = LayoutBuilder::make().add(DataType::Float32, 3).finish();

} // namespace

Skybox::Skybox(const std::string_view path, Device& device, AssetServer& server) :
    m_device(device), m_assets(server), m_path(path) {
    create_resources();
}

auto Skybox::render_behind(CommandBuffer& cmds, ImageHandle image, const Camera& camera) const
    -> void {
    const auto bufferdata = ByteBuffer{Uniforms{
        .projection_view = camera.projection_view(),
        .camera_position = camera.position(),
    }};
    m_uniform_buffer->upload(bufferdata.view());

    const auto& texture = m_assets.get_unsafe(m_skybox_texture);
    const auto& cube    = m_assets.get_unsafe(m_cube);
    const auto& surface = m_assets.get_unsafe(cube.surfaces[0]);

    cmds.render_pass(
        {
            .target =
                {
                    .colors =
                        TargetColorAttachments{
                            TargetColorAttachment{
                                .image           = image,
                                .clear_color     = Rgba::BLACK(),
                                .begin_operation = BeginOperation::Preserve,
                            },
                        },
                },
        },
        [&](RenderCommandEncoder& pass) {
            pass.bind_graphics_pipeline(m_skybox_pipeline->handle());
            pass.bind_uniform_buffer(m_uniform_buffer->handle(), 0, 0);
            pass.bind_sampler(texture.sampler.handle(), 0);
            pass.bind_image(texture.image.handle(), 0);
            pass.bind_vertex_buffer(surface.vertex_buffer.buffer.handle(), 0, 0);
            pass.bind_index_buffer(
                surface.index_buffer.buffer.handle(),
                surface.index_buffer.format
            );
            pass.draw_indexed(surface.index_buffer.count, 0);
        }
    );
}

auto Skybox::create_resources() -> void {
    m_uniform_buffer = std::make_unique<Buffer>(m_device.make_buffer({
        .label = "Skybox Uniform Buffer",
        .size  = sizeof(Uniforms),
        .usage = BufferFlags::from(),
    }));

    auto texture_config = TextureLoader::ConfigType{
        .name                   = std::nullopt,
        .format                 = ImageFormat::RGBA8,
        .sampler                = m_device.make_sampler({
            .s_wrap = WrapMode::ClampEdge,
            .t_wrap = WrapMode::ClampEdge,
            .r_wrap = WrapMode::ClampEdge,
        }),
        .generate_mipmap_levels = false,
    };

    m_skybox_texture = m_assets.load<Texture>(m_path, std::move(texture_config));

    m_skybox_shader = m_assets.load<ShaderAsset>("oiter://assets/shaders/skybox.sshg");

    m_assets.wait_until_loaded(m_skybox_texture);
    m_assets.wait_until_loaded(m_skybox_shader);

    const auto& shader = m_assets.get_unsafe(m_skybox_shader);

    m_skybox_pipeline = std::make_unique<GraphicsPipeline>(m_device.make_graphics_pipeline({
        .label    = "Skybox Pipeline",
        .shader   = shader.shader.handle(),
        .layout   = cube_layout,
        .topology = PrimitiveTopology::Triangles,
        .colors =
            ColorAttachmentDescriptors{
                ColorAttachmentDescriptor{
                    .format     = {},
                    .alpha_mode = AlphaMode::Blend,
                    .color_blend =
                        BlendDescription{
                            .function      = BlendFunction::Add,
                            .source_factor = BlendFactor::OneMinusDestinationAlpha,
                            .dest_factor   = BlendFactor::One
                        },
                    .alpha_blend =
                        BlendDescription{
                            .function      = BlendFunction::Add,
                            .source_factor = BlendFactor::OneMinusDestinationAlpha,
                            .dest_factor   = BlendFactor::One,
                        },
                },
            },
        .depth_stencil = std::nullopt,
        .cull_mode     = CullMode::Back,
    }));

    auto vertex_buffer = m_device.make_buffer(
        {
            .label = "Skybox Vertex Buffer",
            .size  = cube_vertices.size_bytes(),
            .usage = BufferFlags::from(BufferFlag::Vertex),
        },
        cube_vertices.view()
    );

    auto index_buffer = m_device.make_buffer(
        {
            .label = "Skybox Index Buffer",
            .size  = cube_indices.size_bytes(),
            .usage = BufferFlags::from(BufferFlag::Index),
        },
        cube_indices.view()
    );

    auto surface = std::make_unique<Surface>(
        "Skybox Surface",
        NullHandle,
        IndexBuffer{
            .buffer = std::move(index_buffer),
            .count  = cube_indices.size_as<i32>(),
            .format = IndexFormat::UInt32,
        },
        VertexBuffer{
            .buffer = std::move(vertex_buffer),
            .layout = cube_layout,
        }
    );

    m_cube = m_assets.add<Mesh>(std::make_unique<Mesh>(Mesh{
        .name     = "Skybox Cube",
        .surfaces = {
            m_assets.add<Surface>(std::move(surface)),
        },
    }));
}

} // namespace oiter

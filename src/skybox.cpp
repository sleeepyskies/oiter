#include "skybox.hpp"

#include <vector>

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/graphics/device.hpp"

namespace oiter {
namespace {

// clang-format off
const auto cube_vertices = siren::ByteBuffer::make<siren::f32>({
    -1.f,  1.f, -1.f,
    -1.f, -1.f, -1.f,
     1.f, -1.f, -1.f,
     1.f,  1.f, -1.f,

    -1.f,  1.f,  1.f,
    -1.f, -1.f,  1.f,
     1.f, -1.f,  1.f,
     1.f,  1.f,  1.f,
});

const auto cube_indices = siren::ByteBuffer::make<siren::i32>({
    0, 1, 2, 2, 3, 0,
    4, 6, 5, 6, 4, 7,
    4, 5, 1, 1, 0, 4,
    3, 2, 6, 6, 7, 3,
    0, 3, 7, 7, 4, 0,
    1, 5, 6, 6, 2, 1,
});
// clang-format on

const siren::Layout cube_layout = siren::LayoutBuilder::create()
                                      .add(siren::Attribute::Position, 3, siren::DataType::Float32)
                                      .finish();

} // namespace

Skybox::Skybox(const std::string_view path, siren::Device& device, siren::AssetServer& server) :
    m_device(device), m_assets(server), m_path(path) {
    create_resources();
}

auto Skybox::render_behind(const siren::Image& image, const siren::Camera& camera) const -> void {
    const auto bufferdata = siren::ByteBuffer{Uniforms{
        .projection_view = camera.projection_view(),
        .camera_position = camera.position(),
    }};
    m_uniform_buffer->upload(bufferdata.view());

    const auto& texture = m_assets.get_unsafe(m_skybox_texture);
    const auto& cube    = m_assets.get_unsafe(m_cube);
    const auto& surface = m_assets.get_unsafe(cube.surfaces[0]);

    m_device.render_pass(
        {
            .target =
                {
                    .colors =
                        {
                            {
                                .image           = image.handle(),
                                .begin_operation = siren::BeginOperation::Preserve,
                            },
                        },
                    .depth_stencil = std::nullopt,
                },
        },
        [&](siren::RenderPassRecorder& pass) {
            pass.bind_graphics_pipeline(m_skybox_pipeline->handle());
            pass.bind_uniform_buffer(m_uniform_buffer->handle(), 0);
            pass.bind_sampled_image(texture.image.handle(), texture.sampler.handle(), 0);
            pass.bind_vertex_buffer(surface.vertex_buffer.buffer.handle(), 0, 0);
            pass.bind_index_buffer(
                surface.index_buffer.buffer.handle(), surface.index_buffer.format
            );
            pass.draw_indexed(surface.index_buffer.count, 0);
        }
    );
}

auto Skybox::create_resources() -> void {
    m_uniform_buffer = std::make_unique<siren::Buffer>(m_device.make_buffer({
        .label = "Skybox Uniform Buffer",
        .size  = sizeof(Uniforms),
        .usage = siren::BufferUsage::Static,
    }));

    siren::TextureLoader::ConfigType texture_config{
        .name                   = std::nullopt,
        .format                 = siren::ImageFormat::RGBA8,
        .sampler                = m_device.make_sampler({
            .s_wrap = siren::ImageWrapMode::ClampEdge,
            .t_wrap = siren::ImageWrapMode::ClampEdge,
            .r_wrap = siren::ImageWrapMode::ClampEdge,
        }),
        .generate_mipmap_levels = false,
    };

    m_skybox_texture = m_assets.load<siren::Texture>(m_path, std::move(texture_config));

    m_skybox_shader = m_assets.load<siren::ShaderAsset>("oiter://assets/shaders/skybox.sshg");

    m_assets.wait_until_loaded(m_skybox_texture);
    m_assets.wait_until_loaded(m_skybox_shader);

    const auto& shader = m_assets.get_unsafe(m_skybox_shader);

    m_skybox_pipeline = std::make_unique<siren::GraphicsPipeline>(m_device.make_graphics_pipeline({
        .label    = "Skybox Pipeline",
        .layout   = cube_layout,
        .shader   = shader.shader.handle(),
        .topology = siren::PrimitiveTopology::Triangles,

        .alpha_mode = siren::AlphaMode::Blend,
        .color_blend =
            {
                .function      = siren::BlendFunction::Add,
                .source_factor = siren::BlendFactor::OneMinusDestinationAlpha,
                .dest_factor   = siren::BlendFactor::One,
            },
        .alpha_blend =
            {
                .function      = siren::BlendFunction::Add,
                .source_factor = siren::BlendFactor::OneMinusDestinationAlpha,
                .dest_factor   = siren::BlendFactor::One,
            },

        .back_face_culling = false,
        .depth_test        = false,
        .depth_write       = false,
    }));

    auto vertex_buffer = m_device.make_buffer(
        {
            .label = "Skybox Vertex Buffer",
            .size  = cube_vertices.size_bytes(),
            .usage = siren::BufferUsage::Static,
        },
        cube_vertices.view()
    );

    auto index_buffer = m_device.make_buffer(
        {
            .label = "Skybox Index Buffer",
            .size  = cube_indices.size_bytes(),
            .usage = siren::BufferUsage::Static,
        },
        cube_indices.view()
    );

    auto surface = std::make_unique<siren::Surface>(
        "Skybox Surface",
        siren::NullHandle,
        siren::IndexBuffer{
            .buffer = std::move(index_buffer),
            .count  = cube_indices.size_as<siren::i32>(),
            .format = siren::IndexFormat::UInt32,
        },
        siren::VertexBuffer{
            .buffer = std::move(vertex_buffer),
            .layout = cube_layout,
        }
    );

    m_cube = m_assets.add<siren::Mesh>(std::make_unique<siren::Mesh>(siren::Mesh{
        .name     = "Skybox Cube",
        .surfaces = {
            m_assets.add<siren::Surface>(std::move(surface)),
        },
    }));
}

} // namespace oiter

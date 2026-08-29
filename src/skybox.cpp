#include "skybox.hpp"

#include <iterator>
#include <vector>

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/rhi/device.hpp"

namespace oiter {
namespace {

constexpr siren::f32 cube_vertices[] = {
    -1.f,  1.f, -1.f,
    -1.f, -1.f, -1.f,
     1.f, -1.f, -1.f,
     1.f,  1.f, -1.f,

    -1.f,  1.f,  1.f,
    -1.f, -1.f,  1.f,
     1.f, -1.f,  1.f,
     1.f,  1.f,  1.f,
};

constexpr siren::u32 cube_indices[] = {
    0, 1, 2, 2, 3, 0,
    4, 6, 5, 6, 4, 7,
    4, 5, 1, 1, 0, 4,
    3, 2, 6, 6, 7, 3,
    0, 3, 7, 7, 4, 0,
    1, 5, 6, 6, 2, 1,
};

const siren::Layout cube_layout = siren::LayoutBuilder::create()
    .add(siren::Attribute::Position, 3, siren::DataType::Float32)
    .finish();

} // namespace

Skybox::Skybox(
    const std::string_view path,
    siren::Device& device,
    siren::AssetServer& server
) : m_device(device),
    m_assets(server),
    m_path(path) {
    create_resources();
}

auto Skybox::render_behind(
    const siren::Image& image,
    const siren::PerspectiveCamera& camera
) const -> void {
    m_uniform_buffer->upload(
        Uniforms{
            .projection_view = camera.projection_view(),
            .camera_position = camera.position(),
        }
    );

    const auto& texture = m_assets.get_unsafe(m_skybox_texture);
    const auto& cube = m_assets.get_unsafe(m_cube);
    const auto& surface = m_assets.get_unsafe(cube.surfaces[0]);

    m_device.render_pass(
        {
            .target = {
                .colors = {
                    {
                        .image = image.handle(),
                        .begin_operation = siren::BeginOperation::Preserve,
                    },
                },
                .depth_stencil = std::nullopt,
            },
        },
        [&](siren::RenderPassRecorder& pass) {
            pass.bind_graphics_pipeline(m_skybox_pipeline->handle());
            pass.bind_uniform_buffer(m_uniform_buffer->handle(), 0);
            pass.bind_sampled_image(
                texture.image.handle(),
                texture.sampler.handle(),
                0
            );
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
    m_uniform_buffer = std::make_unique<siren::Buffer>(
        m_device.create_buffer(
            {
                .label = "Skybox Uniform Buffer",
                .data = std::nullopt,
                .size = sizeof(Uniforms),
                .usage = siren::BufferUsage::Static,
            }
        )
    );

    siren::TextureLoader::ConfigType texture_config{
        .name = std::nullopt,
        .format = siren::ImageFormat::RGBA8,
        .sampler = m_device.create_sampler(
            {
                .s_wrap = siren::ImageWrapMode::ClampEdge,
                .t_wrap = siren::ImageWrapMode::ClampEdge,
                .r_wrap = siren::ImageWrapMode::ClampEdge,
            }
        ),
        .generate_mipmap_levels = false,
    };

    m_skybox_texture = m_assets.load<siren::Texture>(
        m_path,
        std::move(texture_config)
    );

    m_skybox_shader = m_assets.load<siren::ShaderAsset>(
        "oiter://assets/shaders/skybox.sshg"
    );

    m_assets.wait_until_loaded(m_skybox_texture);
    m_assets.wait_until_loaded(m_skybox_shader);

    const auto& shader = m_assets.get_unsafe(m_skybox_shader);

    m_skybox_pipeline = std::make_unique<siren::GraphicsPipeline>(
        m_device.create_graphics_pipeline(
            {
                .label = "Skybox Pipeline",
                .layout = cube_layout,
                .shader = shader.shader.handle(),
                .topology = siren::PrimitiveTopology::Triangles,

                .alpha_mode = siren::AlphaMode::Blend,
                .color_blend = {
                    .function = siren::BlendFunction::Add,
                    .source_factor = siren::BlendFactor::OneMinusDestinationAlpha,
                    .dest_factor = siren::BlendFactor::One,
                },
                .alpha_blend = {
                    .function = siren::BlendFunction::Add,
                    .source_factor = siren::BlendFactor::OneMinusDestinationAlpha,
                    .dest_factor = siren::BlendFactor::One,
                },

                .back_face_culling = false,
                .depth_test = false,
                .depth_write = false,
            }
        )
    );

    auto vertex_buffer = m_device.create_buffer(
        {
            .label = "Skybox Vertex Buffer",
            .data = std::vector(
                reinterpret_cast<const siren::u8*>(cube_vertices),
                reinterpret_cast<const siren::u8*>(cube_vertices)
                    + sizeof(cube_vertices)
            ),
            .size = sizeof(cube_vertices),
            .usage = siren::BufferUsage::Static,
        }
    );

    auto index_buffer = m_device.create_buffer(
        {
            .label = "Skybox Index Buffer",
            .data = std::vector(
                reinterpret_cast<const siren::u8*>(cube_indices),
                reinterpret_cast<const siren::u8*>(cube_indices)
                    + sizeof(cube_indices)
            ),
            .size = sizeof(cube_indices),
            .usage = siren::BufferUsage::Static,
        }
    );

    auto surface = std::make_unique<siren::Surface>(
        "Skybox Surface",
        siren::NullHandle,
        siren::IndexBuffer{
            .buffer = std::move(index_buffer),
            .count = std::size(cube_indices),
            .format = siren::IndexFormat::UInt32,
        },
        siren::VertexBuffer{
            .buffer = std::move(vertex_buffer),
            .layout = cube_layout,
        }
    );

    m_cube = m_assets.add<siren::Mesh>(
        std::make_unique<siren::Mesh>(
            siren::Mesh{
                .name = "Skybox Cube",
                .surfaces = {
                    m_assets.add<siren::Surface>(std::move(surface)),
                },
            }
        )
    );
}

} // namespace oiter

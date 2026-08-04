#include "oit_method.hpp"

#include "2iren/asset/asset_server.hpp"
#include "2iren/rhi/device.hpp"

namespace oiter {
static constexpr siren::f32 cube_vertices[] = {
    -1.f, 1.f, -1.f,
    -1.f, -1.f, -1.f,
    1.f, -1.f, -1.f,
    1.f, 1.f, -1.f,

    -1.f, 1.f, 1.f,
    -1.f, -1.f, 1.f,
    1.f, -1.f, 1.f,
    1.f, 1.f, 1.f,
};

static constexpr siren::u32 cube_indices[] = {
    // back
    0, 1, 2,
    2, 3, 0,

    // front
    4, 6, 5,
    6, 4, 7,

    // left
    4, 5, 1,
    1, 0, 4,

    // right
    3, 2, 6,
    6, 7, 3,

    // top
    0, 3, 7,
    7, 4, 0,

    // bottom
    1, 5, 6,
    6, 2, 1,
};

static const siren::Layout cube_layout = siren::LayoutBuilder::start()
                                         .add(siren::Attribute::Position, 3, siren::DataType::Float32)
                                         .finish();

OitMethod::OitMethod(siren::Device& device, siren::AssetServer& assets)
    : m_device(device),
      m_assets(assets) {
    create_buffers();
    create_background_resources();
}

auto OitMethod::update_buffers(const siren::PerspectiveCamera& camera, const BakedScene& scene) const -> void {
    m_scene_buffer->upload(
        SceneUniforms{
            .projection_view = camera.projection_view(),
            .camera_position = camera.position()
        }
    );

    if (!m_scene_updated) { return; }
    m_scene_updated = false;
    ASSERT(scene.opaque.size() + scene.transparent.size() <= MAX_MESHES);

    const auto alignment = siren::align_up(sizeof(MeshUniforms), m_device.limits().uniform_buffer_offset_alignment);

    siren::ByteBuffer buffer;

    const auto append_meshes = [&](const auto& surfaces) {
        for (const auto& surface : surfaces) {
            buffer.append(
                MeshUniforms{
                    .material = scene.materials[surface.material_index],
                    .model    = surface.transform,
                },
                alignment
            );
        }
    };

    append_meshes(scene.opaque);
    append_meshes(scene.transparent);
    m_mesh_buffer->upload(buffer);
}

auto OitMethod::render_skybox() const -> void {
    auto& texture = m_assets.get_unsafe(m_skybox_texture);
    auto& cube    = m_assets.get_unsafe(m_cube);
    auto& surface = m_assets.get_unsafe(cube.surfaces[0]);

    m_device.render_pass(
        {.target = m_skybox_render_target},
        [&](siren::RenderPassRecorder& pass) {
            pass.bind_graphics_pipeline(m_skybox_pipeline->handle());
            pass.bind_uniform_buffer(m_scene_buffer->handle(), 0);
            pass.bind_sampled_image(texture.image.handle(), texture.sampler.handle(), 0);
            pass.bind_vertex_buffer(surface.vertex_buffer.buffer.handle(), 0, 0);
            pass.bind_index_buffer(surface.index_buffer.buffer.handle(), surface.index_buffer.format);
            pass.draw_indexed(surface.index_buffer.count, 0);
        }
    );
}

auto OitMethod::create_buffers() -> void {
    m_scene_buffer = std::make_unique<siren::Buffer>(
        m_device.create_buffer(
            {
                .label = "scene uniforms",
                .data  = std::nullopt,
                .size  = sizeof(SceneUniforms),
                .usage = siren::BufferUsage::Static,
            }
        )
    );

    m_mesh_buffer = std::make_unique<siren::Buffer>(
        m_device.create_buffer(
            {
                .label = "mesh uniforms",
                .data  = std::nullopt,
                .size  = siren::align_up(
                    sizeof(MeshUniforms),
                    m_device.limits().uniform_buffer_offset_alignment
                ) * MAX_MESHES,
                .usage = siren::BufferUsage::Static,
            }
        )
    );
}

auto OitMethod::create_background_resources() -> void {
    siren::TextureLoader::ConfigType config{
        .name                   = std::nullopt,
        .format                 = siren::ImageFormat::RGBA8,
        .sampler                = m_device.create_sampler({}),
        .generate_mipmap_levels = false,
    };

    m_skybox_texture = m_assets.load<siren::Texture>(
        "oiter://assets/textures/skybox/skybox.cubemap",
        std::move(config)
    );

    m_skybox_shader          = m_assets.load<siren::ShaderAsset>("oiter://assets/shaders/skybox.sshg");
    const auto& shader_asset = m_assets.get_unsafe(m_skybox_shader);
    m_skybox_pipeline        = std::make_unique<siren::GraphicsPipeline>(
        m_device.create_graphics_pipeline(
            {
                .label             = "Skybox Pipeline",
                .layout            = cube_layout,
                .shader            = shader_asset.shader.handle(),
                .topology          = siren::PrimitiveTopology::Triangles,
                .alpha_mode        = siren::AlphaMode::Opaque,
                .depth_function    = siren::DepthFunction::Less,
                .color_blend       = {},
                .alpha_blend       = {},
                .back_face_culling = false,
                .depth_test        = true,
                .depth_write       = true,
            }
        )
    );

    auto vertex_buffer = m_device.create_buffer(
        {
            .label = "Skybox Vertex Buffer",
            .data  = std::vector(
                reinterpret_cast<const siren::u8*>(cube_vertices),
                reinterpret_cast<const siren::u8*>(cube_vertices) + sizeof(cube_vertices)
            ),
            .size  = sizeof(cube_vertices),
            .usage = siren::BufferUsage::Static,
        }
    );

    auto index_buffer = m_device.create_buffer(
        {
            .label = "Skybox Index Buffer",
            .data  = std::vector(
                reinterpret_cast<const siren::u8*>(cube_indices),
                reinterpret_cast<const siren::u8*>(cube_indices) + sizeof(cube_indices)
            ),
            .size  = sizeof(cube_indices),
            .usage = siren::BufferUsage::Static,
        }
    );

    auto surface = std::make_unique<siren::Surface>(
        "Skybox Surface",
        siren::NullHandle,
        siren::IndexBuffer{
            .buffer = std::move(index_buffer),
            .count  = std::size(cube_indices),
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
                .name     = "Skybox Cube",
                .surfaces = {
                    m_assets.add<siren::Surface>(std::move(surface))
                }
            }
        )
    );

    m_skybox_target_image = std::make_unique<siren::Image>(
        m_device.create_image(
            {
                .format = siren::ImageFormat::RGBA8,
                // todo: this NEEDS to be resizable
                .extent        = {.width = 1280, .height = 720},
                .dimension     = siren::ImageDimension::D2,
                .mipmap_levels = 1,
            }
        )
    );

    m_skybox_render_target = siren::RenderTarget{
        .colors = {
            {
                .image           = m_skybox_target_image->handle(),
                .begin_operation = siren::BeginOperation::Clear,
                .clear_color     = siren::Rgba::zero()
            },
        },
        .depth_stencil = std::nullopt,
    };

    while (!m_assets.is_loaded_with_dependencies(m_skybox_texture)) {
        /** wait lmao */
    }
}
} // namespace oiter

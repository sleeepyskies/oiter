#include "dual_depth_peeling.hpp"

#include "../../bake.hpp"
#include "2iren/rhi/device.hpp"

namespace oiter {
DualDepthPeeling::DualDepthPeeling(siren::Device& device, const glm::uvec2 extent, siren::AssetServer& server) :
    m_device(device),
    m_sampler(init_sampler(device)),
    m_uniforms(init_uniforms(device)),
    m_init(device, server),
    m_peel(device, server, extent),
    m_blend(device, server, extent),
    m_final(device, server, extent) {}

auto DualDepthPeeling::render(
    const siren::PerspectiveCamera& camera,
    const BakedScene& scene
) const
    -> const siren::Image& {
    const auto aligned_ubo_size = siren::align_up(
        sizeof(PerMeshData),
        m_device.limits().uniform_buffer_offset_alignment
    );

    {
        SceneData data;
        data.view_projection = camera.projection_view();
        data.camera_position = camera.position();
        m_uniforms.scene_data.upload(siren::ByteBuffer{data});
    }

    // do we want to do this each frame?
    {
        MaterialData data;
        for (const auto& [index, material] : std::views::enumerate(scene.materials)) {
            data.materials[index] = material;
        }
        m_uniforms.material_data.upload(siren::ByteBuffer{data});
    }

    {
        siren::ByteBuffer buffer;
        ASSERT(scene.opaque.size() + scene.transparent.size() <= MAX_MESHES);
        for (const auto& surface : scene.opaque) {
            buffer.append(
                PerMeshData{.model = surface.transform, .material_index = surface.material_index},
                aligned_ubo_size
            );
        }
        for (const auto& surface : scene.transparent) {
            buffer.append(
                PerMeshData{.model = surface.transform, .material_index = surface.material_index},
                aligned_ubo_size
            );
        }
        m_uniforms.per_mesh_data.upload(siren::ByteBuffer{buffer});
    }

    m_init.execute(scene, m_peel.read_target().render_target, m_uniforms, aligned_ubo_size);

    // todo: query to see if we can early stop
    for (const auto _ : siren::range(MAX_PEELS)) {
        m_peel.execute(scene, m_sampler, m_uniforms, aligned_ubo_size);
        m_peel.swap_targets();
        m_blend.execute(m_sampler, m_peel.read_target());
    }
    return m_final.execute(m_sampler, m_peel.read_target());
}

auto DualDepthPeeling::resize(const glm::uvec2 extent) -> void {
    m_peel.resize(extent);
    m_blend.resize(extent);
    m_final.resize(extent);
}

void DualDepthPeeling::render_debug_info() {
    // todo: impl some debug stuffs here, maybe put into own panel class? idk
}

// ==================== DATA INIT ==============

auto DualDepthPeeling::init_uniforms(siren::Device& device) const -> DualDepthPeelingUniforms {
    siren::log::info("ALIGNED UBO SISE {}", device.limits().uniform_buffer_offset_alignment);

    return DualDepthPeelingUniforms{
        .scene_data = device.create_buffer(
            {
                .label = "UBO: Scene Data",
                .size  = sizeof(SceneData),
                .usage = siren::BufferUsage::Static,
            }
        ),
        .material_data = device.create_buffer(
            {
                .label = "UBO: Material Data",
                .size  = sizeof(MaterialData),
                .usage = siren::BufferUsage::Static,
            }
        ),
        .per_mesh_data = device.create_buffer(
            {
                .label = "UBO: Per Mesh Data",
                .size  = siren::align_up(
                    sizeof(PerMeshData),
                    device.limits().uniform_buffer_offset_alignment
                ) * MAX_MESHES,
                .usage = siren::BufferUsage::Dynamic,
            }
        ),
    };
}

auto DualDepthPeeling::init_sampler(siren::Device& device) const -> siren::Sampler {
    // todo: should we change these params?
    return device.create_sampler(
        {
            .min_filter    = siren::ImageFilterMode::Linear,
            .max_filter    = siren::ImageFilterMode::Nearest,
            .mipmap_filter = siren::ImageFilterMode::Nearest,
            .s_wrap        = siren::ImageWrapMode::ClampEdge,
            .t_wrap        = siren::ImageWrapMode::ClampEdge,
        }
    );
}
} // namespace oiter

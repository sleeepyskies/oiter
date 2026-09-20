#include "oit_method.hpp"

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/container/byte_buffer.hpp"
#include "2iREN/graphics/buffer.hpp"
#include "2iREN/graphics/device.hpp"
#include "2iREN/math/extent.hpp"

namespace oiter {

using namespace siren;

OitMethod::OitMethod(Device& device, AssetServer& assets) : m_device(device), m_assets(assets) {
    create_buffers();
}

auto OitMethod::update_buffers(const Camera& camera, const BakedScene& scene) const -> void {
    const auto scenebuffer = ByteBuffer{SceneUniforms{
        .projection_view = camera.projection_view(),
        .camera_position = camera.position(),
    }};
    m_scene_buffer->upload(scenebuffer.view());

    ASSERT(scene.opaque.size() + scene.transparent.size() <= MAX_MESHES);

    const auto alignment =
        align_up(sizeof(MeshUniforms), m_device.limits().uniform_buffer_offset_alignment);

    ByteBuffer buffer;

    const auto append_meshes = [&](const auto& surfaces) {
        for (const auto& surface : surfaces) {
            buffer.write(
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
    m_mesh_buffer->upload(buffer.view());
}

auto OitMethod::create_buffers() -> void {
    m_scene_buffer = std::make_unique<Buffer>(m_device.make_buffer({
        .label        = "scene uniforms",
        .size         = sizeof(SceneUniforms),
        .usage        = BufferFlags::from(BufferFlag::Uniform),
        .memory_usage = BufferMemoryUsage::CpuAndGpu,
    }));

    m_mesh_buffer = std::make_unique<Buffer>(m_device.make_buffer({
        .label = "mesh uniforms",
        .size  = align_up(sizeof(MeshUniforms), m_device.limits().uniform_buffer_offset_alignment)
            * MAX_MESHES,
        .usage        = BufferFlags::from(BufferFlag::Uniform),
        .memory_usage = BufferMemoryUsage::CpuAndGpu,
    }));
}
} // namespace oiter

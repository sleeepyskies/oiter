#pragma once

#include <glm/matrix.hpp>
#include "../../bake.hpp"
#include "2iren/base.hpp"

constexpr auto MAX_MATERIALS = 256;
constexpr auto MAX_MESHES = 512;

namespace oiter {
struct alignas(16) SceneData {
    glm::mat4 view_projection;
    glm::vec3 camera_position;
    siren::f32 _pad = 0;
};

struct alignas(16) MaterialData {
    std::array<BakedMaterial, MAX_MATERIALS> materials;
};

struct alignas(16) PerMeshData {
    glm::mat4 model;
    siren::u32 material_index;
};

struct DualDepthPeelingUniforms {
    siren::Buffer scene_data;
    siren::Buffer material_data;
    siren::Buffer per_mesh_data;
};
} // namespace oiter

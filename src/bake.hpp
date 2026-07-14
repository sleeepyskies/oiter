#pragma once

#include "2iren/asset/assets/gltf.hpp"
#include "2iren/asset/assets/mesh.hpp"
#include "2iren/rhi/resources/fwd.hpp"

namespace oiter {

struct BakedSurface {
    siren::VertexBuffer& vertex;
    siren::IndexBuffer& index;
    glm::mat4 transform;
    siren::usize material_index;
};

struct alignas(16) BakedMaterial {
    siren::RGBA base_color = siren::RGBA::black();
};

struct BakedScene {
    std::vector<BakedSurface> transparent{};
    std::vector<BakedSurface> opaque{};
    std::vector<BakedMaterial> materials{};
};

auto bake_scene(const siren::StrongHandle<siren::Gltf>& gltf_handle, siren::AssetServer& server) -> BakedScene;

}  // namespace oiter

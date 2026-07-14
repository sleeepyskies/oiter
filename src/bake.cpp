#include "bake.hpp"

#include "2iren/asset/asset_server.hpp"

namespace oiter {

static auto bake_node(BakedScene& scene,
                      siren::AssetServer& server,
                      const siren::GltfNode& node,
                      const glm::mat4& ptransform) -> void {
    // bake this node
    if (node.mesh) {
        for (const siren::Mesh& mesh = server.get_unsafe(*node.mesh); const auto& surfaceh : mesh.surfaces) {
            siren::Surface& surface                 = server.get_unsafe(surfaceh);
            const siren::PBRMaterialAsset& material = server.get_unsafe(surface.material);

            siren::RGBA color = material.base_color();
            color.a = material.alpha_cutoff();
            scene.materials.emplace_back(color);

            BakedSurface baked_surface{
                surface.vertex_buffer,
                surface.index_buffer,
                ptransform * node.transform,  // todo: carry parent transform
                scene.materials.size() - 1,
            };

            switch (material.alpha_mode()) {
                case siren::AlphaMode::Blend: scene.transparent.emplace_back(std::move(baked_surface)); break;

                case siren::AlphaMode::Opaque:
                case siren::AlphaMode::Mask: scene.opaque.emplace_back(std::move(baked_surface)); break;
            }
        }
    }

    // recurse on children
    for (const auto& childh : node.children) {
        const siren::GltfNode& child = server.get_unsafe(childh);
        bake_node(scene, server, child, ptransform * node.transform);
    }
}

auto bake_scene(const siren::StrongHandle<siren::Gltf>& gltf_handle, siren::AssetServer& server) -> BakedScene {
    BakedScene baked{};

    auto& gltf = server.get_unsafe(gltf_handle);

    auto sceneh = gltf.default_scene.value_or(gltf.scenes[0]);
    auto& scene = server.get_unsafe(sceneh);

    for (const auto& nodeh : scene.root_nodes) {
        auto& node = server.get_unsafe(nodeh);
        bake_node(baked, server, node, glm::mat4{ 1 });
    }

    return baked;
}

}  // namespace oiter

#include "bake.hpp"

#include "2iren/asset/asset_server.hpp"

namespace oiter {

template <typename A>
static auto get_or_crash(const siren::StrongHandle<A>& handle, siren::AssetServer& server) -> A& {
    auto* asset = server.get(handle);
    if (!asset) {
        siren::log::error("Could not load {} from server.", siren::typename_of<A>());
        std::exit(1);
    }

    return *asset;
}

static auto bake_node(BakedScene& scene,
                      siren::AssetServer& server,
                      const siren::GltfNode& node,
                      const glm::mat4& ptransform) -> void {
    // bake this node
    if (node.mesh) {
        for (const siren::Mesh& mesh = get_or_crash(*node.mesh, server); const auto& surfaceh : mesh.surfaces) {
            siren::Surface& surface                 = get_or_crash(surfaceh, server);
            const siren::PBRMaterialAsset& material = get_or_crash(surface.material, server);

            scene.materials.emplace_back(material.base_color());

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
        const siren::GltfNode& child = get_or_crash(childh, server);
        bake_node(scene, server, child, ptransform * node.transform);
    }
}

auto bake_scene(const siren::StrongHandle<siren::Gltf>& gltf_handle, siren::AssetServer& server) -> BakedScene {
    BakedScene baked{};

    auto& gltf = get_or_crash(gltf_handle, server);

    auto sceneh = gltf.default_scene.value_or(gltf.scenes[0]);
    auto& scene = get_or_crash(sceneh, server);

    for (const auto& nodeh : scene.root_nodes) {
        auto& node = get_or_crash(nodeh, server);
        bake_node(baked, server, node, glm::mat4{ 1 });
    }

    return baked;
}

}  // namespace oiter

#include "bake.hpp"

#include "2iren/asset/asset_server.hpp"
#include "2iren/util/log.hpp"

namespace oiter {

static auto bake_node(BakedScene& scene,
                      siren::AssetServer& server,
                      const siren::GltfNode& node,
                      const glm::mat4& ptransform,
                      const std::optional<siren::f32> forced_alpha) -> void {
    // bake this node
    const auto world_transform = ptransform * node.transform;
    if (node.mesh) {
        for (const siren::Mesh& mesh = server.get_unsafe(*node.mesh); const auto& surfaceh : mesh.surfaces) {
            siren::Surface& surface              = server.get_unsafe(surfaceh);
            const siren::MaterialAsset& material = server.get_unsafe(surface.material);

            siren::Rgba color = material.base_color();
            if (forced_alpha.has_value()) {
                color.a = forced_alpha.value();
            }
            scene.materials.emplace_back(color);
            siren::log::debug("Added material with color: {}", color);

            BakedSurface baked_surface{
                surface.vertex_buffer,
                surface.index_buffer,
                world_transform,
                scene.materials.size() - 1,
            };

            if (forced_alpha.has_value()) {
                scene.transparent.emplace_back(std::move(baked_surface));
            } else {
                switch (material.alpha_mode()) {
                    case siren::AlphaMode::Blend: scene.transparent.emplace_back(std::move(baked_surface)); break;

                    case siren::AlphaMode::Opaque:
                    case siren::AlphaMode::Mask: scene.opaque.emplace_back(std::move(baked_surface)); break;
                }
            }
        }
    }

    // recurse on children
    for (const auto& childh : node.children) {
        const siren::GltfNode& child = server.get_unsafe(childh);
        bake_node(scene, server, child, world_transform, forced_alpha);
    }
}

auto bake_scene(const siren::StrongHandle<siren::Gltf>& gltf_handle,
                siren::AssetServer& server,
                const std::optional<siren::f32> forced_alpha) -> BakedScene {
    BakedScene baked{};

    auto& gltf = server.get_unsafe(gltf_handle);

    auto sceneh = gltf.default_scene.value_or(gltf.scenes[0]);
    auto& scene = server.get_unsafe(sceneh);

    for (const auto& nodeh : scene.root_nodes) {
        auto& node = server.get_unsafe(nodeh);
        bake_node(baked, server, node, glm::mat4{ 1 }, forced_alpha);
    }

    siren::log::debug("Loaded scene with {} transparent surfaces, {} opaque surfaces, {} materials",
                      baked.transparent.size(), baked.opaque.size(), baked.materials.size());
    return baked;
}

}  // namespace oiter

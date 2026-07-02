#pragma once
#include "2iren/asset/asset_handle.hpp"
#include "2iren/asset/assets/gltf.hpp"

namespace oiter {

/**
 * @brief A gltf scene loaded into memory.
 */
struct Scene {
    /** @brief A handle to the original gltf asset to avoid dropping the resources. */
    siren::StrongHandle<siren::Gltf> gltf;
};

/**
 * @brief Loads a scene into memory. The reason this is used over the AssetServer directly,
 * is that a Gltf contains asset handles, which in turn require more queries to the asset server.
 * For the purpose of benchmarking various OIT methods, we want to avoid any extraneous work.
 * @todo Implement!
 * @return A loaded scene in memory.
 */
inline auto load_scene(const std::string& path, siren::AssetServer& server) -> Scene {
    auto gltf = server.load<siren::Gltf>(path);
    return Scene{
        .gltf = gltf,
    };
}

} // namespace oiter

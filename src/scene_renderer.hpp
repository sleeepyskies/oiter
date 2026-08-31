#pragma once

#include <memory>
#include <string>

#include <glm/vec2.hpp>

#include "2iREN/asset/gltf.hpp"

#include "bake.hpp"
#include "methods/method_kind.hpp"

namespace siren {
class AssetServer;
class Device;
class Image;
class PerspectiveCamera;
} // namespace siren

namespace oiter {
class OitMethod;

/// @brief Owns a loaded scene and renders it with the currently selected OIT method.
class SceneRenderer {
public:
    SceneRenderer(
        siren::Device& device,
        siren::AssetServer& assets,
        const std::string& scene_path,
        MethodKind method_kind,
        glm::uvec2 extent
    );
    ~SceneRenderer();

    SceneRenderer(const SceneRenderer&)                    = delete;
    auto operator=(const SceneRenderer&) -> SceneRenderer& = delete;

    [[nodiscard]]
    auto render(const siren::PerspectiveCamera& camera) -> const siren::Image&;

    /// @brief Resizes the current OIT method's extent-dependent resources.
    auto resize(glm::uvec2 extent) -> void;

    /// @brief Replaces the current OIT method while preserving the loaded scene.
    auto change_method(MethodKind method_kind) -> void;

    /// @brief Reloads the shaders owned by the current OIT method.
    auto reload_shaders() -> void;

    /// @brief Returns the current OIT method.
    [[nodiscard]] auto method() noexcept -> OitMethod&;
    /// @brief Returns the kind of the current OIT method.
    [[nodiscard]] auto method_kind() const noexcept -> MethodKind;

private:
    siren::Device& m_device;
    siren::AssetServer& m_assets;
    glm::uvec2 m_extent;

    siren::StrongHandle<siren::Gltf> m_scene_asset = siren::NullHandle;
    BakedScene m_scene;

    MethodKind m_method_kind;
    std::unique_ptr<OitMethod> m_method;
};
} // namespace oiter

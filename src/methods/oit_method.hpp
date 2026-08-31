#pragma once

#include "2iREN/graphics/device.hpp"
#include "2iREN/graphics/image.hpp"
#include "2iREN/scene/camera.hpp"

#include "bake.hpp"

namespace oiter {
/// @brief The maximum number of meshes that can be drawn in a frame.
/// @warning oiter will crash for a scene with _more_ meshes than this.
constexpr auto MAX_MESHES = 512;

/// @brief Data shared across the entire rendered scene.
struct alignas(16) SceneUniforms {
    /// @brief Combined projection and view transformation matrix.
    glm::mat4 projection_view;
    /// @brief World space position of the camera.
    glm::vec3 camera_position;
    /// @brief Nothing at all...
    siren::f32 _pad = 0;
};

/// @brief Data specific to a single mesh draw call.
///
/// Contains the object's transform and material information used
/// when rendering an individual mesh instance.
struct alignas(16) MeshUniforms {
    /// @brief Material parameters used when shading the mesh.
    BakedMaterial material;
    /// @brief World transformation matrix of the mesh.
    glm::mat4 model;
};

/// @brief Base class all OIT methods should implement.
class OitMethod {
public:
    /// @brief Creates an OIT rendering method.
    /// @param device The rendering device used for resource creation.
    /// @param assets The asset server used to access loaded resources.
    OitMethod(siren::Device& device, siren::AssetServer& assets);

    virtual ~OitMethod() = default;

    /// @brief The main render function of the OIT method.
    /// @param camera The camera to render the scene from.
    /// @param scene The scene to render.
    /// @return An image of the final rendered scene.
    [[nodiscard]] virtual auto render(
        const siren::PerspectiveCamera& camera, const BakedScene& scene
    ) const -> const siren::Image& = 0;

    /// @brief Initiates a resize of the OIT method. The OIT method should reconstruct all sized
    /// resources.
    /// @param extent The new size.
    virtual auto resize(const glm::uvec2 extent) -> void = 0;

    /// @brief Reloads all shaders.
    virtual auto reload_shaders() -> void = 0;

    /// @brief Returns the name of this method.
    [[nodiscard]] virtual auto name() const noexcept -> std::string_view = 0;

    /// @brief Can be optionally implemented to display custom debug information.
    ///
    /// Implementations of this function should render using ImGui, which will be displayed
    /// on the debug panel. This can be toggled by pressing F1.
    virtual auto render_debug_info() -> void {}

protected:
    /// @brief Cached reference to the @ref Device.
    siren::Device& m_device;

    /// @brief Cached reference to the @ref AssetServer.
    siren::AssetServer& m_assets;

    /// @brief Whether scene buffers need to be updated.
    mutable bool m_scene_updated = true;

    /// @brief Buffer containing scene wide data.
    std::unique_ptr<siren::Buffer> m_scene_buffer;

    /// @brief Buffer containing per mesh data.
    std::unique_ptr<siren::Buffer> m_mesh_buffer;

protected:
    /// @brief Updates the contents of the methods buffers.
    auto update_buffers(const siren::PerspectiveCamera& camera, const BakedScene& scene) const
        -> void;

    /// @brief Returns the alignment size of the MeshUniforms buffer.
    [[nodiscard]] auto mesh_uniforms_alignment() const -> siren::usize {
        return siren::align_up(
            sizeof(MeshUniforms), m_device.limits().uniform_buffer_offset_alignment
        );
    }

    /// @brief Simple helper function to reduce code duplication for creating images. Creates a
    /// basic 2D Image.
    [[nodiscard]] auto create_standard_image(
        const glm::uvec2 extent, const std::string& label, const siren::ImageFormat image_format
    ) const -> std::unique_ptr<siren::Image>;

private:
    /// @brief Creates uniform buffers for the scene. This involves global scene data as well as per
    /// call data.
    auto create_buffers() -> void;
};
} // namespace oiter

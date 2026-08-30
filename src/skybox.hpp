#pragma once

#include <memory>
#include <string_view>

#include "2iREN/asset/mesh.hpp"
#include "2iREN/asset/shader.hpp"
#include "2iREN/graphics/image.hpp"
#include "2iREN/scene/camera.hpp"

namespace oiter {

/// @brief Utility class for rendering a skybox.
class Skybox {
public:
    /// @brief Creates a skybox from the given cube map path.
    /// @param path Path to the skybox cube map asset.
    /// @param device Device used to create skybox resources.
    /// @param server Asset server used to load skybox assets.
    Skybox(const std::string_view path, siren::Device& device, siren::AssetServer& server);

    /// @brief Renders the skybox behind the alpha contents of an image.
    /// @param image Image to render the skybox into.
    /// @param camera Camera used to view the skybox.
    auto render_behind(const siren::Image& image, const siren::PerspectiveCamera& camera) const
        -> void;

private:
    /// @brief Uniform data used to render the skybox.
    struct alignas(16) Uniforms {
        glm::mat4 projection_view;
        glm::vec3 camera_position;
        siren::f32 _pad = 0;
    };

    /// @brief Creates the skybox GPU resources.
    auto create_resources() -> void;

    /// @brief Cached reference to the device.
    siren::Device& m_device;

    /// @brief Cached reference to the asset server.
    siren::AssetServer& m_assets;

    /// @brief Path to the cube map texture.
    std::string m_path;

    /// @brief Uniform buffer containing camera data.
    std::unique_ptr<siren::Buffer> m_uniform_buffer;

    /// @brief Image for the skybox.
    siren::StrongHandle<siren::Texture> m_skybox_texture = siren::NullHandle;

    /// @brief Unit cube mesh.
    siren::StrongHandle<siren::Mesh> m_cube = siren::NullHandle;

    /// @brief Graphics pipeline for rendering the skybox underlay.
    std::unique_ptr<siren::GraphicsPipeline> m_skybox_pipeline;

    /// @brief Shader handle for the skybox.
    siren::StrongHandle<siren::ShaderAsset> m_skybox_shader = siren::NullHandle;
};

} // namespace oiter

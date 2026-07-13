#pragma once

#include "../bake.hpp"
#include "2iren/asset/asset_server.hpp"
#include "2iren/asset/assets/shader.hpp"
#include "2iren/rhi/render_target.hpp"
#include "2iren/rhi/resources/buffer.hpp"
#include "2iren/rhi/resources/graphics_pipeline.hpp"
#include "2iren/util/camera.hpp"
#include "2iren/window.hpp"
#include "oit_method.hpp"

#define MAX_LAYERS 10
#define MAX_MATERIALS 64
#define GPU_ALIGNED alignas(16)

namespace oiter {

struct GPU_ALIGNED SceneData {
    glm::mat4 view_projection;
    glm::vec3 camera_position;
    float _pad = 0;
};

struct GPU_ALIGNED MaterialData {
    std::array<BakedMaterial, MAX_MATERIALS> materials;
};

struct GPU_ALIGNED DrawCallData {
    siren::u32 material_index;
    siren::u32 _pad[3];
};

struct Pipeline {
    siren::StrongHandle<siren::ShaderAsset> shader;
    siren::GraphicsPipeline pipeline;
};

struct Target {
    explicit Target(std::vector<siren::Image>&& images) :
        target(images | std::views::transform(&siren::Image::handle) | std::ranges::to<std::vector>()),
        images(std::move(images)) {}

    siren::RenderTarget target;
    std::vector<siren::Image> images;
};

class DualDepthPeeling final : public OitMethod {
public:
    explicit DualDepthPeeling(siren::Device& device, const siren::Window& window, siren::AssetServer& server);

    [[nodiscard]] auto render(const siren::PerspectiveCamera& camera, const BakedScene& scene) const
            -> const siren::Image& override;

private:
    siren::Device& m_device;
    siren::Image m_final_image;

    struct UniformBuffers {
        siren::Buffer scene_data;
        siren::Buffer material_data;
        siren::Buffer draw_call_data;
    } m_uniforms;

    struct GeometryPass {
        Pipeline pipeline;
        Target target;
    } m_geometry;

    struct InitPass {
        Pipeline pipeline;
    } m_init;

    struct PeelPass {
        Pipeline pipeline;
        Target target0;     // holds front color, back color, depth range
        Target target1;     // holds front color, back color, depth range
        mutable bool flag;  // just a simple flag to toggle between both targets
    } m_peel;

    struct BlendPass {
        Pipeline pipeline;
        Target target;
    } m_blend;

private:
    auto geometry_pass(const BakedScene& scene) const -> void;
    auto init_pass(const BakedScene& scene) const -> void;
    auto peel_pass(const BakedScene& scene) const -> void;
    auto blend_pass() const -> void;

private:
    auto init_uniforms(siren::Device& device) const -> UniformBuffers;
    auto init_geometry_pass(siren::Device& device, siren::AssetServer& server, const siren::Window& window) const
            -> GeometryPass;
    auto init_init_pass(siren::Device& device, siren::AssetServer& server) const -> InitPass;
    auto init_peel_pass(siren::Device& device, siren::AssetServer& server, const siren::Window& window) const
            -> PeelPass;
    auto init_blend_pass(siren::Device& device, siren::AssetServer& server, const siren::Window& window) const
            -> BlendPass;
};

}  // namespace oiter

#pragma once

#include <utility>

#include "../bake.hpp"
#include "2iren/asset/asset_server.hpp"
#include "2iren/asset/assets/shader.hpp"
#include "2iren/rhi/render_target.hpp"
#include "2iren/rhi/resources/buffer.hpp"
#include "2iren/rhi/resources/graphics_pipeline.hpp"
#include "2iren/util/camera.hpp"
#include "2iren/window.hpp"
#include "oit_method.hpp"

constexpr auto MAX_PEELS     = 5;
constexpr auto MAX_MATERIALS = 64;
constexpr auto MAX_DEPTH     = 1;

namespace oiter {

// todo: query for number of fragments left in order to allow for early end

struct alignas(16) SceneData {
    glm::mat4 view_projection;
    glm::vec3 camera_position;
    float _pad = 0;
};

struct alignas(16) MaterialData {
    std::array<BakedMaterial, MAX_MATERIALS> materials;
};

struct alignas(16) DrawCallData {
    siren::u32 material_index;
    siren::u32 _pad[3];
};

struct Pipeline {
    siren::StrongHandle<siren::ShaderAsset> shader;
    siren::GraphicsPipeline graphics_pipeline;
};

struct Target {
    explicit Target(siren::RenderTarget render_target, std::vector<siren::Image>&& images, std::optional<siren::Image>&& depth = std::nullopt) :
        render_target(std::move(render_target)), colors(std::move(images)), depth_stencil(std::move(depth)) {}
    siren::RenderTarget render_target;
    std::vector<siren::Image> colors;
    std::optional<siren::Image> depth_stencil;
};

class DualDepthPeeling final : public OitMethod {
public:
    explicit DualDepthPeeling(siren::Device& device, const siren::Window& window, siren::AssetServer& server);

    [[nodiscard]] auto render(const siren::PerspectiveCamera& camera, const BakedScene& scene) const
            -> const siren::Image& override;

private:
    siren::Device& m_device;
    siren::Sampler m_sampler;

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
        Target target0;     // holds min max, back and front color
        Target target1;     // holds min max, back and front color
        mutable bool flag;  // just a simple flag to toggle between both targets

        auto read_target() const -> const Target& { return flag ? target0 : target1; }
        auto write_target() const -> const Target& { return !flag ? target0 : target1; }
    } m_peel;

    struct BlendPass {
        Pipeline pipeline;
        Target target;
    } m_blend;

    struct FinalPass {
        Pipeline pipeline;
        Target target;
    } m_final;

private:
    auto geometry_pass(const BakedScene& scene) const -> void;
    auto init_pass(const BakedScene& scene) const -> void;
    auto peel_pass(const BakedScene& scene) const -> void;
    auto blend_pass() const -> void;
    auto final_pass() const -> void;

private:
    auto init_uniforms(siren::Device& device) const -> UniformBuffers;
    auto init_sampler(siren::Device& device) const -> siren::Sampler;

    auto init_geometry_pass(siren::Device& device, siren::AssetServer& server, const siren::Window& window) const
            -> GeometryPass;
    auto init_init_pass(siren::Device& device, siren::AssetServer& server) const -> InitPass;
    auto init_peel_pass(siren::Device& device, siren::AssetServer& server, const siren::Window& window) const
            -> PeelPass;
    auto init_blend_pass(siren::Device& device, siren::AssetServer& server, const siren::Window& window) const
            -> BlendPass;
    auto init_final_pass(siren::Device& device, siren::AssetServer& server, const siren::Window& window) const
            -> FinalPass;
};

}  // namespace oiter

#pragma once

#include "../oit_method.hpp"
#include "../util.hpp"
#include "../../bake.hpp"
#include "2iren/asset/asset_server.hpp"

namespace oiter {
class DepthPeeling final : public OitMethod {
public:
    static constexpr auto LAYER_COUNT = 8u;
    static constexpr auto DEPTH_COUNT = 2u;

    DepthPeeling(siren::Device& device, glm::uvec2 extent, siren::AssetServer& assets);

    [[nodiscard]] auto render(
        const siren::PerspectiveCamera& camera,
        const BakedScene& scene
    ) const -> const siren::Image& override;

    auto resize(const glm::uvec2 extent) -> void override;

    [[nodiscard]] auto name() const noexcept -> std::string_view override { return "Depth Peeling"; }

    auto render_debug_info() -> void override;

private:
    std::array<std::unique_ptr<siren::Image>, LAYER_COUNT> m_colors;
    std::array<std::unique_ptr<siren::Image>, DEPTH_COUNT> m_depths;
    std::unique_ptr<siren::Image> m_output;

    std::array<siren::RenderTarget, LAYER_COUNT> m_targets;;
    siren::RenderTarget m_combine_target;

    std::unique_ptr<siren::GraphicsPipeline> m_gather_first_pipeline;
    std::unique_ptr<siren::GraphicsPipeline> m_gather_pipeline;
    std::unique_ptr<siren::GraphicsPipeline> m_combine_pipeline;

    siren::StrongHandle<siren::ShaderAsset> m_gather_first_shader = siren::NullHandle;
    siren::StrongHandle<siren::ShaderAsset> m_gather_shader       = siren::NullHandle;
    siren::StrongHandle<siren::ShaderAsset> m_combine_shader      = siren::NullHandle;

    std::unique_ptr<siren::Sampler> m_sampler;

    auto create_images(const glm::uvec2 extent) -> void;
    auto create_sampler() -> void;
    auto create_render_targets() -> void;
    auto create_pipelines() -> void;
};
} // namespace oiter

#pragma once

#include "../../bake.hpp"
#include "2iREN/asset/asset_server.hpp"
#include "2iREN/util/camera.hpp"
#include "../oit_method.hpp"
#include "2iREN/asset/assets/shader.hpp"

namespace oiter {

class DualDepthPeeling final : public OitMethod {
    /// @brief A bunch of configuration options that can be tweaked during runtime for
    /// the @ref DualDepthPeeling OitMethod. These can be viewed in the F1 debug menu when
    /// the DualDepthPeeling method is active.
    struct Config {
        /// @brief The maximum number of peels to perform.
        siren::i32 max_peels = 8;
        /// @brief Whether to query in order to stop early.
        bool perform_query = false;
    } m_config;

public:
    explicit DualDepthPeeling(
        siren::Device& device,
        const glm::uvec2 extent,
        siren::AssetServer& assets
    );

    [[nodiscard]] auto render(
        const siren::PerspectiveCamera& camera,
        const BakedScene& scene
    ) const -> const siren::Image& override;
    [[nodiscard]] auto name() const noexcept -> std::string_view override { return "Dual Depth Peeling"; }
    auto resize(const glm::uvec2 extent) -> void override;
    auto reload_shaders() -> void override;
    auto render_debug_info() -> void override;

private:
    mutable siren::u32 m_last_frame_peels = 0;
    mutable siren::u32 m_pingpong_index   = 0;

    std::unique_ptr<siren::Query> m_occlusion_query;

    std::unique_ptr<siren::GraphicsPipeline> m_init_pipeline;
    std::unique_ptr<siren::GraphicsPipeline> m_peel_pipeline;
    std::unique_ptr<siren::GraphicsPipeline> m_blend_pipeline;
    std::unique_ptr<siren::GraphicsPipeline> m_final_pipeline;

    siren::StrongHandle<siren::ShaderAsset> m_init_shader  = siren::NullHandle;
    siren::StrongHandle<siren::ShaderAsset> m_peel_shader  = siren::NullHandle;
    siren::StrongHandle<siren::ShaderAsset> m_blend_shader = siren::NullHandle;
    siren::StrongHandle<siren::ShaderAsset> m_final_shader = siren::NullHandle;

    // [0 - 2] are first target, [3 - 5] are second target.
    std::array<std::unique_ptr<siren::Image>, 6> m_pingpong_colors;
    std::unique_ptr<siren::Image> m_blend_image;
    std::unique_ptr<siren::Image> m_final_image;

    std::array<siren::RenderTarget, 2> m_pingpong_targets;
    siren::RenderTarget m_blend_target;
    siren::RenderTarget m_final_target;

    std::unique_ptr<siren::Sampler> m_sampler;

private:
    auto create_sampler() -> void;
    auto create_images(const glm::uvec2 extent) -> void;
    auto create_render_targets() -> void;
    auto create_pipelines() -> void;
    auto create_query() -> void;

    auto read_target() const -> const siren::RenderTarget&;
    auto write_target() const -> const siren::RenderTarget&;
    auto swap_targets() const -> void;
    auto reset_targets() const -> void;
};
} // namespace oiter

#pragma once

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/asset/shader.hpp"
#include "2iREN/scene/camera.hpp"

#include "methods/method_kind.hpp"
#include "methods/oit_method.hpp"
#include "utility/bake.hpp"

namespace oiter {

class DualDepthPeeling final : public OitMethod {
    struct Config {
        using Layer = siren::BoundedU32<1u, 100u, siren::ClampBoundsPolicy>;

        Layer layers          = 8;

        bool occlusion_query = true;

        mutable siren::u32 peels_last_frame = 0;
    } m_config;

public:
    explicit DualDepthPeeling(
        siren::Device& device,
        const siren::Extent2u extent,
        siren::AssetServer& assets
    );

    [[nodiscard]]
    auto render(const siren::Camera& camera, const BakedScene& scene) const
        -> siren::ImageHandle override;
    [[nodiscard]]
    auto name() const noexcept -> std::string_view override {
        return "Dual Depth Peeling";
    }
    [[nodiscard]]
    auto kind() const noexcept -> MethodKind override {
        return MethodKind::DualDepthPeeling;
    }
    auto resize(const siren::Extent2u extent) -> void override;
    auto reload_shaders() -> void override;
    auto render_debug_info() -> void override;

private:
    mutable siren::u32 m_pingpong_index = 0;

    std::array<std::unique_ptr<siren::Query>, 2> m_queries;

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
    auto create_images(siren::Extent2u extent) -> void;
    auto create_render_targets() -> void;
    auto create_pipelines() -> void;
    auto create_queries() -> void;

    auto read_target() const -> const siren::RenderTarget&;
    auto write_target() const -> const siren::RenderTarget&;
    auto swap_targets() const -> void;
    auto reset_targets() const -> void;
};
} // namespace oiter

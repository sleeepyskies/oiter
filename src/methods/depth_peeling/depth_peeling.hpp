#pragma once

#include "2iREN/math/bounded.hpp"
#include "methods/oit_method.hpp"
#include "utility/bake.hpp"

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/asset/shader.hpp"
#include "2iREN/graphics/query.hpp"

namespace oiter {

class DepthPeeling final : public OitMethod {
    struct Config {
        using Layer = siren::BoundedU32<1u, 100u, siren::ClampBoundsPolicy>;

        enum Inspecting : siren::i32 {
            None         = 0,
            WriteTexture = 1,
            DepthTexture = 2,
        } inspecting = None;

        Layer inspected_layer = 1;
        Layer layers          = 8;

        bool occlusion_cull_enabled = true;
    } m_config;

public:
    DepthPeeling(siren::Device& device, siren::Extent2u extent, siren::AssetServer& assets);

    [[nodiscard]]
    auto render(const siren::Camera& camera, const BakedScene& scene) const
        -> const siren::Image& override;

    auto resize(const siren::Extent2u extent) -> void override;
    auto reload_shaders() -> void override;

    [[nodiscard]]
    auto name() const noexcept -> std::string_view override {
        return "Depth Peeling";
    }

    [[nodiscard]]
    auto kind() const noexcept -> MethodKind override {
        return MethodKind::DepthPeeling;
    }

    auto render_debug_info() -> void override;

private:
    std::array<std::unique_ptr<siren::Query>, 2> m_queries;

    std::unique_ptr<siren::Image> m_accumulation_color;
    std::unique_ptr<siren::Image> m_write_color;
    std::array<std::unique_ptr<siren::Image>, 2> m_depths;

    std::unique_ptr<siren::Sampler> m_sampler;

    std::unique_ptr<siren::GraphicsPipeline> m_gather_first_pipeline;
    std::unique_ptr<siren::GraphicsPipeline> m_gather_pipeline;
    std::unique_ptr<siren::GraphicsPipeline> m_blend_pipeline;

    siren::StrongHandle<siren::ShaderAsset> m_gather_first_shader = siren::NullHandle;
    siren::StrongHandle<siren::ShaderAsset> m_gather_shader       = siren::NullHandle;
    siren::StrongHandle<siren::ShaderAsset> m_blend_shader        = siren::NullHandle;

    auto create_images(const siren::Extent2u extent) -> void;
    auto create_sampler() -> void;
    auto create_pipelines() -> void;
    auto create_queries() -> void;
};
} // namespace oiter

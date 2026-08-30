#pragma once

#include "bake.hpp"
#include "methods/oit_method.hpp"

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/asset/shader.hpp"

namespace oiter {
class DepthPeeling final : public OitMethod {
    struct Config {
        siren::u32 layers  = 8;
        bool perform_query = true;
    } m_config;

public:
    DepthPeeling(siren::Device& device, glm::uvec2 extent, siren::AssetServer& assets);

    [[nodiscard]] auto render(const siren::PerspectiveCamera& camera, const BakedScene& scene) const
        -> const siren::Image& override;

    auto resize(const glm::uvec2 extent) -> void override;
    auto reload_shaders() -> void override;

    [[nodiscard]] auto name() const noexcept -> std::string_view override {
        return "Depth Peeling";
    }

    auto render_debug_info() -> void override;

private:
    mutable siren::u32 m_last_frame_peels = 0;
    std::unique_ptr<siren::Query> m_occlusion_query;

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

    auto create_images(const glm::uvec2 extent) -> void;
    auto create_sampler() -> void;
    auto create_pipelines() -> void;
    auto create_query() -> void;
};
} // namespace oiter

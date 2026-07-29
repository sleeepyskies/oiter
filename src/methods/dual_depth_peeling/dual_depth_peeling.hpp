#pragma once

#include "ddpconfig.hpp"
#include "uniforms.hpp"
#include "../../bake.hpp"
#include "2iren/asset/asset_server.hpp"
#include "2iren/util/camera.hpp"
#include "../oit_method.hpp"
#include "passes/blend_pass.hpp"
#include "passes/final_pass.hpp"
#include "passes/init_pass.hpp"
#include "passes/peel_pass.hpp"

namespace oiter {
class DualDepthPeeling final : public OitMethod {
public:
    explicit DualDepthPeeling(siren::Device& device, const glm::uvec2 extent, siren::AssetServer& server);

    [[nodiscard]] auto render(
        const siren::PerspectiveCamera& camera,
        const BakedScene& scene
    ) const -> const siren::Image& override;

    [[nodiscard]] auto name() const noexcept -> std::string_view override { return "Dual Depth Peeling"; }

    auto resize(const glm::uvec2 extent) -> void override;
    auto render_debug_info() -> void override;

private:
    std::shared_ptr<DdpConfig> m_config;
    mutable siren::u32 m_last_frame_peels = 0;
    siren::Device& m_device;
    siren::Sampler m_sampler;

    DualDepthPeelingUniforms m_uniforms;

    InitPass m_init;
    PeelPass m_peel;
    BlendPass m_blend;
    FinalPass m_final;

private:
    auto init_uniforms(siren::Device& device) const -> DualDepthPeelingUniforms;
    auto init_sampler(siren::Device& device) const -> siren::Sampler;
};
} // namespace oiter

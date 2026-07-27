#pragma once

#include "uniforms.hpp"
#include "../../bake.hpp"
#include "2iren/asset/asset_server.hpp"
#include "2iren/rhi/resources/buffer.hpp"
#include "2iren/util/camera.hpp"
#include "../oit_method.hpp"
#include "passes/blend_pass.hpp"
#include "passes/final_pass.hpp"
#include "passes/init_pass.hpp"
#include "passes/peel_pass.hpp"

constexpr auto MAX_PEELS = 5;
constexpr auto MAX_DEPTH = 1;

namespace oiter {
// todo: query for number of fragments left in order to allow for early end


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

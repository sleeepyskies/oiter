#pragma once

#include <glm/vec2.hpp>

#include "../ddpconfig.hpp"
#include "../../util.hpp"
#include "2iren/asset/asset_server.hpp"
#include "2iren/rhi/device.hpp"

namespace oiter {
/**
 * @brief The final pass of the dual depth peeling method. It performs a
 * full screen render pass.
 *
 * Conceptually, the final pass combines the front and back accumulated
 * textures into a single output image.
 *
 * To do this, we must blend the front texture *over* the back layer.
 *
 * To do this, we disable pipeline alpha blending. Instead, we output
 * the final values:
 * - out.rgb = front.rgb + back.rgb * front.alpha
 * - out.a   = 1.f
 */
class FinalPass {
public:
    FinalPass(
        siren::Device& device,
        siren::AssetServer& asset_server,
        const glm::uvec2 extent,
        const std::shared_ptr<DdpConfig>& config
    );

    /**
     * @brief Performs the final pass.
     * @return The output image.
     */
    auto execute(
        const siren::Sampler& sampler,
        const RenderTargetResources& read_target
    ) const -> void;

    /**
     * @brief Recreates any sized resources managed by this pass.
     */
    auto resize(const glm::uvec2 extent) -> void;

    [[nodiscard]] auto image() const -> const siren::Image&;

private:
    siren::Device& m_device;
    siren::AssetServer& m_asset_server;

    GraphicsPipelineResources m_pipeline;
    RenderTargetResources m_target;
    std::shared_ptr<DdpConfig> m_config;
};
} // namespace oiter

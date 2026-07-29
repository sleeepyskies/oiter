#pragma once

#include <glm/vec2.hpp>

#include "../ddpconfig.hpp"
#include "../../util.hpp"
#include "2iren/asset/asset_server.hpp"
#include "2iren/rhi/device.hpp"
#include "2iren/rhi/resources/query.hpp"

namespace oiter {
/**
 * @brief The blend pass of the Dual Depth Peeling OIT method. This handles
 * accumulating layer colors into the back texture. Since for back layers,
 * we find them in reverse order C -> B -> A, we use the following to blend:
 * Cdst = Asrc * Csrc + (1 - Asrc) * Cdst
 *
 * To do this, we simply use hardware blending with:
 * .source_blend_factor = siren::BlendFactor::SourceAlpha,
 * .dest_blend_factor   = siren::BlendFactor::OneMinusSourceAlpha,
 *
 * Furthermore, in this step we can do the occlusion query. If no samples
 * passed the blend pass, we can stop early :D!
 */
class BlendPass {
public:
    BlendPass(
        siren::Device& device,
        siren::AssetServer& server,
        const glm::uvec2& extent,
        const std::shared_ptr<DdpConfig>& config
    );

    /**
     * @brief Performs the blending pass of the dual depth peeling method.
     * @param sampler A shared default sampler to use.
     * @param write_target The write target of the @ref PeelPass.
     * @return Whether to stop early or not. Dependent on the occlusion query param in the config.
     */
    [[nodiscard]] auto execute(
        const siren::Sampler& sampler,
        const RenderTargetResources& write_target
    ) const -> bool;

    /**
     * @brief Reconstructs any sized resources owned by this render pass.
     * @param extent The new size of resources.
     */
    auto resize(const glm::uvec2 extent) -> void;

private:
    siren::Device& m_device;

    GraphicsPipelineResources m_pipeline;
    RenderTargetResources m_target;
    siren::Query m_query;
    std::shared_ptr<DdpConfig> m_config;
};
} // namespace oiter

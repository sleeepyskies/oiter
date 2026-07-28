#pragma once

#include <glm/vec2.hpp>

#include "../config.hpp"
#include "../../util.hpp"
#include "2iren/asset/asset_server.hpp"
#include "2iren/rhi/device.hpp"
#include "2iren/rhi/resources/query.hpp"

namespace oiter {
/**
 * @brief The blend pass of the Dual Depth Peeling OIT method.
 *
 * The blend pass performs a full screen shader pass.
 *
 * The blend pass runs in combination with the @ref PeelPass for a total of n/2 times, where
 * n is the total number of transparent layers in the scene.
 *
 * @todo: finish explanation
 */
class BlendPass {
public:
    BlendPass(
        siren::Device& device,
        siren::AssetServer& server,
        const glm::uvec2& extent,
        const std::shared_ptr<DualDepthPeelingConfig>& config
    );

    /**
     * @brief Performs the blending pass of the dual depth peeling method.
     * @param sampler A shared default sampler to use.
     * @param read_target The read target of the @ref PeelPass.
     * @return Whether to stop early or not. Dependent on the occlusion query param in the config.
     */
    auto execute(
        const siren::Sampler& sampler,
        const RenderTargetResources& read_target
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
    std::shared_ptr<DualDepthPeelingConfig> m_config;
};
} // namespace oiter

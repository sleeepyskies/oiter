#pragma once

#include <glm/vec2.hpp>

#include "../../util.hpp"
#include "2iren/asset/asset_server.hpp"
#include "2iren/rhi/device.hpp"

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
    BlendPass(siren::Device& device, siren::AssetServer& server, const glm::uvec2& extent);

    /**
     * @brief Performs the blending pass of the dual depth peeling method.
     * @param sampler A shared default sampler to use.
     * @param read_target The read target of the @ref PeelPass.
     */
    auto execute(
        const siren::Sampler& sampler,
        const RenderTargetResources& read_target
    ) const -> void;

    /**
     * @brief Reconstructs any sized resources owned by this render pass.
     * @param extent The new size of resources.
     */
    auto resize(const glm::uvec2 extent) -> void;

private:
    siren::Device& m_device;

    GraphicsPipelineResources m_pipeline;
    RenderTargetResources m_target;
};
} // namespace oiter

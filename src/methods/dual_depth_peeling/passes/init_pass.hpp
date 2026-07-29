#pragma once

#include "../ddpconfig.hpp"
#include "../../util.hpp"
#include "../uniforms.hpp"
#include "2iren/asset/asset_server.hpp"
#include "2iren/rhi/device.hpp"

namespace oiter {
/**
 * @brief The init pass of the Dual Depth Peeling OIT technique. The job of
 * this pass is to setup the render targets for the main depth peeling loop.
 *
 * To do this, it will clear the color textures of the Peeling read target to
 * ZERO.
 *
 * Furthermore, it will also initialize the depth buffer to contains the depth
 * of the outer most front and back layers. To do this, the following steps
 * are done:
 * 1. Initially clear the RG32F depth texture to (-1, -1)
 * 2. Loop over all transparent meshes in scene, and for each fragment write
 *  (-depth, depth) to texture.
 * 3. Since we have enabled Blending::Max, the R channel contains the closest
 *  fragment depths, and the G channel contains the farthest
 *
 * Thus we get the outer layers of depth of the transparent items.
 */
class InitPass {
public:
    InitPass(siren::Device& device, siren::AssetServer& server, const std::shared_ptr<DdpConfig>& config);

    /**
     * @brief Performs the init pass of Dual Depth Peeling.
     */
    auto execute(
        const BakedScene& scene,
        const siren::RenderTarget& read_target,
        const DualDepthPeelingUniforms& uniforms,
        const siren::usize ubo_alignment
    ) const -> void;

private:
    siren::Device& m_device;
    GraphicsPipelineResources m_pipeline;
    std::shared_ptr<DdpConfig> m_config;
};
} // namespace oiter

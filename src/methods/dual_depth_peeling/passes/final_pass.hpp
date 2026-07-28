#pragma once

#include <glm/vec2.hpp>

#include "../config.hpp"
#include "../../util.hpp"
#include "2iren/asset/asset_server.hpp"
#include "2iren/rhi/device.hpp"

namespace oiter {
/**
 * @brief The final pass of the dual depth peeling method.
 *
 * The final render pass runs a fullscreen shader using the front and back textures.
 * It acts as a composite pass, by blending the accumulated values in the textures together.
 *
 * It combines the images together using the formula:
 * out_color = front_color + back_color * (1 - alpha)
 */
class FinalPass {
public:
    FinalPass(
        siren::Device& device,
        siren::AssetServer& asset_server,
        const glm::uvec2 extent,
        const std::shared_ptr<DualDepthPeelingConfig>& config
    );

    /**
     * @brief Performs the final pass.
     * @return The output image.
     */
    [[nodiscard]] auto execute(
        const siren::Sampler& sampler,
        const RenderTargetResources& read_target
    ) const -> const siren::Image&;

    /**
     * @brief Recreates any sized resources managed by this pass.
     */
    auto resize(const glm::uvec2 extent) -> void;

private:
    siren::Device& m_device;
    siren::AssetServer& m_asset_server;

    GraphicsPipelineResources m_pipeline;
    RenderTargetResources m_target;
    std::shared_ptr<DualDepthPeelingConfig> m_config;
};
} // namespace oiter

#pragma once

#include <glm/vec2.hpp>

#include "../ddpconfig.hpp"
#include "../../util.hpp"
#include "../../../bake.hpp"
#include "2iren/asset/asset_server.hpp"
#include "../uniforms.hpp"
#include "2iren/rhi/device.hpp"

namespace oiter {
struct PingPongTarget {
    PingPongTarget(RenderTargetResources&& target0, RenderTargetResources&& target1);

    [[nodiscard]] auto read_target() const -> const RenderTargetResources&;
    [[nodiscard]] auto write_target() const -> const RenderTargetResources&;
    auto swap_targets() const -> void;

    std::array<RenderTargetResources, 2> targets;
    mutable siren::usize index = 0;
};

/**
 * @brief The core of the Dual Depth Peeling oit technique.
 *
 * The peel pass makes use of 2 targets that it toggles between. Both targets
 * are identical, and store:
 * 1. An RG32F dual depth buffer.
 * 2. An RGBA8 front color buffer.
 * 3. An RGBA8 back color buffer.
 *
 * The reason we need 2 targets is that each pass needs texture input.
 * Since we cannot read from the same texture we write to safely bc many
 * fragments may overlap pixels.
 *
 * Conceptually, the peeling pass will loop over all transparent fragments of
 * the scene and do:
 * - Init the write textures to:
 *      - Init out_depth = in_depth (pass through read)
 *      - out_front = in_front      (pass through read)
 *      - out_back = ZERO
 * - If the fragments depth is outside the to be peeled range, ignore.
 * - If the fragments depth is inside the range, but *not* on the layer,
 *   write depth to output. Essentially mimics @ref InitPass logic for
 *   setting up the next pass.
 * - If neither of these cases apply, we are on one of the layers. Then we can
 *   shade the fragment. Also, to prevent, we do not write its actual depth
 *   to the write depth buffer.
 * - Furthermore, we then must determine which layer we are on. We then
 *   distinguish:
 *      1. If on the front layer: accumulate the front color
 *      2. If on the back layer:  simply write the layer color to out_back
 *
 * Note that we accumulate the front and back layers differently.
 * For the front texture, since we find layers in order A -> B -> C, we
 * can do under blending. This is given by:
 * front.rgb   += front_alpha * (frag.rgb * frag.alpha);
 * front_alpha *= (1 - frag.alpha);
 *
 * For the back texture, we find layers in reverse order C -> B -> A,
 * so we must do over blending. This is explained in the @ref BlendPass.
 */
class PeelPass {
public:
    PeelPass(
        siren::Device& device,
        siren::AssetServer& server,
        const glm::uvec2& extent,
        const std::shared_ptr<DdpConfig>& config
    );

    auto execute(
        const BakedScene& scene,
        const siren::Sampler& sampler,
        const DualDepthPeelingUniforms& uniforms,
        const siren::usize ubo_alignment
    ) const -> void;

    /**
     * @brief Reconstructs any sized resources owned by this render pass.
     * @param extent The new size of resources.
     */
    auto resize(const glm::uvec2 extent) -> void;

    auto read_target() const -> const RenderTargetResources&;
    auto write_target() const -> const RenderTargetResources&;
    auto swap_targets() const -> void;

    auto read_image() const -> const siren::Image&;
    auto write_image() const -> const siren::Image&;

private:
    siren::Device& m_device;

    PingPongTarget m_target;
    GraphicsPipelineResources m_pipeline;
    std::shared_ptr<DdpConfig> m_config;
};
} // namespace oiter

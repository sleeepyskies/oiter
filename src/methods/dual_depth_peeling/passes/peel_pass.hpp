#pragma once

#include <glm/vec2.hpp>

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

class PeelPass {
public:
    PeelPass(siren::Device& device, siren::AssetServer& server, const glm::uvec2& extent);

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

private:
    siren::Device& m_device;

    PingPongTarget m_target;
    GraphicsPipelineResources m_pipeline;
};
} // namespace oiter

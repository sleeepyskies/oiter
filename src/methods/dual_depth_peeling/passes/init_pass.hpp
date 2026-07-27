#pragma once

#include "../../util.hpp"
#include "../uniforms.hpp"
#include "2iren/asset/asset_server.hpp"
#include "2iren/rhi/device.hpp"

namespace oiter {
class InitPass {
public:
    InitPass(siren::Device& device, siren::AssetServer& server);

    auto execute(
        const BakedScene& scene,
        const siren::RenderTarget& read_target,
        const DualDepthPeelingUniforms& uniforms
    ) const -> void;

private:
    siren::Device& m_device;
    GraphicsPipelineResources m_pipeline;
};
} // namespace oiter

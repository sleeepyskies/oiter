#pragma once

#include "2iren/asset/asset_server.hpp"
#include "2iren/rhi/render_target.hpp"
#include "2iren/rhi/resources/graphics_pipeline.hpp"
#include "2iren/window.hpp"
#include "oit_method.hpp"

namespace oiter {

class DualDepthPeeling final : public OitMethod {
public:
    explicit DualDepthPeeling(siren::Device& device, const siren::Window& window, siren::AssetServer& server);

    [[nodiscard]] auto render(const BakedScene& scene) const -> const siren::Image& override;

private:
    siren::Device& m_device;

    struct OpaquePassData {
        siren::Image target_image;  // needed bc render target doesn't take ownership of the image
        siren::RenderTarget target;
        siren::GraphicsPipeline pipeline;
    } m_opaque;

    siren::Image m_final_image;

private:
    auto perform_opaque_pass(const BakedScene& scene) const -> void;
    auto perform_depth_peeling(const BakedScene& scene) const -> void;
    auto perform_blending() const -> void;

    auto init_opaque_pass(siren::Device& device, const siren::Window& window, siren::AssetServer& server)
            -> OpaquePassData;
    auto init_final_image(siren::Device& device, const siren::Window& window) -> siren::Image;
};

}  // namespace oiter

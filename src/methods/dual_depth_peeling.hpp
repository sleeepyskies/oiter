#pragma once

#include "2iren/asset/asset_server.hpp"
#include "2iren/asset/assets/shader.hpp"
#include "2iren/rhi/render_target.hpp"
#include "2iren/rhi/resources/buffer.hpp"
#include "2iren/rhi/resources/graphics_pipeline.hpp"
#include "2iren/util/camera.hpp"
#include "2iren/window.hpp"
#include "oit_method.hpp"

namespace oiter {

struct UniformBufferData {
    glm::mat4 view_projection;
};

class DualDepthPeeling final : public OitMethod {
public:
    explicit DualDepthPeeling(siren::Device& device, const siren::Window& window, siren::AssetServer& server);

    [[nodiscard]] auto render(const siren::PerspectiveCamera& camera, const BakedScene& scene) const
            -> const siren::Image& override;

private:
    siren::Device& m_device;

    struct OpaquePassData {
        siren::StrongHandle<siren::ShaderAsset> shaderh;
        siren::RenderTarget target;
        siren::Image target_image;  // needed bc render target doesn't take ownership of the image
        siren::GraphicsPipeline pipeline;
    } m_opaque;

    siren::Image m_final_image;
    siren::Buffer m_uniform_buffer;

private:
    auto perform_opaque_pass(const siren::PerspectiveCamera& camera, const BakedScene& scene) const -> void;
    auto perform_depth_peeling(const BakedScene& scene) const -> void;
    auto perform_blending() const -> void;

    auto init_opaque_pass(siren::Device& device, const siren::Window& window, siren::AssetServer& server)
            -> OpaquePassData;
    auto init_final_image(siren::Device& device, const siren::Window& window) -> siren::Image;
    auto init_uniform_buffer(siren::Device& device) -> siren::Buffer;
};

}  // namespace oiter

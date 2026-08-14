#pragma once

#include "../../bake.hpp"
#include "2iren/asset/asset_server.hpp"
#include "2iren/util/camera.hpp"
#include "../oit_method.hpp"
#include "2iren/asset/assets/shader.hpp"

namespace oiter {
/**
 * @section Init Pass
 *
 * The init pass of the Dual Depth Peeling OIT technique. The job of
 * this pass is to set up the render targets for the main depth peeling loop.
 *
 * To do this, it will clear the color textures of the Peeling read target to
 * ZERO.
 *
 * Furthermore, it will also initialize the depth buffer to contains the depth
 * of the outermost front and back layers. To do this, the following steps
 * are done:
 * 1. Initially clear the RG32F depth texture to (-1, -1)
 * 2. Loop over all transparent meshes in scene, and for each fragment write
 *  (-depth, depth) to texture.
 * 3. Since we have enabled Blending::Max, the R channel contains the closest
 *  fragment depths, and the G channel contains the farthest
 *
 * Thus we get the outer layers of depth of the transparent items.
 *
 * @section Peel Pass
 * The core of the Dual Depth Peeling oit technique.
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
 *      - Init out_depth = in_depth      (pass through read)
 *      - out_front      = in_front      (pass through read)
 *      - out_back       = ZERO
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
 *
 * @section Blend Pass
 * The blend pass of the Dual Depth Peeling OIT method. This handles
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
 *
 * @section Final Pass
 * The final pass of the dual depth peeling method. It performs a
 * full screen render pass.
 *
 * Conceptually, the final pass combines the front and back accumulated
 * textures into a single output image.
 *
 * To do this, we must blend the front texture *over* the back layer.
 *
 * To do this, we disable pipeline alpha blending. Instead, we output
 * the final values:
 * - out.rgb = front.rgb + back.rgb * front.alpha
 * - out.a   = 1.f
 */
class DualDepthPeeling final : public OitMethod {
    /**
     * @brief A bunch of configuration options that can be tweaked during runtime for
     * the @ref DualDepthPeeling OitMethod. These can be viewed in the F1 debug menu when
     * the DualDepthPeeling method is active.
     */
    struct Config {
        /** @brief The maximum number of peels to perform. */
        siren::i32 max_peels = 8;
        /** @brief Whether to query in order to stop early. */
        bool perform_query = false;
    } m_config;

public:
    explicit DualDepthPeeling(
        siren::Device& device,
        const glm::uvec2 extent,
        siren::AssetServer& assets
    );

    [[nodiscard]] auto render(
        const siren::PerspectiveCamera& camera,
        const BakedScene& scene
    ) const -> const siren::Image& override;
    [[nodiscard]] auto name() const noexcept -> std::string_view override { return "Dual Depth Peeling"; }
    auto resize(const glm::uvec2 extent) -> void override;
    auto reload_shaders() -> void override;
    auto render_debug_info() -> void override;

private:
    mutable siren::u32 m_last_frame_peels = 0;
    mutable siren::u32 m_pingpong_index   = 0;

    std::unique_ptr<siren::Query> m_occlusion_query;

    std::unique_ptr<siren::GraphicsPipeline> m_init_pipeline;
    std::unique_ptr<siren::GraphicsPipeline> m_peel_pipeline;
    std::unique_ptr<siren::GraphicsPipeline> m_blend_pipeline;
    std::unique_ptr<siren::GraphicsPipeline> m_final_pipeline;

    siren::StrongHandle<siren::ShaderAsset> m_init_shader  = siren::NullHandle;
    siren::StrongHandle<siren::ShaderAsset> m_peel_shader  = siren::NullHandle;
    siren::StrongHandle<siren::ShaderAsset> m_blend_shader = siren::NullHandle;
    siren::StrongHandle<siren::ShaderAsset> m_final_shader = siren::NullHandle;

    // [0 - 2] are first target, [3 - 5] are second target.
    std::array<std::unique_ptr<siren::Image>, 6> m_pingpong_colors;
    std::unique_ptr<siren::Image> m_blend_image;
    std::unique_ptr<siren::Image> m_final_image;

    std::array<siren::RenderTarget, 2> m_pingpong_targets;
    siren::RenderTarget m_blend_target;
    siren::RenderTarget m_final_target;

    std::unique_ptr<siren::Sampler> m_sampler;

private:
    auto create_sampler() -> void;
    auto create_images(const glm::uvec2 extent) -> void;
    auto create_render_targets() -> void;
    auto create_pipelines() -> void;
    auto create_query() -> void;

    auto read_target() const -> const siren::RenderTarget&;
    auto write_target() const -> const siren::RenderTarget&;
    auto swap_targets() const -> void;
    auto reset_targets() const -> void;
};
} // namespace oiter

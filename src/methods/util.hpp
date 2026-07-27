#pragma once

#include <optional>
#include <string>
#include <vector>

#include "2iren/asset/asset_handle.hpp"
#include "2iren/asset/assets/shader.hpp"
#include "2iren/rhi/device.hpp"
#include "2iren/rhi/render_target.hpp"
#include "2iren/rhi/resources/graphics_pipeline.hpp"
#include "2iren/rhi/resources/image.hpp"

namespace oiter {
/**
 * @brief Utility struct that has ownership of all resources used by a @ref siren::GraphicsPipeline.
 */
struct GraphicsPipelineResources {
    /** @brief The shader used by the pipeline. */
    siren::StrongHandle<siren::ShaderAsset> shader;
    /** @brief The actual GraphicsPipeline. */
    siren::GraphicsPipeline graphics_pipeline;

    /**
     * @brief Loads a new @ref GraphicsPipelineResources.
     * @param device The @ref Device to use to create resources.
     * @param server The @ref AssetServer to load assets from.
     * @param path The path to the shader file.
     * @param descriptor Description of how to configure the pipeline.
     * @return A new @ref GraphicsPipelineResources.
     */
    static auto create(
        siren::Device& device,
        siren::AssetServer& server,
        const std::string& path,
        siren::GraphicsPipelineDescriptor& descriptor
    ) -> GraphicsPipelineResources;
};

/**
 * @brief Utility struct that has ownership of all resources used by a @ref siren::RenderTarget.
 */
struct RenderTargetResources {
    /** @brief Constructs a new render target and takes ownership of all resources. */
    explicit RenderTargetResources(
        siren::RenderTarget render_target,
        std::vector<siren::Image>&& images,
        std::optional<siren::Image>&& depth = std::nullopt
    ) : render_target(std::move(render_target)), colors(std::move(images)), depth_stencil(std::move(depth)) {}

    /** @brief The actual @ref siren::RenderTarget. */
    siren::RenderTarget render_target;
    /** @brief All color images to ensure lifetime. */
    std::vector<siren::Image> colors;
    /** @brief Optional depth + stencil image to ensure lifetime. */
    std::optional<siren::Image> depth_stencil;
};

/**
 * @brief Utility class to help build a @ref RenderTargetResources
 */
class RenderTargetBuilder {
public:
    /**
     * @ref Creates a new RenderTargetBuilder.
     * @param device The device to use for creating resources.
     * @param extent The size of the render target.
     * @param label An optional label. If provided, all images created will be labeled based on this.
     */
    static auto create(
        siren::Device& device,
        const glm::uvec2& extent,
        const std::optional<std::string>& label
    ) -> RenderTargetBuilder;

    /**
     * @brief Adds a new color image to the render target.
     * @param format The desired image format.
     * @param begin_op Operation to perform on load.
     * @param clear What color to clear to, only relevant if begin_op == Clear.
     * @return A reference to this @ref RenderTargetBuilder.
     */
    auto add_color(
        siren::ImageFormat format,
        siren::BeginOperation begin_op,
        siren::Rgba clear
    ) -> RenderTargetBuilder&;

    /**
     * @brief Adds a new depth + stencil image to the render target.
     * @warning If called multiple times, old data will be overwritten and resources dropped.
     * @param format The desired image format.
     * @param begin_op Operation to perform on load.
     * @param clear_depth What depth value to clear to, only relevant if begin_op == Clear.
     * @param clear_stencil What stencil value to clear to, only relevant if begin_op == Clear.
     * @return A reference to this @ref RenderTargetBuilder.
     */
    auto add_depth_stencil(
        siren::ImageFormat format,
        siren::BeginOperation begin_op,
        siren::f32 clear_depth = 1.f,
        siren::u8 clear_stencil = 0
    ) -> RenderTargetBuilder&;

    /**
     * @brief Builds the final render target.
     * @return A render target owning all of its resources.
     */
    auto build() -> RenderTargetResources;

private:
    RenderTargetBuilder(
        siren::Device& device,
        const glm::uvec2 extent,
        const std::optional<std::string>& label
    ) : m_device(device), m_label(label), m_extent(extent) {}

    siren::Device& m_device;
    std::optional<std::string> m_label;
    glm::uvec2 m_extent;
    std::vector<siren::Image> m_colors;
    std::vector<siren::ColorAttachment> m_color_attachments;
    std::optional<siren::Image> m_depth;
    std::optional<siren::DepthStencilAttachment> m_depth_attachment;
};
} // namespace oiter

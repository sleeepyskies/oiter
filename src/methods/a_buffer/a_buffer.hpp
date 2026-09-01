#pragma once

#include "2iREN/asset/shader.hpp"

#include "methods/oit_method.hpp"

namespace oiter {
/// @brief A node of the linked list.
/// @note This isn't used CPU side, we just have this here for reference.
///
/// The actual SSBO object we use looks something like:
/// struct List {
///     u32 counter;
///     Node[] nodes;
/// }
struct alignas(16) ABufferNode {
    /// @brief The color of the node fragment.
    siren::Rgba color;
    /// @brief The depth of the node fragment.
    siren::f32 depth;
    /// @brief The index of the next node in the list.
    siren::u32 next;
};

class ABuffer final : public OitMethod {
    struct Config {
        // nothing yet :D
    } m_config;

public:
    ABuffer(siren::Device& device, glm::uvec2 extent, siren::AssetServer& assets);

    [[nodiscard]] auto render(const siren::PerspectiveCamera& camera, const BakedScene& scene) const
        -> const siren::Image& override;

    auto resize(const glm::uvec2 extent) -> void override;
    auto reload_shaders() -> void override;

    [[nodiscard]]
    auto name() const noexcept -> std::string_view override {
        return "A-Buffer";
    }

    [[nodiscard]]
    auto kind() const noexcept -> MethodKind override {
        return MethodKind::ABuffer;
    }

    auto render_debug_info() -> void override;

private:
    // todo: put into config?
    static constexpr siren::u8 k_list_length = 8;

    // should store per pixel, its corresponding entry in the SSBO
    std::unique_ptr<siren::Image> m_list_head = nullptr;
    std::unique_ptr<siren::Image> m_output    = nullptr;

    std::unique_ptr<siren::Buffer> m_ssbo = nullptr;

    siren::StrongHandle<siren::ShaderAsset> m_gather_shader = siren::NullHandle;
    siren::StrongHandle<siren::ShaderAsset> m_blend_shader  = siren::NullHandle;

    std::unique_ptr<siren::GraphicsPipeline> m_gather_pipeline = nullptr;
    std::unique_ptr<siren::GraphicsPipeline> m_blend_pipeline  = nullptr;

    auto create_buffers(const glm::uvec2 extent) -> void;
    auto create_images(const glm::uvec2 extent) -> void;
    auto create_pipelines() -> void;
};
} // namespace oiter

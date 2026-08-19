#pragma once

#include "../oit_method.hpp"

namespace oiter {
struct alignas(16) ABufferNode {
    siren::Rgba color;
    siren::f32 depth;
    siren::u32 next;
};

class ABuffer final : public OitMethod {
    struct Config {
        // nothing yet :D
    } m_config;

public:
    ABuffer(siren::Device& device, glm::uvec2 extent, siren::AssetServer& assets);

    [[nodiscard]] auto render(
        const siren::PerspectiveCamera& camera,
        const BakedScene& scene
    ) const -> const siren::Image& override;

    auto resize(const glm::uvec2 extent) -> void override;
    auto reload_shaders() -> void override;

    [[nodiscard]] auto name() const noexcept -> std::string_view override { return "A-Buffer"; }

    auto render_debug_info() -> void override;

private:
    // todo: put into config?
    static constexpr siren::u8 k_list_length = 8;

    // should store per pixel, its corresponding entry in the SSBO
    std::unique_ptr<siren::Image> m_list_head = nullptr;
    std::unique_ptr<siren::Image> m_output    = nullptr;

    std::unique_ptr<siren::Buffer> m_ssbo = nullptr;

    siren::StrongHandle<siren::ShaderAsset> m_gather_shader  = siren::NullHandle;
    siren::StrongHandle<siren::ShaderAsset> m_combine_shader = siren::NullHandle;

    std::unique_ptr<siren::GraphicsPipeline> m_gather_pipeline  = nullptr;
    std::unique_ptr<siren::GraphicsPipeline> m_combine_pipeline = nullptr;

    auto create_buffers(const glm::uvec2 extent) -> void;
    auto create_images(const glm::uvec2 extent) -> void;
    auto create_pipelines() -> void;
};
} // namespace oiter

#pragma once

#include "../oit_method.hpp"

namespace oiter {
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
};
} // namespace oiter

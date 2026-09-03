#pragma once

#include "2iREN/math/extent.hpp"
#include "methods/oit_method.hpp"

namespace oiter {
class KBuffer final : public OitMethod {
    struct Config {
        // nothing yet :D
    } m_config;

public:
    KBuffer(siren::Device& device, siren::Extent2u extent, siren::AssetServer& assets);

    [[nodiscard]]
    auto render(const siren::Camera& camera, const BakedScene& scene) const
        -> const siren::Image& override;

    auto resize(const siren::Extent2u extent) -> void override;
    auto reload_shaders() -> void override;

    [[nodiscard]]
    auto name() const noexcept -> std::string_view override {
        return "K-Buffer";
    }

    [[nodiscard]]
    auto kind() const noexcept -> MethodKind override {
        return MethodKind::KBuffer;
    }

    auto render_debug_info() -> void override;

private:
};
} // namespace oiter

#pragma once

#include "methods/oit_method.hpp"

namespace oiter {

class ScreenDoor final : public OitMethod {
public:
    struct Config {
        siren::f32 threshold = 0.5;
    } m_config;

    explicit ScreenDoor(
        siren::Device& device,
        const siren::Extent2u extent,
        siren::AssetServer& assets
    );

    [[nodiscard]]
    auto render(const siren::Camera& camera, const BakedScene& scene) const
        -> const siren::Image& override;

    auto resize(const siren::Extent2u extent) -> void override;

    auto reload_shaders() -> void override;

    [[nodiscard]]
    auto name() const noexcept -> std::string_view override {
        return "Screen Door";
    }

    [[nodiscard]]
    auto kind() const noexcept -> MethodKind override {
        return MethodKind::ScreenDoor;
    }

private:
    auto create_images(siren::Extent2u extent) -> void;
    auto create_shaders() -> void;
};

} // namespace oiter

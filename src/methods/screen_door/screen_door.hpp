#pragma once

#include <memory>
#include "2iREN/asset/shader.hpp"
#include "2iREN/graphics/graphics_pipeline.hpp"
#include "2iREN/graphics/image.hpp"
#include "2iREN/utility/identifier.hpp"
#include "methods/oit_method.hpp"

namespace oiter {

struct alignas(16) ScreenDoorUniforms {
    siren::u32 size; // see ScreenDoor::Config::GridSize
};

class ScreenDoor final : public OitMethod {
public:
    struct Config {
        enum GridSize {
            TwoXTwo     = 0,
            FourXFour   = 1,
            EightXEight = 2,
        } gridsize = TwoXTwo;
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

    auto render_debug_info() -> void override;

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
    auto create_buffer() -> void;

    std::unique_ptr<siren::GraphicsPipeline> m_screendoor_pipeline = nullptr;
    siren::StrongHandle<siren::ShaderAsset> m_screendoor_shader    = siren::NullHandle;

    std::unique_ptr<siren::Image> m_output = nullptr;
    std::unique_ptr<siren::Image> m_depth  = nullptr;

    std::unique_ptr<siren::Buffer> m_ubo = nullptr;
};

} // namespace oiter

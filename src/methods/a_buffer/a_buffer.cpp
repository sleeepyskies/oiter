#include "a_buffer.hpp"

namespace oiter {
ABuffer::ABuffer(
    siren::Device& device,
    glm::uvec2 extent,
    siren::AssetServer& assets
) : OitMethod(device, assets) {}

auto ABuffer::render(
    const siren::PerspectiveCamera& camera,
    const BakedScene& scene
) const -> const siren::Image& {
    UNIMPLEMENTED();
}

auto ABuffer::resize(const glm::uvec2 extent) -> void {
    UNIMPLEMENTED();
}

auto ABuffer::reload_shaders() -> void {
    UNIMPLEMENTED();
}

auto ABuffer::render_debug_info() -> void {
    UNIMPLEMENTED();
}
} // namespace oiter

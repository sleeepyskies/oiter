#include "k_buffer.hpp"

namespace oiter {
KBuffer::KBuffer(
    siren::Device& device,
    glm::uvec2 extent,
    siren::AssetServer& assets
) : OitMethod(device, assets) {
    /*
    create_images(extent);
    create_sampler();
    create_pipelines();
*/
}

auto KBuffer::render(
    const siren::PerspectiveCamera& camera,
    const BakedScene& scene
) const -> const siren::Image& {
    UNIMPLEMENTED();
}

auto KBuffer::resize(const glm::uvec2 extent) -> void {
    UNIMPLEMENTED();
}

auto KBuffer::reload_shaders() -> void {
    UNIMPLEMENTED();
}

auto KBuffer::render_debug_info() -> void {
    UNIMPLEMENTED();
}
} // namespace oiter

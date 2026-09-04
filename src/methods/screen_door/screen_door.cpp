#include "screen_door.hpp"

#include "2iREN/math/extent.hpp"
#include "methods/oit_method.hpp"

namespace oiter {

ScreenDoor::ScreenDoor(
    siren::Device& device,
    const siren::Extent2u extent,
    siren::AssetServer& assets
) : OitMethod(device, assets) {
    create_images(extent);
    create_shaders();
}

auto ScreenDoor::render(const siren::Camera& camera, const BakedScene& scene) const
    -> const siren::Image& {
    PANIC("not implemented.");
}

auto ScreenDoor::resize(const siren::Extent2u extent) -> void {
    create_images(extent);
}

auto ScreenDoor::reload_shaders() -> void {
    create_shaders();
}

auto ScreenDoor::create_images(const siren::Extent2u extent) -> void { }

auto ScreenDoor::create_shaders() -> void { }

} // namespace oiter

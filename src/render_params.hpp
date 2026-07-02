#pragma once
#include <memory>

#include "2iren/asset/assets/gltf.hpp"
#include "util/load_scene.hpp"

namespace siren {
class Device;
}

namespace oiter {

struct RenderParams {
    std::unique_ptr<siren::Device> device;
    Scene scene;
};

} // namespace oiter

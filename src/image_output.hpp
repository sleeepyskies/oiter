#pragma once

#include <string>

namespace siren {
class AssetServer;
class Device;
class Image;
} // namespace siren

namespace oiter {
/// @brief Converts a linear premultiplied render result to sRGB and writes it as a PNG.
auto write_rendered_image(
    siren::Device& device,
    siren::AssetServer& assets,
    const siren::Image& source,
    const std::string& output_path
) -> void;
} // namespace oiter

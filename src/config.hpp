#pragma once

#include <lyra/lyra.hpp>
#include <string>

namespace oiter {
struct MethodKind {
    enum Value {
        DualDepthPeeling,
        DepthPeeling,
    } value;

    // ReSharper disable once CppNonExplicitConvertingConstructor
    constexpr MethodKind(const Value v) : value(v) {}
    // ReSharper disable once CppNonExplicitConversionOperator
    constexpr operator Value() const { return value; }

    /** @brief Stringifies the given MethodKind. */
    [[nodiscard]] constexpr auto to_string() const -> std::string {
        switch (value) {
            case DualDepthPeeling: return "DualDepthPeeling";
            case DepthPeeling: return "DepthPeeling";
            default: UNREACHABLE();
        };
    }

    /** @brief Factory method to create a new MethodKind from a string input. */
    static auto from_string(const std::string_view str) -> MethodKind {
        if (str == "ddp" || str == "DualDepthPeeling") {
            return DualDepthPeeling;
        }
        if (str == "dp" || str == "DepthPeeling") {
            return DepthPeeling;
        }
        throw std::invalid_argument("Invalid OIT method");
    }
};


struct Config {
    std::string scene_path    = "oiter://assets/meshes/stresstest.glb";
    MethodKind oit_method     = MethodKind::DualDepthPeeling;
    glm::vec3 camera_position = glm::vec3(0.0f);
};

inline auto parse_cli_args(int argc, const char** argv) -> Config {
    bool show_help = false;
    Config config;

    std::string method = config.oit_method.to_string();
    std::string camera_position;

    const auto cli =
        lyra::help(show_help).description("oiter is a simple showcase of various oit methods.") |
        lyra::opt(config.scene_path, "scene")["--scene"]["-s"]("Path to scene file.") |
        lyra::opt(method, "method")["--method"]["-m"]("Choice of OIT method.").choices("ddp", "dp") |
        lyra::opt(camera_position, "position")["--camera-position"]("Camera position (x,y,z).");

    const auto result = cli.parse({argc, argv});

    if (!result) {
        throw std::runtime_error(result.message());
    }

    if (!camera_position.empty()) {
        std::stringstream ss(camera_position);
        char comma;

        if (!(ss >> config.camera_position.x >> comma
                >> config.camera_position.y >> comma
                >> config.camera_position.z) ||
            comma != ',') {
            throw std::runtime_error(
                "Invalid --camera-position. Expected x,y,z"
            );
        }
    }

    return config;
}
} // namespace oiter

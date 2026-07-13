#pragma once

#include <lyra/lyra.hpp>
#include <string>

namespace oiter {

namespace methods {
constexpr auto DUAL_DEPTH_PEELING = "ddp";
constexpr auto A_BUFFER           = "ab";
} // namespace methods

struct CliConfig {
    std::string scene_path = "oiter://assets/meshes/basic-oit-no-monkey.glb";
    std::string oit_method = methods::DUAL_DEPTH_PEELING;
};

inline auto parse_cli_args(int argc, const char** argv) -> CliConfig {
    bool show_help = false;
    CliConfig config;

    const auto cli = lyra::help(show_help).description("oiter is a simple showcase of various oit methods.") |
            lyra::opt(config.scene_path, "scene")["--scene"]["-s"]("Path to scene file.") |
            lyra::opt(config.oit_method, "method")["--method"]["-m"]("Choice of OIT method.")
                    .choices(methods::DUAL_DEPTH_PEELING, methods::A_BUFFER);

    if (const auto parse_result = cli.parse({ argc, argv }); !parse_result) {
        std::cerr << parse_result.message() << "\n\n";
        std::cout << cli;
        std::exit(1);
    }

    if (show_help) {
        std::cout << cli;
        std::exit(0);
    }

    return config;
}

} // namespace oiter

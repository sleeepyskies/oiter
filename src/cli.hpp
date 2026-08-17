#pragma once

#include <lyra/lyra.hpp>

#include "app.hpp"
#include "2iren/base.hpp"

#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <glm/vec2.hpp>

namespace oiter {
/**
 * @brief Command line arguments used for running in interactive mode.
 */
struct InteractiveOptions {
    /** @brief Core application configuration. */
    AppConfig app;
};

/**
 * @brief Command line arguments used rendering an image.
 * @todo This isn't actually supported yet.
 */
struct RenderOptions {
    /** @brief Core application configuration. */
    AppConfig app;
    /** @brief Output path of the image. */
    std::string output_path;
    /** @brief Size of the image to render. */
    glm::uvec2 image_size = {1920, 1080};
};

/**
 * @brief All possible cli options for different launch modes.
 */
using CommandOptions = std::variant<InteractiveOptions, RenderOptions>;

/**
 * @brief Parses a string of format "x,y,z" into a glm::vec3. Will throw on failure D:
 */
[[nodiscard]] inline auto parse_camera_position(const std::string& text) -> glm::vec3 {
    // some weirdness sometimes using siren::f32 over float
    auto stream = std::stringstream{text};
    float x, y, z;
    char comma;

    if (!(stream >> x >> comma) || comma != ',' ||
        !(stream >> y >> comma) || comma != ',' ||
        !(stream >> z)) {
        throw std::runtime_error("Invalid --camera-position. Expected format x,y,z");
    }

    stream >> std::ws;
    if (!stream.eof()) {
        throw std::runtime_error("Invalid --camera-position. Expected format x,y,z");
    }

    return glm::vec3{x, y, z};
}

/**
 * @brief Parses the cli arguments. Currently there can be 2 launch modes, either interactive or render image mode.
 * @param argc The number of arguments.
 * @param argv The actual cli arguments.
 * @return The parse cli arguments.
 */
[[nodiscard]] inline auto parse_cli(
    const siren::i32 argc,
    const char** argv
) -> std::optional<CommandOptions> {
    enum class SelectedCommand {
        Interactive,
        Render,
    } selected_command = SelectedCommand::Interactive;

    bool show_help = false;
    InteractiveOptions interactive;
    RenderOptions render;

    std::string interactive_method = "ddp";
    std::string interactive_camera_position;

    std::string render_method = "ddp";
    std::string render_camera_position;

    auto interactive_command = lyra::command(
        "interactive",
        [&](const lyra::group&) { selected_command = SelectedCommand::Interactive; }
    ).help("Opens the interactive demo.");
    interactive_command.add_argument(
        lyra::opt(interactive.app.scene_path, "scene")["--scene"]["-s"]("Path to scene file.")
    );
    interactive_command.add_argument(
        lyra::opt(interactive_method, "method")["--method"]["-m"]("OIT method.").choices("ddp", "dp")
    );
    interactive_command.add_argument(
        lyra::opt(interactive_camera_position, "position")["--camera-position"]("Camera position (x,y,z).")
    );

    auto render_command = lyra::command(
        "render",
        [&](const lyra::group&) { selected_command = SelectedCommand::Render; }
    ).help("Renders a single image.");
    render_command.add_argument(
        lyra::opt(render.app.scene_path, "scene")["--scene"]["-s"]("Path to scene file to render")
    );
    render_command.add_argument(
        lyra::opt(render_method, "method")["--method"]["-m"]("OIT method.").choices("ddp", "dp", "ab", "kb")
    );
    render_command.add_argument(
        lyra::opt(render_camera_position, "position")["--camera-position"]("Camera position in format (x,y,z).")
    );
    render_command.add_argument(
        lyra::opt(render.output_path, "path")["--output"]["-o"]("Output image path.")
    );
    render_command.add_argument(lyra::opt(render.image_size.x, "width")["--width"]("Width in pixels of output image."));
    render_command.add_argument(
        lyra::opt(render.image_size.y, "height")["--height"]("Height in pixels of output image.")
    );

    lyra::group commands;
    commands.require(1, 1); // at least and at most one command is required
    commands.add_argument(interactive_command);
    commands.add_argument(render_command);

    auto cli = lyra::cli();
    cli.add_argument(lyra::help(show_help).description("OIT method renderer."));
    cli.add_argument(commands);

    const auto result = cli.parse({argc, argv});

    if (!result) {
        throw std::runtime_error(result.message());
    }

    if (show_help) {
        std::cout << cli << '\n';
        return std::nullopt;
    }

    if (selected_command == SelectedCommand::Interactive) {
        interactive.app.oit_method = MethodKind::from_string(interactive_method);
        if (!interactive_camera_position.empty()) {
            interactive.app.camera_position = parse_camera_position(interactive_camera_position);
        }
        return std::move(interactive);
    }

    if (selected_command == SelectedCommand::Render) {
        render.app.oit_method = MethodKind::from_string(render_method);
        if (!render_camera_position.empty()) {
            render.app.camera_position = parse_camera_position(render_camera_position);
        }
        return std::move(render);
    }

    return std::nullopt;
}
} // namespace oiter

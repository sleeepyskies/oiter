#include "cli.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "2iREN/math/point.hpp"

namespace {

[[nodiscard]]
auto parse_point3f(const std::string& text) -> siren::Point3f {
    std::stringstream stream{text};

    siren::f32 x      = 0.f;
    siren::f32 y      = 0.f;
    siren::f32 z      = 0.f;
    char first_comma  = '\0';
    char second_comma = '\0';

    if (!(stream >> x >> first_comma >> y >> second_comma >> z)
        || first_comma
        != ','
        || second_comma
        != ',') {
        throw std::runtime_error("Invalid vec3 '" + text + "'. Expected x,y,z.");
    }

    stream >> std::ws;

    if (!stream.eof()) {
        throw std::runtime_error("Invalid vec3 '" + text + "'. Expected x,y,z.");
    }

    return siren::Point3f{x, y, z};
}

[[nodiscard]]
auto bind_point3f(siren::Point3f& destination) {
    return [destination = &destination](const std::string& text) -> lyra::parser_result {
        try {
            *destination = parse_point3f(text);

            return lyra::parser_result::ok(lyra::parser_result_type::matched);
        } catch (const std::exception& error) {
            return lyra::parser_result::error(lyra::parser_result_type::no_match, error.what());
        }
    };
}

[[nodiscard]]
auto bind_method(oiter::MethodKind& destination) {
    return [destination = &destination](const std::string& text) -> lyra::parser_result {
        try {
            *destination = oiter::MethodKind::from_string(text);

            return lyra::parser_result::ok(lyra::parser_result_type::matched);
        } catch (const std::exception& error) {
            return lyra::parser_result::error(lyra::parser_result_type::no_match, error.what());
        }
    };
}

[[nodiscard]]
auto bind_loglevel(siren::log::Level& destination) {
    return [destination = &destination](const std::string& text) -> lyra::parser_result {
        try {
            *destination = *siren::log::Level::from_string(text);

            return lyra::parser_result::ok(lyra::parser_result_type::matched);
        } catch (const std::exception& error) {
            return lyra::parser_result::error(lyra::parser_result_type::no_match, error.what());
        }
    };
}

} // namespace

namespace oiter {
Command::Command(std::string name, std::string description) :
    m_name(std::move(name)), m_description(std::move(description)) {}

auto Command::selected() const noexcept -> bool { return m_selected; }

auto Command::create_base_parser() -> lyra::command {
    return lyra::command{
        m_name,
        [this](const lyra::group&) { m_selected = true; },
    }
        .help(m_description);
}

InteractiveCommand::InteractiveCommand() :
    Command{
        "interactive",
        "Runs Oiter in interactive mode.",
    } {}

auto InteractiveCommand::create_parser() -> lyra::command {
    auto command = create_base_parser();

    command.add_argument(
        lyra::opt(m_options.scene_path, "path")["-s"]["--scene"].help("Path to the scene file.")
    );

    command.add_argument(
        lyra::opt(bind_method(m_options.method), "method")["-m"]["--method"]
            .choices("ddp", "dp", "ab", "kb")
            .help("OIT method: ddp, dp, ab, kb.")
    );

    command.add_argument(
        lyra::opt(bind_point3f(m_options.camera_position), "x,y,z")["--camera-position"].help(
            "Camera position in x,y,z format."
        )
    );

    command.add_argument(
        lyra::opt(bind_point3f(m_options.camera_lookat), "x,y,z")["--camera-lookat"].help(
            "Camera lookat in x,y,z format."
        )
    );

    command.add_argument(
        lyra::opt(bind_loglevel(m_options.log_level), "loglevel")["--loglevel"]
            .choices("trace", "debug", "info", "warn", "error", "none")
            .help("The logging output level.")
    );

    return command;
}

auto InteractiveCommand::run() -> void {
    InteractiveApp app{m_options};
    app.run();
}

RenderCommand::RenderCommand() :
    Command{
        "render",
        "Renders a single image.",
    } {}

auto RenderCommand::create_parser() -> lyra::command {
    auto command = create_base_parser();

    command.add_argument(
        lyra::opt(m_options.scene_path, "path")["-s"]["--scene"].help("Path to the scene file.")
    );

    command.add_argument(
        lyra::opt(bind_method(m_options.method), "method")["-m"]["--method"]
            .choices("ddp", "dp", "ab", "kb")
            .help("OIT method: ddp, dp, ab, kb.")
            .required()
    );

    command.add_argument(
        lyra::opt(bind_point3f(m_options.camera_position), "x,y,z")["--camera-position"].help(
            "Camera position in x,y,z format."
        )
    );

    command.add_argument(
        lyra::opt(bind_point3f(m_options.camera_lookat), "x,y,z")["--camera-lookat"].help(
            "Camera lookat in x,y,z format."
        )
    );

    command.add_argument(
        lyra::opt(m_options.output_path, "path")["-o"]["--output"]
            .help("Output image path.")
            .required()
    );

    command.add_argument(
        lyra::opt(m_options.dimensions.x, "pixels")["--width"].help("Output image width.")
    );

    command.add_argument(
        lyra::opt(m_options.dimensions.y, "pixels")["--height"].help("Output image height.")
    );

    command.add_argument(
        lyra::opt(bind_loglevel(m_options.log_level), "loglevel")["--loglevel"]
            .choices("trace", "debug", "info", "warn", "error", "none")
            .help("The logging output level.")
    );

    return command;
}

auto RenderCommand::run() -> void {
    RenderApp app{m_options};
    app.run();
}

auto Cli::parse(const int argc, const char** argv) -> std::unique_ptr<Command> {
    bool show_help = false;

    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<InteractiveCommand>());
    commands.push_back(std::make_unique<RenderCommand>());

    lyra::group command_parsers;
    command_parsers.require(1, 1);

    for (auto& command : commands) {
        command_parsers.add_argument(command->create_parser());
    }

    lyra::cli cli;

    cli.add_argument(lyra::help(show_help).description("OIT renderer."));

    cli.add_argument(command_parsers);

    const auto result = cli.parse({argc, argv});

    if (show_help) {
        std::cout << cli << '\n';
        return nullptr;
    }

    if (!result) {
        throw std::runtime_error(result.message());
    }

    const auto selected =
        std::ranges::find_if(commands, [](const auto& command) { return command->selected(); });

    if (selected == commands.end()) {
        throw std::logic_error("Parsing succeeded without selecting a command.");
    }

    return std::move(*selected);
}
} // namespace oiter

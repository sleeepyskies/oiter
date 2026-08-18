#include "app.hpp"
#include "cli.hpp"

#include <type_traits>
#include <variant>

auto main(const int argc, const char** argv) -> siren::i32 {
    const auto command = oiter::parse_cli(argc, argv);

    if (!command) {
        return 0;
    }

    std::visit(
        []<typename Command>(const Command& options) {
            oiter::App app{options.app_options};
            if constexpr (std::is_same_v<Command, oiter::InteractiveOptions>) {
                app.run_interactive();
            } else if constexpr (std::is_same_v<Command, oiter::RenderOptions>) {
                app.run_render(/* todo: pass in render options here! */);
            }
        },
        *command
    );
}

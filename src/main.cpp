#include "app.hpp"

#include <iostream>

#include "cli.hpp"

auto main(const int argc, const char** argv) -> int {
    try {
        const auto command = oiter::Cli::parse(argc, argv);
        if (command) {
            command->run();
        }
    } catch (std::exception& exception) {
        std::cout << exception.what() << std::endl;
    }
}

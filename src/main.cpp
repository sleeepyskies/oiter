#include <iostream>

#include "cli.hpp"

auto main(const int argc, const char** argv) -> int {
    try {
        if (const auto command = oiter::Cli::parse(argc, argv)) {
            command->run();
        }
    } catch (std::exception& exception) {
        std::cout << exception.what() << std::endl;
    }
}

#include "cli_parser.h"

auto main(const int argc, const char** argv) -> int {
    const auto config = oiter::load_cli_config(argc, argv);
    return 0;
}

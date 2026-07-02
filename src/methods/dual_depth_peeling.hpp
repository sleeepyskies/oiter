#pragma once
#include "../render_params.hpp"

namespace oiter {

class DualDepthPeeling final {
public:
    auto render(RenderParams& params) -> void;
};

} // namespace oiter


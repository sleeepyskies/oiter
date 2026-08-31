#pragma once

#include "2iREN/base.hpp"

namespace oiter {
struct FrameStats {
    siren::u32 full_frame_ms = 0;
    siren::u32 oit_render_ms = 0;
    siren::u64 frame         = 0;
    siren::f32 fps           = 0.f;
};
} // namespace oiter

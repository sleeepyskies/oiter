#pragma once
#include <string_view>

#include "2iren/util/log.hpp"

namespace oiter {
static constexpr auto USE_LOG_TIMER = false;

class LogTimer {
    using Clock = std::chrono::high_resolution_clock;

public:
    explicit LogTimer(
        const std::string_view name
    ) : m_name(name) {
        if constexpr (USE_LOG_TIMER) {
            m_start = Clock::now();
        }
    }


    ~LogTimer() {
        if constexpr (USE_LOG_TIMER) {
            const auto duration = std::chrono::duration<double, std::milli>(Clock::now() - m_start).count();
            siren::log::debug("{} took {:.3f}ms", m_name, duration);
        }
    }

private:
    std::string_view m_name;
    Clock::time_point m_start;
};
} // namespace oiter

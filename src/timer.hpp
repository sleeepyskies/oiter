#pragma once
#include <string_view>

#include "2iren/util/log.hpp"

namespace oiter {
class LogTimer {
    using Clock = std::chrono::high_resolution_clock;

public:
    explicit LogTimer(
        const std::string_view name
    ) : m_name(name) {
        m_start = Clock::now();
    }


    ~LogTimer() {
        const auto duration = std::chrono::duration<double, std::milli>(Clock::now() - m_start).count();
        siren::log::debug("{} took {:.3f}ms", m_name, duration);
    }

private:
    std::string_view m_name;
    Clock::time_point m_start;
};

class SetTimer {
    using Clock = std::chrono::high_resolution_clock;

public:
    explicit SetTimer(
        siren::u32& to_set
    ) : m_to_set(to_set) {
        m_start = Clock::now();
    }

    ~SetTimer() {
        const auto duration = std::chrono::duration<double, std::milli>(Clock::now() - m_start).count();
        m_to_set            = duration;
    }

private:
    Clock::time_point m_start;
    siren::u32& m_to_set;
};
} // namespace oiter

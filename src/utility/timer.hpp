#pragma once

#include <2iREN/base.hpp>
#include <chrono>
#include <functional>
#include <ratio>

namespace oiter {

/// @brief Starts a timer. This timer can be queried at any time for the time
/// passed since creation.
/// Optionally, a callback can be passed into the timer which will be invoked
/// once the object is dropped.
template <typename D = std::milli, typename C = std::chrono::high_resolution_clock>
class Timer {
public:
    using Clock    = C;
    using Duration = D;
    using Callback = std::function<void(siren::f64)>;

    Timer(Callback&& callback = nullptr) : m_callback(std::move(callback)) {
        m_start = Clock::now();
    }

    ~Timer() {
        if (m_callback) {
            m_callback(query());
        }
    }

    [[nodiscard]]
    auto query() const noexcept -> siren::f64 {
        return std::chrono::duration<siren::f64, Duration>(Clock::now() - m_start).count();
    }

private:
    Callback m_callback;
    Clock::time_point m_start;
};

using TimerMs = Timer<std::milli>;
} // namespace oiter

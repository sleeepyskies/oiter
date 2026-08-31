#pragma once

#include <optional>

#include "frame_stats.hpp"
#include "methods/method_kind.hpp"

namespace siren {
class Device;
class PerspectiveCamera;
class PerspectiveCameraController;
class Window;
struct Statistics;
} // namespace siren

namespace oiter {
class OitMethod;
} // namespace oiter

namespace gui {
/// @brief Owns the ImGui context and its platform and renderer backends.
class Session {
public:
    Session(const siren::Window& window, siren::Device& device);
    ~Session();

    Session(const Session&)                    = delete;
    auto operator=(const Session&) -> Session& = delete;

private:
    siren::Device& m_device;
};

struct DebugPanel {
    const siren::Statistics& statistics;
    siren::PerspectiveCamera& camera;
    siren::PerspectiveCameraController& controller;
    oiter::OitMethod& oit_method;
    oiter::MethodKind method_kind;
    const oiter::FrameStats& frame_stats;
};

struct Actions {
    std::optional<oiter::MethodKind> oit_method;
};

[[nodiscard]] auto wants_mouse_input() -> bool;
[[nodiscard]] auto wants_keyboard_input() -> bool;

auto prepare_frame() -> void;
[[nodiscard]] auto build_frame(bool debug_panel_visible, const DebugPanel& panel) -> Actions;
auto draw_frame() -> void;
} // namespace gui

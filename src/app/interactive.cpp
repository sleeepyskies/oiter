#include "interactive.hpp"

#include <optional>

#include <stb_image_write.h>

#include "2iREN/core/context.hpp"
#include "2iREN/graphics/device.hpp"
#include "2iREN/graphics/swapchain.hpp"
#include "2iREN/math/extent.hpp"
#include "2iREN/scene/camera.hpp"
#include "2iREN/utility/filesystem.hpp"
#include "2iREN/utility/time.hpp"
#include "2iREN/window/input.hpp"
#include "2iREN/window/window.hpp"

#include "gui.hpp"
#include "scene_renderer.hpp"
#include "skybox.hpp"
#include "utility/timer.hpp"

#ifndef OITER_VFS
#define OITER_VFS "."
#endif

using namespace siren;

namespace oiter {

static auto create_swapchain(Device& device, Window& window) -> Swapchain {
    return device.make_swapchain(
        window,
        {
            .extent = window.framebuffer_extent(),
            .vsync  = false,
        }
    );
}

struct InteractiveApp::Impl {
    Impl(const InteractiveAppOptions& options, InteractiveState& interactive_state) :
        context(Context::make({.level = options.log_level})),
        window(context.make_window({.title = "Oiter"})), device(context.make_device()),
        assets(*device),
        renderer(*device, assets, options.scene_path, options.method, window.extent()),
        swapchain(create_swapchain(*device, window)),
        skybox("oiter://assets/textures/skybox/skybox.cubemap", *device, assets),
        interactive_state(interactive_state), frame_stats({}) {

        camera.set_position(options.camera_position);
        camera.lookat(options.camera_lookat);
        camera.set_aspect(window.aspect());

        gui::init(window, *device);

        window.on_resize([this](const Extent2 extent) {
            camera.set_aspect(static_cast<f32>(extent.x) / static_cast<f32>(extent.y));
            swapchain.update({.extent = extent});
            renderer.resize(extent);
        });
    }

    ~Impl() {
        gui::shutdown();
    }

    Context context;
    Window window;
    std::unique_ptr<Device> device;
    AssetServer assets;
    SceneRenderer renderer;
    Swapchain swapchain;
    Skybox skybox;
    Camera camera               = Camera{{}};
    CameraController controller = CameraController{5.f, 0.5f};
    InteractiveState& interactive_state;
    FrameStats frame_stats;

    auto run() -> void {
        auto last_update = time::elapsed(); // used for fps update

        while (!window.should_close()) {
            time::step();

            auto since_update = time::elapsed().miliseconds() - last_update.miliseconds();

            if (since_update > 1000) {
                last_update     = time::elapsed();
                frame_stats.fps = 1.f / static_cast<f32>(time::delta().seconds());
            }

            TimerMs full_frame_timer{[this](const f64 ms) {
                frame_stats.full_frame_ms = static_cast<u32>(ms);
            }};

            handle_input();
            draw_scene();

            interactive_state.camera_position = camera.position();
        }
    }

    auto handle_input() -> void {
        window.poll_events();

        if (!ImGui::GetIO().WantCaptureMouse) {
            controller.process_look(camera, window.input());
        }

        if (!ImGui::GetIO().WantCaptureKeyboard) {
            controller.process_movement(camera, window.input().keyboard(), time::delta().seconds());
        }

        if (window.input().keyboard().just_pressed(Key::F1)) {
            interactive_state.debug_menu_visible = !interactive_state.debug_menu_visible;
        }

        if (window.input().keyboard().just_pressed(Key::F2)) {
            renderer.reload_shaders();
        }

        if (window.input().keyboard().just_pressed(Key::F3)) {
            interactive_state.skybox_visible = !interactive_state.skybox_visible;
        }
    }

    auto draw_scene() -> void {
        auto cmds             = device->make_command_buffer();
        const auto backbuffer = swapchain.next_image();

        TIMER(frame_stats.oit_render_ms) {
            renderer.render(*cmds, backbuffer, camera);
        }

        if (interactive_state.skybox_visible) {
            skybox.render_behind(*cmds, backbuffer, camera);
        }

        if (interactive_state.debug_menu_visible) {
            const auto actions = gui::render_debug(
                *cmds,
                device->statistics(),
                frame_stats,
                renderer.method()
            );

            if (actions.oit_method.has_value()) {
                interactive_state.oit_method = actions.oit_method.value();
                renderer.set_method(interactive_state.oit_method);
            }
        }
    }
};

InteractiveApp::InteractiveApp(const InteractiveAppOptions& options) :
    m_interactive_state{
        .oit_method      = options.method,
        .camera_position = options.camera_position,
    } {
    FileSystem::mount("oiter", OITER_VFS);
    m_impl = std::make_unique<Impl>(options, m_interactive_state);
}

InteractiveApp::~InteractiveApp() = default;

auto InteractiveApp::run() -> void {
    m_impl->run();
}

} // namespace oiter

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
    Impl(const InteractiveAppOptions& options) :
        context(Context::make({.level = options.log_level})),
        window(context.make_window({.title = "Oiter"})), device(context.make_device()),
        assets(*device),
        renderer(*device, assets, options.scene_path, options.method, window.extent()),
        swapchain(create_swapchain(*device, window)),
        skybox("oiter://assets/textures/skybox/skybox.cubemap", *device, assets), frame_stats({}) {

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
    FrameStats frame_stats;
    bool debug_menu_visible = true;
    bool skybox_visible     = true;
    bool exit               = false;

    auto run() -> void {
        auto last_update = time::elapsed();

        while (!window.should_close() and not exit) {
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

        if (window.input().keyboard().just_pressed(Key::Num1)) {
            debug_menu_visible = !debug_menu_visible;
        }

        if (window.input().keyboard().just_pressed(Key::Num2)) {
            renderer.reload_shaders();
        }

        if (window.input().keyboard().just_pressed(Key::Num3)) {
            skybox_visible = !skybox_visible;
        }

        if (window.input().keyboard().just_pressed(Key::Esc)) {
            exit = true;
        }
    }

    auto draw_scene() -> void {
        auto cmds             = device->make_command_buffer();
        const auto backbuffer = swapchain.next_image();

        TIMER(frame_stats.oit_render_ms) {
            renderer.render(*cmds, backbuffer, camera);

            if (skybox_visible) {
                skybox.render_behind(*cmds, backbuffer, camera);
            }
        }

        if (debug_menu_visible) {
            const auto actions = gui::render_debug(
                *device,
                *cmds,
                backbuffer,
                device->statistics(),
                frame_stats,
                renderer.method()
            );

            if (actions.oit_method.has_value()) {
                renderer.set_method(*actions.oit_method);
            }
        }

        swapchain.present(std::move(cmds));
    }
};

InteractiveApp::InteractiveApp(const InteractiveAppOptions& options) {
    FileSystem::mount("oiter", OITER_VFS);
    m_impl = std::make_unique<Impl>(options);
}

InteractiveApp::~InteractiveApp() = default;

auto InteractiveApp::run() -> void {
    m_impl->run();
}

} // namespace oiter

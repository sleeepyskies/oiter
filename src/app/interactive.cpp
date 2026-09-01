#include "interactive.hpp"

#include <optional>

#include <stb/stb_image_write.h>

#include "2iREN/asset/gltf.hpp"
#include "2iREN/context.hpp"
#include "2iREN/graphics/device.hpp"
#include "2iREN/graphics/swapchain.hpp"
#include "2iREN/input/input.hpp"
#include "2iREN/scene/camera.hpp"
#include "2iREN/utility/filesystem.hpp"
#include "2iREN/utility/time.hpp"
#include "2iREN/window.hpp"

#include "gui.hpp"
#include "scene_renderer.hpp"
#include "skybox.hpp"
#include "utility/bake.hpp"
#include "utility/timer.hpp"

#ifndef OITER_VFS
#define OITER_VFS "."
#endif

namespace oiter {

static auto create_swapchain(siren::Device& device, siren::Window& window) -> siren::Swapchain {
    return device.create_swapchain({
        .label  = std::nullopt,
        .vsync  = true,
        .extent = window.size(),
        .window = &window,
    });
}

struct InteractiveApp::Impl {
    Impl(
        const InteractiveAppOptions& options,
        InteractiveState& interactive_state,
        FrameStats& frame_stats
    ) :
        context(
            siren::Context::create({
                .debug   = true,
                .level   = options.log_level,
                .backend = siren::Backend::Auto,
            })
        ),
        window(context.create_window({.title = "Oiter"})),
        device(context.create_device({.window = window})), assets(*device), input(window),
        renderer(
            *device, assets, options.scene_path, options.method, {window.width(), window.height()}
        ),
        swapchain(create_swapchain(*device, window)),
        skybox("oiter://assets/textures/skybox/skybox.cubemap", *device, assets),
        interactive_state(interactive_state), frame_stats(frame_stats) {
        camera.set_position(options.camera_position);
        camera.look_at(options.camera_lookat);
        camera.set_aspect(
            static_cast<siren::f32>(window.width()) / static_cast<siren::f32>(window.height())
        );

        device->render_thread().spawn([this] { gui::init(window); });
        device->wait_idle();

        window.on_resize([this](const glm::ivec2 size) {
            if (size.x <= 0 || size.y <= 0) {
                return;
            }

            device->wait_idle();
            const glm::uvec2 extent{
                static_cast<siren::u32>(size.x), static_cast<siren::u32>(size.y)
            };
            camera.set_aspect(
                static_cast<siren::f32>(extent.x) / static_cast<siren::f32>(extent.y)
            );
            swapchain = create_swapchain(*device, window);
            renderer.resize(extent);
        });
    }

    ~Impl() {
        device->render_thread().spawn([] { gui::shutdown(); });
    }

    siren::Context context;
    siren::Window window;
    std::unique_ptr<siren::Device> device;
    siren::AssetServer assets;
    siren::Input input;
    SceneRenderer renderer;
    siren::Swapchain swapchain;
    Skybox skybox;
    siren::PerspectiveCamera camera;
    siren::PerspectiveCameraController controller;
    InteractiveState& interactive_state;
    FrameStats& frame_stats;
    std::optional<MethodKind> pending_method;

    auto run() -> void {
        siren::time::step();
        while (!window.should_close()) {
            siren::time::step();
            frame_stats.frame++;
            if (!(frame_stats.frame % 60)) {
                frame_stats.fps = 1.f / static_cast<siren::f32>(siren::time::delta().seconds());
            }
            TimerMs full_frame_timer{[this](const siren::f64 ms) {
                frame_stats.full_frame_ms = static_cast<siren::u32>(ms);
            }};

            handle_input();
            draw_scene();
            draw_gui();

            device->flush_delete_queue();
            input.update();
            interactive_state.camera_position = camera.position();
        }

        device->wait_idle();
    }

    auto handle_input() -> void {
        window.poll_events();

        if (!ImGui::GetIO().WantCaptureMouse) {
            controller.update_look(camera, input);
        }

        if (!ImGui::GetIO().WantCaptureKeyboard) {
            controller.update_position(camera, input);
        }

        if (input.keyboard().just_pressed(siren::Key::F1)) {
            interactive_state.debug_menu_visible = !interactive_state.debug_menu_visible;
        }

        if (input.keyboard().just_pressed(siren::Key::F2)) {
            renderer.reload_shaders();
        }

        if (input.keyboard().just_pressed(siren::Key::F3)) {
            interactive_state.skybox_visible = !interactive_state.skybox_visible;
        }
    }

    auto draw_scene() -> void {
        TimerMs oit_render_timer{[this](const siren::f64 ms) {
            frame_stats.oit_render_ms = static_cast<siren::u32>(ms);
        }};
        const auto& image = renderer.render(camera);
        if (interactive_state.skybox_visible) {
            skybox.render_behind(image, camera);
        }
        device->blit_image(image.handle(), swapchain.next_image());
    }

    auto draw_gui() -> void {
        swapchain.present_overlay([this] {
            if (!interactive_state.debug_menu_visible) {
                return;
            }

            const auto actions =
                gui::render_debug(device->statistics(), frame_stats, renderer.method());

            if (actions.oit_method) {
                pending_method = actions.oit_method;
            }
        });
        device->wait_idle();

        if (pending_method) {
            interactive_state.oit_method = *pending_method;
            renderer.set_method(interactive_state.oit_method, window.size());
            pending_method.reset();
        }
    }
};

InteractiveApp::InteractiveApp(const InteractiveAppOptions& options) :
    m_interactive_state{
        .oit_method      = options.method,
        .camera_position = options.camera_position,
    } {
    siren::FileSystem::mount("oiter", OITER_VFS);
    m_impl = std::make_unique<Impl>(options, m_interactive_state, m_frame_stats);
}

InteractiveApp::~InteractiveApp() = default;

auto InteractiveApp::run() -> void { m_impl->run(); }

} // namespace oiter

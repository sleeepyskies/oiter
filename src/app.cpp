#include "app.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/context.hpp"
#include "2iREN/graphics/device.hpp"
#include "2iREN/graphics/graphics_pipeline.hpp"
#include "2iREN/graphics/layout.hpp"
#include "2iREN/graphics/swapchain.hpp"
#include "2iREN/input/input.hpp"
#include "2iREN/scene/camera.hpp"
#include "2iREN/utility/filesystem.hpp"
#include "2iREN/utility/time.hpp"
#include "2iREN/window.hpp"

#include "frame_stats.hpp"
#include "gui.hpp"
#include "image_output.hpp"
#include "scene_renderer.hpp"
#include "skybox.hpp"
#include "timer.hpp"

#ifndef OITER_VFS
#define OITER_VFS "."
#endif

namespace {

auto mount_app_filesystem() -> void { siren::FileSystem::mount("oiter", siren::Path{OITER_VFS}); }

[[nodiscard]]
auto aspect_ratio(const glm::uvec2 extent) -> siren::f32 {
    return static_cast<siren::f32>(extent.x) / static_cast<siren::f32>(extent.y);
}

[[nodiscard]]
auto create_swapchain(siren::Device& device, siren::Window& window) -> siren::Swapchain {
    return device.create_swapchain({
        .label  = std::nullopt,
        .vsync  = true,
        .extent = window.size(),
        .window = &window,
    });
}

auto validate(const oiter::RenderAppOptions& options) -> void {
    if (options.dimensions.x == 0 || options.dimensions.y == 0) {
        throw std::invalid_argument("Render dimensions must be greater than zero.");
    }

    if (options.output_path.empty()) {
        throw std::invalid_argument("An output path is required in render mode.");
    }
}
} // namespace

namespace oiter {
struct InteractiveApp::State {
    siren::Context context;
    siren::Window window;
    std::unique_ptr<siren::Device> device;
    siren::AssetServer assets;
    siren::Input input;
    siren::Swapchain swapchain;

    SceneRenderer renderer;
    siren::PerspectiveCamera camera;
    siren::PerspectiveCameraController controller;
    Skybox skybox;
    gui::Session gui_session;

    FrameStats frame_stats;
    siren::f64 fps_sample_seconds = 0.0;
    siren::u32 fps_sample_frames  = 0;
    bool debug_menu_visible       = true;
    bool skybox_visible           = true;

    explicit State(const InteractiveAppOptions& options) :
        context(
            siren::Context::create({
                .debug   = true,
                .level   = options.log_level,
                .backend = siren::Backend::Auto,
            })
        ),
        window(context.create_window({.title = "Oiter"})),
        device(context.create_device({.window = window})), assets(*device), input(window),
        swapchain(create_swapchain(*device, window)),
        renderer(*device, assets, options.scene_path, options.method, window.size()),
        camera(
            siren::PerspectiveCameraDescriptor{
                .position = options.camera_position,
                .aspect   = aspect_ratio(window.size()),
            }
        ),
        skybox("oiter://assets/textures/skybox/skybox.cubemap", *device, assets),
        gui_session(window, *device) {
        camera.look_at(options.camera_lookat);
        window.on_resize([this](const glm::ivec2 size) { resize(size); });
    }

    auto run() -> void {
        siren::time::step();
        while (!window.should_close()) {
            tick();
        }
    }

    auto tick() -> void {
        siren::time::step();
        if (frame_stats.frame > 0) {
            sample_fps(siren::time::delta().seconds());
        }

        SetTimer full_frame_timer{frame_stats.full_frame_ms};
        ++frame_stats.frame;

        window.poll_events();
        handle_input();
        render_scene();
        render_gui();

        device->flush_delete_queue();
        input.update();
    }

    auto handle_input() -> void {
        if (!gui::wants_mouse_input()) {
            controller.update_look(camera, input);
        }

        if (!gui::wants_keyboard_input()) {
            controller.update_position(camera, input);
        }

        if (input.keyboard().just_pressed(siren::Key::F1)) {
            debug_menu_visible = !debug_menu_visible;
        }

        if (input.keyboard().just_pressed(siren::Key::F2)) {
            renderer.reload_shaders();
        }

        if (input.keyboard().just_pressed(siren::Key::F3)) {
            skybox_visible = !skybox_visible;
        }
    }

    auto render_scene() -> void {
        SetTimer oit_render_timer{frame_stats.oit_render_ms};

        const auto& image = renderer.render(camera);
        if (skybox_visible) {
            skybox.render_behind(image, camera);
        }

        device->blit_image(image.handle(), swapchain.next_image());
    }

    auto render_gui() -> void {
        device->render_thread().spawn([] { gui::prepare_frame(); });
        device->wait_idle();

        const auto statistics = device->statistics();
        const auto actions    = gui::build_frame(
            debug_menu_visible,
            {
                .statistics  = statistics,
                .camera      = camera,
                .controller  = controller,
                .oit_method  = renderer.method(),
                .method_kind = renderer.method_kind(),
                .frame_stats = frame_stats,
            }
        );

        swapchain.present_overlay([] { gui::draw_frame(); });
        device->wait_idle();

        if (actions.oit_method) {
            renderer.change_method(*actions.oit_method);
        }
    }

    auto resize(const glm::ivec2 size) -> void {
        if (size.x <= 0 || size.y <= 0) {
            return;
        }

        device->wait_idle();

        const auto extent = glm::uvec2{
            static_cast<siren::u32>(size.x),
            static_cast<siren::u32>(size.y),
        };
        auto resized_swapchain = create_swapchain(*device, window);

        renderer.resize(extent);
        swapchain = std::move(resized_swapchain);
        camera.set_aspect(aspect_ratio(extent));
    }

    auto sample_fps(const siren::f64 frame_seconds) -> void {
        fps_sample_seconds += frame_seconds;
        ++fps_sample_frames;

        if (fps_sample_frames < 60) {
            return;
        }

        if (fps_sample_seconds > 0.0) {
            frame_stats.fps = static_cast<siren::f32>(
                static_cast<siren::f64>(fps_sample_frames) / fps_sample_seconds
            );
        }

        fps_sample_seconds = 0.0;
        fps_sample_frames  = 0;
    }
};

struct RenderApp::State {
    siren::Context context;
    siren::Window window;
    std::unique_ptr<siren::Device> device;
    siren::AssetServer assets;

    SceneRenderer renderer;
    siren::PerspectiveCamera camera;
    std::string output_path;

    explicit State(const RenderAppOptions& options) :
        context(
            siren::Context::create({
                .debug   = true,
                .level   = options.log_level,
                .backend = siren::Backend::Auto,
            })
        ),
        window(context.create_window({
            .title        = "Oiter",
            .width        = options.dimensions.x,
            .height       = options.dimensions.y,
            .decorated    = false,
            .resizable    = false,
            .transparent  = false,
            .initial_mode = siren::WindowMode::Normal,
        })),
        device(context.create_device({.window = window})), assets(*device),
        renderer(*device, assets, options.scene_path, options.method, options.dimensions),
        camera(
            siren::PerspectiveCameraDescriptor{
                .position = options.camera_position,
                .aspect   = aspect_ratio(options.dimensions),
            }
        ),
        output_path(options.output_path) {
        camera.look_at(options.camera_lookat);
    }

    ~State() { device->wait_idle(); }

    auto run() -> void {
        const auto& image = renderer.render(camera);
        write_rendered_image(*device, assets, image, output_path);
    }
};

InteractiveApp::InteractiveApp(const InteractiveAppOptions& options) {
    mount_app_filesystem();
    m_state = std::make_unique<State>(options);
}

InteractiveApp::~InteractiveApp() = default;

auto InteractiveApp::run() -> void { m_state->run(); }

RenderApp::RenderApp(const RenderAppOptions& options) {
    validate(options);
    mount_app_filesystem();
    m_state = std::make_unique<State>(options);
}

RenderApp::~RenderApp() = default;

auto RenderApp::run() -> void { m_state->run(); }
} // namespace oiter

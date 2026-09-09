#include "screen_door.hpp"
#include <imgui.h>
#include <memory>

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/asset/shader.hpp"
#include "2iREN/graphics/buffer.hpp"
#include "2iREN/graphics/graphics_pipeline.hpp"
#include "2iREN/graphics/image.hpp"
#include "2iREN/graphics/render_command.hpp"
#include "2iREN/graphics/render_target.hpp"
#include "2iREN/math/extent.hpp"

#include "2iREN/utility/byte_buffer.hpp"
#include "2iREN/utility/identifier.hpp"

#include "methods/oit_method.hpp"

namespace oiter {

ScreenDoor::ScreenDoor(
    siren::Device& device,
    const siren::Extent2u extent,
    siren::AssetServer& assets
) : OitMethod(device, assets) {
    create_images(extent);
    create_shaders();
    create_buffer();
}

auto ScreenDoor::render(const siren::Camera& camera, const BakedScene& scene) const
    -> siren::ImageHandle {
    update_buffers(camera, scene);

    auto draw_scene = [&](siren::RenderPassRecorder& pass) {
        pass.bind_uniform_buffer(m_scene_buffer->handle(), 0);
        for (const auto& [index, surface] : std::views::enumerate(scene.transparent)) {
            pass.bind_uniform_buffer_range(
                m_mesh_buffer->handle(),
                1,
                mesh_uniforms_alignment() * (scene.opaque.size() + index),
                sizeof(MeshUniforms)
            );
            pass.bind_uniform_buffer(m_ubo->handle(), 2);
            pass.bind_vertex_buffer(surface.vertex.buffer.handle(), 0, 0);
            pass.bind_index_buffer(surface.index.buffer.handle(), surface.index.format);
            pass.draw_indexed(surface.index.count, 0);
        }
    };

    m_device.render_pass(
        siren::RenderPassDescriptor{
            .label = "Screen Door Transparency Pass",
            .target =
                siren::RenderTarget{
                    .colors =
                        {
                            siren::ColorAttachment{
                                .image           = m_output->handle(),
                                .begin_operation = siren::BeginOperation::Clear,
                                .clear_color     = siren::Rgba::ZERO(),
                            },

                        },
                    .depth_stencil =
                        siren::DepthStencilAttachment{
                            .image           = m_depth->handle(),
                            .begin_operation = siren::BeginOperation::Clear,
                            .clear_depth     = 1,
                            .clear_stencil   = 0,
                        },
                },
        },
        [&](siren::RenderPassRecorder& pass) {
            pass.bind_graphics_pipeline(m_screendoor_pipeline->handle());
            draw_scene(pass);
        }
    );

    return m_output->handle();
}

auto ScreenDoor::resize(const siren::Extent2u extent) -> void {
    create_images(extent);
}

auto ScreenDoor::reload_shaders() -> void {
    m_screendoor_shader   = siren::NullHandle;
    m_screendoor_pipeline = nullptr;

    create_shaders();
}

auto ScreenDoor::render_debug_info() -> void {
    auto gridsize = (siren::i32*)&m_config.gridsize;

    ImGui::Text("Pattern Size");
    if (ImGui::RadioButton("2x2", gridsize, 0)) {
        m_config.gridsize = Config::TwoXTwo;
        create_buffer();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("4x4", gridsize, 1)) {
        m_config.gridsize = Config::FourXFour;
        create_buffer();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("8x8", gridsize, 2)) {
        m_config.gridsize = Config::EightXEight;
        create_buffer();
    }
    ImGui::SameLine();
}

auto ScreenDoor::create_images(const siren::Extent2u extent) -> void {
    m_output = nullptr;
    m_depth  = nullptr;

    m_output = create_standard_image(extent, "ScreenDoor Output Image", siren::ImageFormat::RGBA8);
    m_depth  = create_standard_image(
        extent, "ScreenDoor Depth Buffer", siren::ImageFormat::Depth24Stencil8
    );
}

auto ScreenDoor::create_shaders() -> void {
    m_screendoor_shader =
        m_assets.load<siren::ShaderAsset>("oiter://assets/shaders/screendoor/dither.sshg");
    m_assets.wait_until_loaded(m_screendoor_shader);
    const auto& shader = m_assets.get_unsafe(m_screendoor_shader);
    m_screendoor_pipeline =
        std::make_unique<siren::GraphicsPipeline>(m_device.make_graphics_pipeline({
            .label             = "ScreenDoor Dither Pipeline",
            .shader            = shader.shader.handle(),
            .alpha_mode        = siren::AlphaMode::Opaque,
            .depth_function    = siren::DepthFunction::Less,
            .back_face_culling = false,
            .depth_test        = true,
            .depth_write       = true,
        }));
}

auto ScreenDoor::create_buffer() -> void {
    const auto buf = siren::ByteBuffer{ScreenDoorUniforms{.size = m_config.gridsize}};

    m_ubo = std::make_unique<siren::Buffer>(m_device.make_buffer(
        {
            .label = "Screen Door Uniforms",
            .size  = sizeof(ScreenDoorUniforms),
            .usage = siren::BufferUsage::Static,
        },
        buf.view()
    ));
}

} // namespace oiter

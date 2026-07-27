#pragma once

#include "2iren/rhi/resources/image.hpp"
#include "2iren/util/camera.hpp"

namespace oiter {

struct BakedScene;

/**
 * @brief Base class all OIT methods should implement.
 */
class OitMethod {
public:
    virtual ~OitMethod() = default;

    /**
     * @brief The main render function of the OIT method.
     * @param camera The camera to render the scene from.
     * @param scene The scene to render.
     * @return An image of the final rendered scene.
     */
    [[nodiscard]] virtual auto render(
        const siren::PerspectiveCamera& camera,
        const BakedScene& scene
    ) const -> const siren::Image& = 0;


    /**
     * @brief Initiates a resize of the OIT method. The OIT method should reconstruct all sized resources.
     * @param extent The new size.
     */
    virtual auto resize(const glm::uvec2 extent) -> void = 0;

    /**
     * @brief Returns the name of this method.
     */
    [[nodiscard]] virtual auto name() const noexcept -> std::string_view = 0;

    /**
     * @brief Can be optionally implemented to display custom debug information.
     *
     * Implementations of this function should render using ImGui, which will be displayed
     * on the debug panel. This can be toggled by pressing F1.
     */
    virtual auto render_debug_info() -> void {}
};

}  // namespace oiter

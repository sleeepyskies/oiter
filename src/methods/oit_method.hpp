#pragma once
#include "2iren/rhi/resources/image.hpp"

namespace oiter {

struct BakedScene;

class OitMethod {
public:
    virtual ~OitMethod()                                                                       = default;
    [[nodiscard]] virtual auto render(const BakedScene& scene) const -> const siren::Image& = 0;
};

}  // namespace oiter

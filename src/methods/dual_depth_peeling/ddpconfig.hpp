#pragma once
#include "2iren/base.hpp"

namespace oiter {
enum class DdpOutputImage {
    Init,
    Peel,
    Final,
};

/**
 * @brief A bunch of configuration options that can be tweaked during runtime for
 * the @ref DualDepthPeeling OitMethod. These can be viewed in the F1 debug menu when
 * the DualDepthPeeling method is active.
 */
struct DdpConfig {
    /** @brief The maximum number of peels to perform. */
    siren::i32 max_peels = 5;
    /** @brief Whether to query in order to stop early. */
    bool occlusion_query = true;
    /** @brief Which image of the method out display. Not showing an early stage still performs the later ones. */
    DdpOutputImage output_image = DdpOutputImage::Final;
};
}

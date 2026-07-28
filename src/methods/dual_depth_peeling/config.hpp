#pragma once
#include "2iren/base.hpp"

namespace oiter {
/**
 * @brief A bunch of configuration options that can be tweaked during runtime for
 * the @ref DualDepthPeeling OitMethod. These can be viewed in the F1 debug menu when
 * the DualDepthPeeling method is active.
 */
struct DualDepthPeelingConfig {
    /** @brief The maximum number of peels to perform. */
    siren::i32 max_peels = 5;
    /** @brief Whether or not to query in order to stop early. */
    bool occlusion_query = true;
};
}


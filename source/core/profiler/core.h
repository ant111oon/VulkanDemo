#pragma once

#if !defined(ENG_BUILD_RELEASE)
    #define ENG_PROFILING_ENABLED
#endif


#if defined(ENG_PROFILING_ENABLED)
    #define TRACY_ENABLED
#endif


#if defined(ENG_PROFILING_ENABLED)
#include "tracy/../common/TracyColor.hpp"


namespace prfl
{
    using Color = tracy::Color;
}
#endif

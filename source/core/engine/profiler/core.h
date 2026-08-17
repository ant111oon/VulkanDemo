#pragma once

#if !defined(ENG_BUILD_RELEASE)
    #define ENG_PROFILING_ENABLED
#endif


#if defined(ENG_PROFILING_ENABLED)
    #define TRACY_ENABLED
#endif


#if defined(ENG_PROFILING_ENABLED)
#include "tracy/../common/TracyColor.hpp"
#endif


namespace eng
{
    namespace profile
    {    
        inline constexpr size_t ENG_PROFILE_MARKER_NAME_LEN = 256;
    }
}

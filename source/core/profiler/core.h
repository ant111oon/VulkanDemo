#pragma once

#if !defined(ENG_BUILD_RELEASE)
    #define ENG_PROFILING_ENABLED
#endif


#if defined(ENG_PROFILING_ENABLED)
    #define TRACY_ENABLED
#endif

#pragma once

#if !defined(ENG_BUILD_RELEASE)
    #define ENG_PROFILING_ENABLED
#endif


#if defined(ENG_PROFILING_ENABLED)
    #define TRACY_ENABLED
#endif


#define _ENG_PROFILE_MAKE_COLOR_U32(R, G, B, A) \
    uint32_t(((uint32_t(A) & 0xFFU) << 24U) | ((uint32_t(R) & 0xFFU) << 16U) | ((uint32_t(G) & 0xFFU) << 8U) | (uint32_t(B) & 0xFFU))

#define _ENG_PROFILE_CONCAT(A, B) _ENG_PROFILE_CONCAT_INDIRECT(A, B)
#define _ENG_PROFILE_CONCAT_INDIRECT(A, B) A##B

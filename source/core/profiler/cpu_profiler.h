#pragma once

#include "core/profiler/core.h"

#include <tracy/Tracy.hpp>
#include <cstdint>


#define _ENG_PROFILE_MAKE_COLOR_U32(R, G, B, A) (((uint32_t(A) & 0xFFU) << 24U) | ((uint32_t(R) & 0xFFU) << 16U) | ((uint32_t(G) & 0xFFU) << 8U) | (uint32_t(B) & 0xFFU))


#define ENG_PROFILE_BEGIN_FRAME(NAME)   FrameMarkStart(NAME)
#define ENG_PROFILE_END_FRAME(NAME)     FrameMarkEnd(NAME)


#define ENG_PROFILE_SCOPED_MARKER(NAME, LABEL_CSTR)                   ZoneNamedN(NAME, LABEL_CSTR, true)
#define ENG_PROFILE_SCOPED_MARKER_C(NAME, LABEL_CSTR, R8, G8, B8, A8) ZoneNamedNC(NAME, LABEL_CSTR, _ENG_PROFILE_MAKE_COLOR_U32(R8, G8, B8, A8), true)

// For very short-lived events that is called frequently
#define ENG_PROFILE_TRANSIENT_SCOPED_MARKER(NAME, LABEL_CSTR)                   ZoneTransientN(NAME, LABEL_CSTR, true)
// For very short-lived events that is called frequently
#define ENG_PROFILE_TRANSIENT_SCOPED_MARKER_C(NAME, LABEL_CSTR, R8, G8, B8, A8) ZoneTransientNC(NAME, LABEL_CSTR, _ENG_PROFILE_MAKE_COLOR_U32(R8, G8, B8, A8), true)


#define ENG_PROFILE_BEGIN_MARKER_SCOPE(NAME, LABEL_CSTR)    \
    {                                                       \
        ENG_PROFILE_SCOPED_MARKER(NAME, LABEL_CSTR)

#define ENG_PROFILE_BEGIN_MARKER_C_SCOPE(NAME, LABEL_CSTR, R8, G8, B8, A8)  \
    {                                                                       \
        ENG_PROFILE_SCOPED_MARKER_C(NAME, LABEL_CSTR, R8, G8, B8, A8)

#define ENG_PROFILE_BEGIN_TRANSIENT_MARKER_SCOPE(NAME, LABEL_CSTR)      \
    {                                                                   \
        ENG_PROFILE_TRANSIENT_SCOPED_MARKER(NAME, LABEL_CSTR)

#define ENG_PROFILE_BEGIN_TRANSIENT_MARKER_C_SCOPE(NAME, LABEL_CSTR, R8, G8, B8, A8)    \
    {                                                                                   \
        ENG_PROFILE_TRANSIENT_SCOPED_MARKER_C(NAME, LABEL_CSTR, R8, G8, B8, A8)


#define ENG_PROFILE_END_MARKER_SCOPE() }


#define ENG_PROFILE_MARKER_TEXT(MARKER_NAME, FMT, ...) ZoneTextVF(MARKER_NAME, FMT, __VA_ARGS__)
#define ENG_PROFILE_MARKER_VALUE(MARKER_NAME, VALUE) ZoneValueV(MARKER_NAME, VALUE)

#define ENG_PROFILE_IS_MARKER_ACTIVE(MARKER_NAME) ZoneIsActiveV(MARKER_NAME)


#define ENG_PROFILE_LOG(TEXT, R8, G8, B8, A8) TracyMessageC(TEXT, strlen(TEXT), _ENG_PROFILE_MAKE_COLOR_U32(R8, G8, B8, A8))

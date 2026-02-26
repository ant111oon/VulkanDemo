#pragma once

#include "core/engine/profiler/core.h"

#include <array>
#include <cstdint>


#if defined(ENG_PROFILING_ENABLED)
#include <tracy/Tracy.hpp>


#define ENG_PROFILE_FRAME(NAME) FrameMarkNamed(NAME)

#define ENG_PROFILE_BEGIN_FRAME(NAME) FrameMarkStart(NAME)
#define ENG_PROFILE_END_FRAME(NAME)   FrameMarkEnd(NAME)


#pragma region Named Markers
#define ENG_PROFILE_SCOPED_MARKER_N(NAME, LABEL)         ZoneNamedN(NAME, LABEL, true)
#define ENG_PROFILE_SCOPED_MARKER_NC(NAME, LABEL, COLOR) ZoneNamedNC(NAME, LABEL, COLOR, true)


#define ENG_PROFILE_SCOPED_MARKER_NC_FMT(NAME, COLOR, FMT, ...)                                                                             \
    std::array<char, 256> TracyConcat(localCPUMarkerName_,TracyLine) = {};                                                                  \
    sprintf_s(TracyConcat(localCPUMarkerName_,TracyLine).data(), TracyConcat(localCPUMarkerName_,TracyLine).size() - 1, FMT, __VA_ARGS__);  \
    ZoneNamed(NAME, true);                                                                                                                  \
    ZoneNameV(NAME, TracyConcat(localCPUMarkerName_,TracyLine).data(), TracyConcat(localCPUMarkerName_,TracyLine).size());                  \
    ZoneColorV(NAME, COLOR)


#define ENG_PROFILE_SCOPED_MARKER_N_FMT(NAME, FMT, ...) \
    ENG_PROFILE_SCOPED_MARKER_NC_FMT(NAME, eng::ProfileColor::Grey51, FMT, __VA_ARGS__)
#pragma endregion


#pragma region Unamed Markers
#define ENG_PROFILE_SCOPED_MARKER(LABEL)          ZoneScopedN(LABEL)
#define ENG_PROFILE_SCOPED_MARKER_C(LABEL, COLOR) ZoneScopedNC(LABEL, COLOR)


#define ENG_PROFILE_SCOPED_MARKER_C_FMT(COLOR, FMT, ...) \
    ENG_PROFILE_SCOPED_MARKER_NC_FMT(TracyConcat(localMarker_,TracyLine), COLOR, FMT, __VA_ARGS__)


#define ENG_PROFILE_SCOPED_MARKER_FMT(FMT, ...) \
    ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::Grey51, FMT, __VA_ARGS__)
#pragma endregion


#pragma region Transient Markers
// For very short-lived events that is called frequently
#define ENG_PROFILE_TRANSIENT_SCOPED_MARKER_N(NAME, LABEL)         ZoneTransientN(NAME, LABEL, true)

// For very short-lived events that is called frequently
#define ENG_PROFILE_TRANSIENT_SCOPED_MARKER_NC(NAME, LABEL, COLOR) ZoneTransientNC(NAME, LABEL, COLOR, true)

// For very short-lived events that is called frequently
#define ENG_PROFILE_TRANSIENT_SCOPED_MARKER(LABEL) \
    ENG_PROFILE_TRANSIENT_SCOPED_MARKER_N(TracyConcat(localTransientCPUMarker_, TracyLine), LABEL)

// For very short-lived events that is called frequently
#define ENG_PROFILE_TRANSIENT_SCOPED_MARKER_C(LABEL, COLOR) \
    ENG_PROFILE_TRANSIENT_SCOPED_MARKER_NC(TracyConcat(localTransientCPUMarker_, TracyLine), LABEL, COLOR)
#pragma endregion


#pragma region Markers Geters-Setters
#define ENG_PROFILE_MARKER_N_TEXT(NAME, FMT, ...)  ZoneTextVF(NAME, FMT, __VA_ARGS__)
#define ENG_PROFILE_MARKER_N_VALUE(NAME, VALUE)    ZoneValueV(NAME, VALUE)

#define ENG_PROFILE_MARKER_TEXT(FMT, ...)  ZoneTextF(FMT, __VA_ARGS__)
#define ENG_PROFILE_MARKER_VALUE(VALUE)    ZoneValue(VALUE)

#define ENG_PROFILE_IS_MARKER_ACTIVE()       ZoneIsActive
#define ENG_PROFILE_IS_MARKER_N_ACTIVE(NAME) ZoneIsActiveV(NAME)
#pragma endregion


#pragma region Logging
#define ENG_PROFILE_LOG_C(TEXT, COLOR) TracyMessageC(TEXT, strlen(TEXT), COLOR)
#pragma endregion
#else
#define ENG_PROFILE_FRAME(NAME)

#define ENG_PROFILE_BEGIN_FRAME(NAME)
#define ENG_PROFILE_END_FRAME(NAME)


#pragma region Named Markers
#define ENG_PROFILE_SCOPED_MARKER_N(NAME, LABEL)
#define ENG_PROFILE_SCOPED_MARKER_NC(NAME, LABEL, COLOR)
#define ENG_PROFILE_SCOPED_MARKER_NC_FMT(NAME, COLOR, FMT, ...)
#define ENG_PROFILE_SCOPED_MARKER_N_FMT(NAME, FMT, ...)
#pragma endregion


#pragma region Unamed Markers
#define ENG_PROFILE_SCOPED_MARKER(LABEL)
#define ENG_PROFILE_SCOPED_MARKER_C(LABEL, COLOR)
#define ENG_PROFILE_SCOPED_MARKER_C_FMT(COLOR, FMT, ...)
#define ENG_PROFILE_SCOPED_MARKER_FMT(FMT, ...)
#pragma endregion


#pragma region Transient Markers
#define ENG_PROFILE_TRANSIENT_SCOPED_MARKER_N(NAME, LABEL)
#define ENG_PROFILE_TRANSIENT_SCOPED_MARKER_NC(NAME, LABEL, COLOR)
#define ENG_PROFILE_TRANSIENT_SCOPED_MARKER(LABEL)
#define ENG_PROFILE_TRANSIENT_SCOPED_MARKER_C(LABEL, COLOR)
#pragma endregion


#pragma region Markers Geters-Setters
#define ENG_PROFILE_MARKER_N_TEXT(NAME, FMT, ...)
#define ENG_PROFILE_MARKER_N_VALUE(NAME, VALUE)

#define ENG_PROFILE_MARKER_TEXT(FMT, ...)
#define ENG_PROFILE_MARKER_VALUE(VALUE)

#define ENG_PROFILE_IS_MARKER_ACTIVE()
#define ENG_PROFILE_IS_MARKER_N_ACTIVE(NAME)
#pragma endregion


#pragma region Logging
#define ENG_PROFILE_LOG_C(TEXT, COLOR)
#pragma endregion
#endif

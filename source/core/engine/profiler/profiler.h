#pragma once

#include <array>
#include <cstdint>
#include <string_view>


#if !defined(ENG_BUILD_RELEASE)
    #define ENG_PROFILING_ENABLED
#endif


#if defined(ENG_PROFILING_ENABLED)
    #define TRACY_ENABLED
#endif


#if defined(ENG_PROFILING_ENABLED)
#include "render/core/vulkan/vk_cmd.h"

#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#include "core/core.h"


namespace eng
{
    class GpuProfiler
    {
        friend GpuProfiler& GetGpuProfiler();

    public:
        ENG_DECL_CLASS_NO_COPIABLE(GpuProfiler);
        ENG_DECL_CLASS_NO_MOVABLE(GpuProfiler);

        ~GpuProfiler();

        GpuProfiler& Create(vkn::Device* pDevice);
        GpuProfiler& Destroy();

        const GpuProfiler& BeginCmdGroup(vkn::CmdBuffer& cmd, std::string_view groupName) const;
        const GpuProfiler& BeginCmdGroup(vkn::CmdBuffer& cmd, std::string_view groupName, uint32_t color = 0x7f7f7f) const;

        const GpuProfiler& EndCmdGroup(vkn::CmdBuffer& cmd) const;

        const GpuProfiler& CollectCmdStats(vkn::CmdBuffer& cmd) const;

        TracyVkCtx GetTracyCtx() const;

        bool IsCreated() const;

    private:
        GpuProfiler() = default;

    private:
        vkn::Device* m_pDevice = nullptr;

        vkn::CmdPool m_cmdPool;
        vkn::CmdBuffer* m_pCmdBuffer;

        TracyVkCtx m_context = nullptr;
    };


    class GpuMarker
    {
    public:
        GpuMarker(vkn::CmdBuffer& cmd, std::string_view name, uint32_t color = 0x7f7f7f);
        ~GpuMarker();

    private:
        vkn::CmdBuffer& m_cmdBuf;
    };


    GpuProfiler& GetGpuProfiler(); 

    inline std::string_view BuildMarkerName(std::string_view name)
    {
        static char buff[256] = {0};
        sprintf_s(buff, name.data());

        return buff;
    }

    template <typename... Args>
    inline std::string_view BuildMarkerName(std::string_view fmt, Args&&... args)
    {
        static char buff[256] = {0};
        sprintf_s(buff, fmt.data(), std::forward<Args>(args)...);

        return buff;
    }


    void SetName(tracy::ScopedZone& marker, std::string_view name);
}


#define TM_CPU_MARKER_VAR    TracyConcat(_cpuMarker,TracyLine)
#define TM_GPU_MARKER_VAR    TracyConcat(_gpuMarker,TracyLine)

#define TM_GPU_PROFILER      eng::GetGpuProfiler()
#define TM_GPU_TRACY_CTX     TM_GPU_PROFILER.GetTracyCtx()


#pragma region CPU MARKERS
#define TM_FRAME(NAME) FrameMarkNamed(NAME)

#define TM_BEGIN_FRAME(NAME) FrameMarkStart(NAME)
#define TM_END_FRAME(NAME)   FrameMarkEnd(NAME)

#define TM_MARKER_N(NAME, LABEL)         ZoneNamedN(NAME, LABEL, true)
#define TM_MARKER_NC(NAME, COLOR, LABEL) ZoneNamedNC(NAME, LABEL, COLOR, true)

#define TM_MARKER_NC_FMT(NAME, COLOR, FMT, ...)                                                      \
    ZoneNamed(NAME, true); eng::SetName(NAME, eng::BuildMarkerName(FMT, __VA_ARGS__)); ZoneColorV(NAME, COLOR)

#define TM_MARKER_N_FMT(NAME, FMT, ...) TM_MARKER_NC_FMT(NAME, 0x828282, FMT, __VA_ARGS__)

#define TM_MARKER(LABEL)          ZoneScopedN(LABEL)
#define TM_MARKER_C(COLOR, LABEL) ZoneScopedNC(LABEL, COLOR)

#define TM_MARKER_C_FMT(COLOR, FMT, ...) TM_MARKER_NC_FMT(TM_CPU_MARKER_VAR, COLOR, FMT, __VA_ARGS__)

#define TM_MARKER_FMT(FMT, ...) TM_MARKER_C_FMT(0x828282, FMT, __VA_ARGS__)

// For very short-lived events that is called frequently
#define TM_TRANSIENT_MARKER_N(NAME, LABEL)         ZoneTransientN(NAME, LABEL, true)

// For very short-lived events that is called frequently
#define TM_TRANSIENT_MARKER_NC(NAME, COLOR, LABEL) ZoneTransientNC(NAME, LABEL, COLOR, true)

// For very short-lived events that is called frequently
#define TM_TRANSIENT_MARKER(LABEL) TM_TRANSIENT_MARKER_N(TM_CPU_MARKER_VAR, LABEL)

// For very short-lived events that is called frequently
#define TM_TRANSIENT_MARKER_C(COLOR, LABEL) TM_TRANSIENT_MARKER_NC(TM_CPU_MARKER_VAR, COLOR, LABEL)

#define TM_MARKER_N_TEXT(NAME, FMT, ...)  ZoneTextVF(NAME, FMT, __VA_ARGS__)
#define TM_MARKER_N_VALUE(NAME, VALUE)    ZoneValueV(NAME, VALUE)

#define TM_MARKER_TEXT(FMT, ...)  ZoneTextF(FMT, __VA_ARGS__)
#define TM_MARKER_VALUE(VALUE)    ZoneValue(VALUE)

#define TM_IS_MARKER_ACTIVE()       ZoneIsActive
#define TM_IS_MARKER_ACTIVE_N(NAME) ZoneIsActiveV(NAME)

#define TM_LOG_C(COLOR, TEXT) TracyMessageC(TEXT, strlen(TEXT), COLOR)

#pragma endregion CPU MARKERS


#pragma region GPU MARKERS
#define TM_GPU_MARKER_NC(CMD_BUFFER, NAME, COLOR, LABEL)    \
    TracyVkNamedZoneC(                                      \
        TM_GPU_TRACY_CTX,                                   \
        TracyConcat(NAME,_Tracy),                           \
        CMD_BUFFER.Get(),                                   \
        LABEL,                                              \
        COLOR,                                              \
        true);                                              \
    eng::GpuMarker NAME(CMD_BUFFER, LABEL, COLOR)

#define TM_GPU_MARKER_N(CMD_BUFFER, NAME, LABEL) TM_GPU_MARKER_NC(CMD_BUFFER, NAME, 0x7f7f7f, LABEL)

#define TM_GPU_MARKER_NC_FMT(CMD_BUFFER, NAME, COLOR, FMT, ...) \
    TracyVkZoneTransient(                                       \
        TM_GPU_TRACY_CTX,                                       \
        TracyConcat(NAME,_Tracy),                               \
        CMD_BUFFER.Get(),                                       \
        eng::BuildMarkerName(FMT, __VA_ARGS__).data(),          \
        true);                                                  \
    eng::GpuMarker NAME(CMD_BUFFER, eng::BuildMarkerName(FMT, __VA_ARGS__), COLOR)

#define TM_GPU_MARKER_N_FMT(CMD_BUFFER, NAME, FMT, ...)     TM_GPU_MARKER_NC_FMT(CMD_BUFFER, NAME, 0x7f7f7f, FMT, __VA_ARGS__)

#define TM_GPU_MARKER(CMD_BUFFER, LABEL)                    TM_GPU_MARKER_N(CMD_BUFFER, TM_GPU_MARKER_VAR, LABEL)
#define TM_GPU_MARKER_C(CMD_BUFFER, COLOR, LABEL)           TM_GPU_MARKER_NC(CMD_BUFFER, TM_GPU_MARKER_VAR, COLOR, LABEL)
#define TM_GPU_MARKER_FMT(CMD_BUFFER, FMT, ...)             TM_GPU_MARKER_N_FMT(CMD_BUFFER, TM_GPU_MARKER_VAR, FMT, __VA_ARGS__)
#define TM_GPU_MARKER_C_FMT(CMD_BUFFER, COLOR, FMT, ...)    TM_GPU_MARKER_NC_FMT(CMD_BUFFER, TM_GPU_MARKER_VAR, COLOR, FMT, __VA_ARGS__)

#define TM_GPU_COLLECT_STATS(CMD_BUFFER) TM_GPU_PROFILER.CollectCmdStats(CMD_BUFFER)
#pragma endregion GPU MARKERS
#else
#pragma region CPU MARKERS
#define TM_FRAME(NAME)

#define TM_BEGIN_FRAME(NAME)
#define TM_END_FRAME(NAME)

#define TM_MARKER_N(NAME, LABEL)
#define TM_MARKER_NC(NAME, COLOR, LABEL)
#define TM_MARKER_NC_FMT(NAME, COLOR, FMT, ...)
#define TM_MARKER_N_FMT(NAME, FMT, ...)

#define TM_MARKER(LABEL)
#define TM_MARKER_C(COLOR, LABEL)
#define TM_MARKER_C_FMT(COLOR, FMT, ...)
#define TM_MARKER_FMT(FMT, ...)

#define TM_TRANSIENT_MARKER_N(NAME, LABEL)
#define TM_TRANSIENT_MARKER_NC(NAME, COLOR, LABEL)
#define TM_TRANSIENT_MARKER(LABEL)
#define TM_TRANSIENT_MARKER_C(COLOR, LABEL)

#define TM_MARKER_N_TEXT(NAME, FMT, ...)
#define TM_MARKER_N_VALUE(NAME, VALUE)

#define TM_MARKER_TEXT(FMT, ...)
#define TM_MARKER_VALUE(VALUE)

#define TM_IS_MARKER_ACTIVE()
#define TM_IS_MARKER_ACTIVE_N(NAME)

#define TM_LOG_C(COLOR, TEXT)
#pragma endregion CPU MARKERS


#pragma region GPU MARKERS
#define TM_GPU_MARKER_NC(CMD_BUFFER, NAME, COLOR, LABEL)
#define TM_GPU_MARKER_N(CMD_BUFFER, NAME, LABEL)
#define TM_GPU_MARKER_NC_FMT(CMD_BUFFER, NAME, COLOR, FMT, ...)
#define TM_GPU_MARKER_N_FMT(CMD_BUFFER, NAME, FMT, ...)

#define TM_GPU_MARKER(CMD_BUFFER, LABEL)
#define TM_GPU_MARKER_C(CMD_BUFFER, COLOR, LABEL)
#define TM_GPU_MARKER_FMT(CMD_BUFFER, FMT, ...)
#define TM_GPU_MARKER_C_FMT(CMD_BUFFER, COLOR, FMT, ...)

#define TM_GPU_COLLECT_STATS(CMD_BUFFER)
#pragma endregion GPU MARKERS
#endif

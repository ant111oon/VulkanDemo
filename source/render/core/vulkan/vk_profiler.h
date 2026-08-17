#pragma once

#include "core/engine/profiler/core.h"

#ifdef ENG_PROFILING_ENABLED

#include "vk_cmd.h"

#include <tracy/TracyVulkan.hpp>


namespace vkn
{
    class Profiler
    {
        friend Profiler& GetProfiler();

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Profiler);
        ENG_DECL_CLASS_NO_MOVABLE(Profiler);

        ~Profiler();

        Profiler& Create(Device* pDevice);
        Profiler& Destroy();

        const Profiler& BeginCmdGroup(CmdBuffer& cmd, std::string_view groupName) const;
        const Profiler& BeginCmdGroup(CmdBuffer& cmd, std::string_view groupName, uint32_t color = 0x7f7f7f) const;

        const Profiler& EndCmdGroup(CmdBuffer& cmd) const;

        const Profiler& CollectCmdStats(CmdBuffer& cmd) const;

        TracyVkCtx GetTracyContext() const;

        bool IsCreated() const;

    private:
        Profiler() = default;

    private:
        Device* m_pDevice = nullptr;

        CmdPool m_cmdPool;
        CmdBuffer* m_pCmdBuffer;

        TracyVkCtx m_context = nullptr;
    };


    ENG_FORCE_INLINE Profiler& GetProfiler()
    {
        static Profiler profiler;
        return profiler;
    }


    class GpuMarker
    {
    public:
        GpuMarker(CmdBuffer& cmd, std::string_view name, uint32_t color = 0x7f7f7f);
        ~GpuMarker();

    private:
        CmdBuffer& m_cmdBuf;
    };
}


#define ENG_VKN_PROFILER_CONTEXT vkn::GetProfiler().GetTracyContext()

#pragma region Named Markers
#define ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, NAME, COLOR, LABEL)                                              \
    TracyVkNamedZoneC(ENG_VKN_PROFILER_CONTEXT, TracyConcat(_gpuMarker, NAME), CMD_BUFFER.Get(), LABEL, COLOR, true); \
    vkn::GpuMarker NAME(CMD_BUFFER, LABEL, COLOR)

#define ENG_PROFILE_GPU_SCOPED_MARKER_NC_FMT(CMD_BUFFER, NAME, COLOR, FMT, ...)                                                                 \
    char TracyConcat(_gpuMarkName,TracyLine)[eng::profile::ENG_PROFILE_MARKER_NAME_LEN] = {};                                                   \
    sprintf_s(TracyConcat(_gpuMarkName,TracyLine), eng::profile::ENG_PROFILE_MARKER_NAME_LEN - 1, FMT, __VA_ARGS__);                            \
    TracyVkZoneTransient(ENG_VKN_PROFILER_CONTEXT, TracyConcat(_gpuMarker, NAME), CMD_BUFFER.Get(), TracyConcat(_gpuMarkName,TracyLine), true); \
    vkn::GpuMarker NAME(CMD_BUFFER, TracyConcat(_gpuMarkName,TracyLine), COLOR)

#define ENG_PROFILE_GPU_SCOPED_MARKER_N(CMD_BUFFER, NAME, LABEL) \
    ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, NAME, 0x7f7f7f, LABEL)

#define ENG_PROFILE_GPU_SCOPED_MARKER_N_FMT(CMD_BUFFER, NAME, FMT, ...) \
    ENG_PROFILE_GPU_SCOPED_MARKER_NC_FMT(CMD_BUFFER, NAME, 0x7f7f7f, FMT, __VA_ARGS__)
#pragma endregion


#pragma region Unamed Markers
#define ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, COLOR, LABEL) \
    ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, TracyConcat(_gpuMarker,TracyLine), COLOR, LABEL)

#define ENG_PROFILE_GPU_SCOPED_MARKER_C_FMT(CMD_BUFFER, COLOR, FMT, ...) \
    ENG_PROFILE_GPU_SCOPED_MARKER_NC_FMT(CMD_BUFFER, TracyConcat(_gpuMarker,TracyLine), COLOR, FMT, __VA_ARGS__)

#define ENG_PROFILE_GPU_SCOPED_MARKER(CMD_BUFFER, LABEL) \
    ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, 0x7f7f7f, LABEL)

#define ENG_PROFILE_GPU_SCOPED_MARKER_FMT(CMD_BUFFER, FMT, ...) \
    ENG_PROFILE_GPU_SCOPED_MARKER_C_FMT(CMD_BUFFER, 0x7f7f7f, FMT, __VA_ARGS__)
#pragma endregion


#define ENG_PROFILE_GPU_COLLECT_STATS(CMD_BUFFER) vkn::GetProfiler().CollectCmdStats(CMD_BUFFER)
#else  
#pragma region Named Markers
#define ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, NAME, COLOR, LABEL)
#define ENG_PROFILE_GPU_SCOPED_MARKER_NC_FMT(CMD_BUFFER, NAME, COLOR, FMT, ...)
#define ENG_PROFILE_GPU_SCOPED_MARKER_N(CMD_BUFFER, NAME, LABEL)
#define ENG_PROFILE_GPU_SCOPED_MARKER_N_FMT(CMD_BUFFER, NAME, FMT, ...)
#pragma endregion


#pragma region Unamed Markers
#define ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, COLOR, LABEL)
#define ENG_PROFILE_GPU_SCOPED_MARKER_C_FMT(CMD_BUFFER, COLOR, FMT, ...)
#define ENG_PROFILE_GPU_SCOPED_MARKER(CMD_BUFFER, LABEL)
#define ENG_PROFILE_GPU_SCOPED_MARKER_FMT(CMD_BUFFER, FMT, ...)
#pragma endregion

#define ENG_PROFILE_GPU_COLLECT_STATS(CMD_BUFFER)
#endif

#pragma once

#include "core/profiler/core.h"

#ifdef ENG_PROFILING_ENABLED

#include "vk_object.h"
#include "vk_cmd.h"

#include <tracy/TracyVulkan.hpp>


namespace vkn
{
    class Profiler : public Object
    {
        friend Profiler& GetProfiler();

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Profiler);
        ENG_DECL_CLASS_NO_MOVABLE(Profiler);

        ~Profiler();

        Profiler& Create(Device* pDevice);
        Profiler& Destroy();

        const Profiler& BeginCmdGroup(CmdBuffer& cmd, std::string_view groupName) const;
        const Profiler& BeginCmdGroup(CmdBuffer& cmd, std::string_view groupName, uint32_t color = eng::ProfileColor::Grey50) const;

        const Profiler& EndCmdGroup(CmdBuffer& cmd) const;

        const Profiler& CollectCmdStats(CmdBuffer& cmd) const;

        TracyVkCtx GetTracyContext() const;

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
        GpuMarker(CmdBuffer& cmd, std::string_view name, uint32_t color);
        ~GpuMarker();

    private:
        CmdBuffer& m_cmdBuf;
    };
}


#pragma region Named Markers
#define ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, NAME, LABEL, COLOR)                                                      \
    TracyVkNamedZoneC(vkn::GetProfiler().GetTracyContext(), TracyConcat(NAME, _TRACY), CMD_BUFFER.Get(), LABEL, COLOR, true); \
    vkn::GpuMarker NAME(CMD_BUFFER, LABEL, COLOR)


#define ENG_PROFILE_GPU_SCOPED_MARKER_N(CMD_BUFFER, NAME, LABEL) \
    ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, NAME, LABEL, eng::ProfileColor::Grey51)


#define ENG_PROFILE_GPU_SCOPED_MARKER_NC_FMT(CMD_BUFFER, NAME, COLOR, FMT, ...)                                                            \
    std::array<char, 256> TracyConcat(localGPUMarkerName_,TracyLine) = {};                                                                 \
    sprintf_s(TracyConcat(localGPUMarkerName_,TracyLine).data(), TracyConcat(localGPUMarkerName_,TracyLine).size() - 1, FMT, __VA_ARGS__); \
    ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, NAME, TracyConcat(localGPUMarkerName_,TracyLine).data(), COLOR)


#define ENG_PROFILE_GPU_SCOPED_MARKER_N_FMT(CMD_BUFFER, NAME, FMT, ...) \
    ENG_PROFILE_GPU_SCOPED_MARKER_NC_FMT(CMD_BUFFER, NAME, eng::ProfileColor::Grey51, FMT, __VA_ARGS__)
#pragma endregion


#pragma region Unamed Markers
#define ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, LABEL, COLOR) \
    ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, TracyConcat(localGPUMarker_,TracyLine), LABEL, COLOR)


#define ENG_PROFILE_GPU_SCOPED_MARKER_C_FMT(CMD_BUFFER, COLOR, FMT, ...) \
    ENG_PROFILE_GPU_SCOPED_MARKER_NC_FMT(CMD_BUFFER, TracyConcat(localGPUMarker_,TracyLine), COLOR, FMT, __VA_ARGS__)


#define ENG_PROFILE_GPU_SCOPED_MARKER(CMD_BUFFER, LABEL) \
    ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, LABEL, eng::ProfileColor::Grey51)


#define ENG_PROFILE_GPU_SCOPED_MARKER_FMT(CMD_BUFFER, FMT, ...) \
    ENG_PROFILE_GPU_SCOPED_MARKER_C_FMT(CMD_BUFFER, eng::ProfileColor::Grey51, FMT, __VA_ARGS__)
#pragma endregion


#define ENG_PROFILE_GPU_COLLECT_STATS(CMD_BUFFER) vkn::GetProfiler().CollectCmdStats(CMD_BUFFER)
#else  
#pragma region Named Markers
#define ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, NAME, LABEL, COLOR)
#define ENG_PROFILE_GPU_SCOPED_MARKER_N(CMD_BUFFER, NAME, LABEL)
#define ENG_PROFILE_GPU_SCOPED_MARKER_NC_FMT(CMD_BUFFER, NAME, COLOR, FMT, ...)
#define ENG_PROFILE_GPU_SCOPED_MARKER_N_FMT(CMD_BUFFER, NAME, FMT, ...)
#pragma endregion


#pragma region Unamed Markers
#define ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, LABEL, COLOR)
#define ENG_PROFILE_GPU_SCOPED_MARKER(CMD_BUFFER, LABEL)
#define ENG_PROFILE_GPU_SCOPED_MARKER_C_FMT(CMD_BUFFER, COLOR, FMT, ...)
#define ENG_PROFILE_GPU_SCOPED_MARKER_FMT(CMD_BUFFER, FMT, ...)
#pragma endregion

#define ENG_PROFILE_GPU_COLLECT_STATS(CMD_BUFFER)
#endif

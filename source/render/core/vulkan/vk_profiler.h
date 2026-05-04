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
        const Profiler& BeginCmdGroup(CmdBuffer& cmd, std::string_view groupName, uint32_t color = eng::ProfileColor::Grey50) const;

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
#pragma endregion


#pragma region Unamed Markers
#define ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, LABEL, COLOR) \
    ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, TracyConcat(localGPUMarker_,TracyLine), LABEL, COLOR)


#define ENG_PROFILE_GPU_SCOPED_MARKER(CMD_BUFFER, LABEL) \
    ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, LABEL, eng::ProfileColor::Grey51)
#pragma endregion


#define ENG_PROFILE_GPU_COLLECT_STATS(CMD_BUFFER) vkn::GetProfiler().CollectCmdStats(CMD_BUFFER)
#else  
#pragma region Named Markers
#define ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, NAME, LABEL, COLOR)
#define ENG_PROFILE_GPU_SCOPED_MARKER_N(CMD_BUFFER, NAME, LABEL)
#pragma endregion


#pragma region Unamed Markers
#define ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, LABEL, COLOR)
#define ENG_PROFILE_GPU_SCOPED_MARKER(CMD_BUFFER, LABEL)
#pragma endregion

#define ENG_PROFILE_GPU_COLLECT_STATS(CMD_BUFFER)
#endif

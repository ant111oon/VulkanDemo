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

        const Profiler& BeginCmdGroup(CmdBuffer& cmd, const char* pGroupName) const;
        const Profiler& BeginCmdGroup(CmdBuffer& cmd, const char* pGroupName, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) const;

        const Profiler& EndCmdGroup(CmdBuffer& cmd) const;

        const Profiler& CollectCmdStats(CmdBuffer& cmd) const;

        TracyVkCtx GetTracyContext() const;

    private:
        Profiler() = default;

    private:
        Device* m_pDevice = nullptr;

        CmdPool m_cmdPool;
        CmdBuffer m_cmdBuffer;

        TracyVkCtx m_context = nullptr;

        PFN_vkCmdBeginDebugUtilsLabelEXT m_vkCmdBeginDebugUtilsLabelFunc = nullptr;
        PFN_vkCmdEndDebugUtilsLabelEXT   m_vkCmdEndDebugUtilsLabelFunc = nullptr;
    };


    ENG_FORCE_INLINE Profiler& GetProfiler()
    {
        static Profiler profiler;
        return profiler;
    }


    class GpuMarker
    {
    public:
        GpuMarker(CmdBuffer& cmd, std::string_view name, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
        ~GpuMarker();

    private:
        CmdBuffer& m_cmdBuf;
    };
}

     
#define ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, NAME, LABEL, R8, G8, B8, A8)                                                               \
    TracyVkNamedZoneC(vkn::GetProfiler().GetTracyContext(), _ENG_PROFILE_CONCAT(NAME, _T), CMD_BUFFER.Get(), LABEL, _ENG_PROFILE_MAKE_COLOR_U32(R8, G8, B8, A8), true)   \
    vkn::GpuMarker NAME(CMD_BUFFER, LABEL, uint8_t(R8), uint8_t(G8), uint8_t(B8), uint8_t(A8))

#define ENG_PROFILE_GPU_SCOPED_MARKER_N(CMD_BUFFER, NAME, LABEL) \
    ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, NAME, LABEL, 0, 0, 0, 255)

#define ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, LABEL, R8, G8, B8, A8) \
    ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, _ENG_PROFILE_CONCAT(_vkn_gpu_marker_, __LINE__), LABEL, R8, G8, B8, A8)

#define ENG_PROFILE_GPU_SCOPED_MARKER(CMD_BUFFER, LABEL) \
    ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, LABEL, 0, 0, 0, 255)

#define ENG_PROFILE_GPU_COLLECT_STATS(CMD_BUFFER) vkn::GetProfiler().CollectCmdStats(CMD_BUFFER)
#else  
#define ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, NAME, LABEL, R8, G8, B8, A8)
#define ENG_PROFILE_GPU_SCOPED_MARKER_N(CMD_BUFFER, NAME, LABEL)                 

#define ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, LABEL, R8, G8, B8, A8)
#define ENG_PROFILE_GPU_SCOPED_MARKER(CMD_BUFFER, LABEL)

#define ENG_PROFILE_GPU_COLLECT_STATS(CMD_BUFFER)
#endif

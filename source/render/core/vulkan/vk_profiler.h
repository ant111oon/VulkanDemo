#pragma once

#include "vk_object.h"
#include "vk_cmd.h"

#include "core/profiler/core.h"

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

        bool Create(Device* pDevice);
        void Destroy();

        void BeginCmdGroup(CmdBuffer& buffer, const char* pGroupName) const;
        void BeginCmdGroup(CmdBuffer& buffer, const char* pGroupName, uint8_t r, uint8_t g, uint8_t b, uint8_t a) const;

        void EndCmdGroup(CmdBuffer& buffer) const;

        TracyVkCtx Get();

    private:
        Profiler() = default;

    private:
    #ifdef ENG_PROFILING_ENABLED
        Device* m_pDevice = nullptr;

        CmdPool m_cmdPool;
        CmdBuffer m_cmdBuffer;

        TracyVkCtx m_context = nullptr;

        PFN_vkCmdBeginDebugUtilsLabelEXT m_vkCmdBeginDebugUtilsLabelFunc = nullptr;
        PFN_vkCmdEndDebugUtilsLabelEXT   m_vkCmdEndDebugUtilsLabelFunc = nullptr;
    #endif
    };


    ENG_FORCE_INLINE Profiler& GetProfiler()
    {
        static Profiler profiler;
        return profiler;
    }
}


#ifdef ENG_PROFILING_ENABLED
#define _ENG_PROFILE_GPU_ASSERT_CMD_BUFFER(CMD_BUFFER, MSG, ...) VK_ASSERT_MSG(CMD_BUFFER.IsStarted(), MSG, __VA_ARGS__) 


#define ENG_PROFILE_BEGIN_GPU_MARKER_N_SCOPE(CMD_BUFFER, NAME, LABEL) \
    {                                                                 \
        _ENG_PROFILE_GPU_ASSERT_CMD_BUFFER(CMD_BUFFER, "Attempt to begin GPU marker scope within not started command buffer: %s", \
            CMD_BUFFER.GetDebugName()); \
        TracyVkNamedZone(vkn::GetProfiler().Get(), NAME, CMD_BUFFER.Get(), LABEL, true); \
        vkn::GetProfiler().BeginCmdGroup(CMD_BUFFER, LABEL)

#define ENG_PROFILE_BEGIN_GPU_MARKER_NC_SCOPE(CMD_BUFFER, NAME, LABEL, R8, G8, B8, A8) \
    {                                                                                  \
        _ENG_PROFILE_GPU_ASSERT_CMD_BUFFER(CMD_BUFFER, "Attempt to begin GPU marker scope within not started command buffer: %s", \
            CMD_BUFFER.GetDebugName()); \
        TracyVkNamedZoneC(vkn::GetProfiler().Get(), NAME, CMD_BUFFER.Get(), LABEL, _ENG_PROFILE_MAKE_COLOR_U32(R8, G8, B8, A8), true); \
        vkn::GetProfiler().BeginCmdGroup(CMD_BUFFER, LABEL, R8, G8, B8, A8)

#define ENG_PROFILE_BEGIN_GPU_MARKER_SCOPE(CMD_BUFFER, LABEL) \
    {                                                         \
        _ENG_PROFILE_GPU_ASSERT_CMD_BUFFER(CMD_BUFFER, "Attempt to begin GPU marker scope within not started command buffer: %s", \
            CMD_BUFFER.GetDebugName()); \
        TracyVkZone(vkn::GetProfiler().Get(), CMD_BUFFER.Get(), LABEL); \
        vkn::GetProfiler().BeginCmdGroup(CMD_BUFFER, LABEL)

#define ENG_PROFILE_BEGIN_GPU_MARKER_C_SCOPE(CMD_BUFFER, LABEL, R8, G8, B8, A8) \
    {                                                                           \
        _ENG_PROFILE_GPU_ASSERT_CMD_BUFFER(CMD_BUFFER, "Attempt to begin GPU marker scope within not started command buffer: %s", \
            CMD_BUFFER.GetDebugName()); \
        TracyVkZoneC(vkn::GetProfiler().Get(), CMD_BUFFER.Get(), LABEL, _ENG_PROFILE_MAKE_COLOR_U32(R8, G8, B8, A8)); \
        vkn::GetProfiler().BeginCmdGroup(CMD_BUFFER, LABEL, R8, G8, B8, A8)

// For very short-lived events that is called frequently
#define ENG_PROFILE_BEGIN_GPU_TRANSIENT_MARKER_SCOPE(CMD_BUFFER, NAME, LABEL) \
    {                                                                         \
        _ENG_PROFILE_GPU_ASSERT_CMD_BUFFER(CMD_BUFFER, "Attempt to begin GPU marker scope within not started command buffer: %s", \
            CMD_BUFFER.GetDebugName()); \
        TracyVkZoneTransient(vkn::GetProfiler().Get(), NAME, CMD_BUFFER.Get(), LABEL, true); \
        vkn::GetProfiler().BeginCmdGroup(CMD_BUFFER, LABEL)

#define ENG_PROFILE_END_GPU_MARKER_SCOPE(CMD_BUFFER)    \
        _ENG_PROFILE_GPU_ASSERT_CMD_BUFFER(CMD_BUFFER, "Attempt to end GPU marker scope within not started command buffer: %s", \
            CMD_BUFFER.GetDebugName()); \
        vkn::GetProfiler().EndCmdGroup(CMD_BUFFER);     \
    }

#define ENG_PROFILE_GPU_COLLECT_STATS(CMD_BUFFER)                                                                         \
    {                                                                                                                     \
        _ENG_PROFILE_GPU_ASSERT_CMD_BUFFER(CMD_BUFFER, "Attempt to collect tracy GPU timings within not started command buffer: %s", \
            CMD_BUFFER.GetDebugName()); \
        TracyVkCollect(vkn::GetProfiler().Get(), CMD_BUFFER.Get()); \
    }
#else
#define ENG_PROFILE_BEGIN_GPU_MARKER_N_SCOPE(CMD_BUFFER, NAME, LABEL);
#define ENG_PROFILE_BEGIN_GPU_MARKER_NC_SCOPE(CMD_BUFFER, NAME, LABEL, R8, G8, B8, A8);
#define ENG_PROFILE_BEGIN_GPU_MARKER_SCOPE(CMD_BUFFER, LABEL)
#define ENG_PROFILE_BEGIN_GPU_MARKER_C_SCOPE(CMD_BUFFER, LABEL, R8, G8, B8, A8)
#define ENG_PROFILE_BEGIN_GPU_TRANSIENT_MARKER_SCOPE(CMD_BUFFER, NAME, LABEL)
#define ENG_PROFILE_END_GPU_MARKER_SCOPE(CMD_BUFFER)
#define ENG_PROFILE_GPU_COLLECT_STATS(CMD_BUFFER)
#endif

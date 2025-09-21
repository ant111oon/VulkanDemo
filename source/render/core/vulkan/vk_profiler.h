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

        TracyVkCtx Get();

    private:
        Profiler() = default;

    private:
    #ifdef ENG_PROFILING_ENABLED
        Device* m_pDevice = nullptr;

        CmdPool m_cmdPool;
        CmdBuffer m_cmdBuffer;

        TracyVkCtx m_context = nullptr;
    #endif
    };


    ENG_FORCE_INLINE Profiler& GetProfiler()
    {
        static Profiler profiler;
        return profiler;
    }
}


#if defined(ENG_PROFILING_ENABLED)
#define ENG_PROFILE_GPU_SCOPED_MARKER_N(CMD_BUFFER, NAME, LABEL)                    TracyVkNamedZone(vkn::GetProfiler().Get(), NAME, CMD_BUFFER.Get(), LABEL, true)
#define ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, NAME, LABEL, R8, G8, B8, A8)   TracyVkNamedZoneC(vkn::GetProfiler().Get(), NAME, CMD_BUFFER.Get(), LABEL, _ENG_PROFILE_MAKE_COLOR_U32(R8, G8, B8, A8), true)

#define ENG_PROFILE_GPU_SCOPED_MARKER(CMD_BUFFER, LABEL)                    TracyVkZone(vkn::GetProfiler().Get(), CMD_BUFFER.Get(), LABEL)
#define ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, LABEL, R8, G8, B8, A8)  TracyVkZoneC(vkn::GetProfiler().Get(), CMD_BUFFER.Get(), LABEL, _ENG_PROFILE_MAKE_COLOR_U32(R8, G8, B8, A8))

// For very short-lived events that is called frequently
#define ENG_PROFILE_GPU_TRANSIENT_SCOPED_MARKER(CMD_BUFFER, NAME) TracyVkZoneTransient(vkn::GetProfiler().Get(), NAME, CMD_BUFFER.Get(), #NAME, true)


#define ENG_PROFILE_BEGIN_GPU_MARKER_N_SCOPE(CMD_BUFFER, NAME, LABEL)   \
    {                                                                   \
        ENG_PROFILE_GPU_SCOPED_MARKER_N(CMD_BUFFER, NAME, LABEL)

#define ENG_PROFILE_BEGIN_GPU_MARKER_NC_SCOPE(CMD_BUFFER, NAME, LABEL, R8, G8, B8, A8)  \
    {                                                                                   \
        ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, NAME, LABEL, R8, G8, B8, A8)

#define ENG_PROFILE_BEGIN_GPU_MARKER_SCOPE(CMD_BUFFER, LABEL)   \
    {                                                           \
        ENG_PROFILE_GPU_SCOPED_MARKER(CMD_BUFFER, LABEL)

#define ENG_PROFILE_BEGIN_GPU_MARKER_C_SCOPE(CMD_BUFFER, LABEL, R8, G8, B8, A8) \
    {                                                                           \
        ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, LABEL, R8, G8, B8, A8)

#define ENG_PROFILE_BEGIN_GPU_TRANSIENT_MARKER_SCOPE(CMD_BUFFER, NAME)  \
    {                                                                   \
        ENG_PROFILE_GPU_TRANSIENT_SCOPED_MARKER(CMD_BUFFER, NAME)


#define ENG_PROFILE_END_GPU_MARKER_SCOPE() }


#define ENG_PROFILE_GPU_COLLECT_STATS(CMD_BUFFER) TracyVkCollect(vkn::GetProfiler().Get(), CMD_BUFFER.Get())
#else
#define ENG_PROFILE_GPU_SCOPED_MARKER_N(CMD_BUFFER, NAME, LABEL)
#define ENG_PROFILE_GPU_SCOPED_MARKER_NC(CMD_BUFFER, NAME, LABEL, R8, G8, B8, A8)

#define ENG_PROFILE_GPU_SCOPED_MARKER(CMD_BUFFER, LABEL)
#define ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, LABEL, R8, G8, B8, A8)

// For very short-lived events that is called frequently
#define ENG_PROFILE_GPU_TRANSIENT_SCOPED_MARKER(CMD_BUFFER, NAME)

#define ENG_PROFILE_BEGIN_GPU_MARKER_N_SCOPE(CMD_BUFFER, NAME, LABEL)

#define ENG_PROFILE_BEGIN_GPU_MARKER_NC_SCOPE(CMD_BUFFER, NAME, LABEL, R8, G8, B8, A8)

#define ENG_PROFILE_BEGIN_GPU_MARKER_SCOPE(CMD_BUFFER, LABEL)

#define ENG_PROFILE_BEGIN_GPU_MARKER_C_SCOPE(CMD_BUFFER, LABEL, R8, G8, B8, A8)

#define ENG_PROFILE_BEGIN_GPU_TRANSIENT_MARKER_SCOPE(CMD_BUFFER, NAME)

#define ENG_PROFILE_END_GPU_MARKER_SCOPE()

#define ENG_PROFILE_GPU_COLLECT_STATS(CMD_BUFFER)
#endif

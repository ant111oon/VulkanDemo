#include "pch.h"

#include "vk_profiler.h"

#include "vk_device.h"


namespace vkn
{
    Profiler::~Profiler()
    {
        CORE_ASSERT_MSG(!IsCreated(), "Need to destroy vkn::Profiler manually");
    }


    bool Profiler::Create(Device* pDevice)
    {
    #ifdef ENG_PROFILING_ENABLED
        if (IsCreated()) {
            VK_LOG_WARN("Vulkan profiler is already created");
            return false;
        }

        CORE_ASSERT(pDevice && pDevice->IsCreated());

        m_pDevice = pDevice;

        CmdPoolCreateInfo cmdPoolCreateInfo = {};
        cmdPoolCreateInfo.pDevice = m_pDevice;
        cmdPoolCreateInfo.queueFamilyIndex = m_pDevice->GetQueueFamilyIndex();
        cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        m_cmdPool.Create(cmdPoolCreateInfo);
        CORE_ASSERT(m_cmdPool.IsCreated());
        m_cmdPool.SetDebugName("PROFILER_CMD_POOL");

        m_cmdBuffer = m_cmdPool.AllocCmdBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        CORE_ASSERT(m_cmdBuffer.IsCreated());
        m_cmdBuffer.SetDebugName("PROFILER_CMD_BUFFER");

        m_context = TracyVkContext(m_pDevice->GetPhysDevice()->Get(), m_pDevice->Get(), m_pDevice->GetQueue(), m_cmdBuffer.Get());

        const bool isCreated = m_context != nullptr;
        CORE_ASSERT_MSG(isCreated, "Failed to create Vulkan profiler");

        const char* pContextName = "Vulkan Queue";
        TracyVkContextName(m_context, pContextName, strlen(pContextName));

        SetCreated(isCreated);

        return isCreated;
    #else
        SetCreated(true);
        return true;
    #endif
    }


    void Profiler::Destroy()
    {
    #ifdef ENG_PROFILING_ENABLED
        if (!IsCreated()) {
            return;
        }

        TracyVkDestroy(m_context);
        m_context = nullptr;

        m_cmdPool.FreeCmdBuffer(m_cmdBuffer);
        m_cmdPool.Destroy();

        m_pDevice = nullptr;

        Object::Destroy();        
    #endif
    }


    TracyVkCtx Profiler::Get()
    {
    #ifdef ENG_PROFILING_ENABLED
        CORE_ASSERT(IsCreated());
        return m_context;
    #else
        return nullptr;
    #endif
    }
}
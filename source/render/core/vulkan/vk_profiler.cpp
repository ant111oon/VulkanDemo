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
            CORE_LOG_WARN("Vulkan profiler is already created");
            return false;
        }

        CORE_ASSERT(pDevice && pDevice->IsCreated());

        m_pDevice = pDevice;

        CmdPoolCreateInfo cmdPoolCreateInfo = {};
        cmdPoolCreateInfo.pDevice = m_pDevice;
        cmdPoolCreateInfo.queueFamilyIndex = m_pDevice->GetQueueFamilyIndex();
        cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        m_vkCmdBeginDebugUtilsLabelFunc = (PFN_vkCmdBeginDebugUtilsLabelEXT)m_pDevice->GetPhysDevice()->GetInstance()->GetProcAddr("vkCmdBeginDebugUtilsLabelEXT");
        m_vkCmdEndDebugUtilsLabelFunc = (PFN_vkCmdEndDebugUtilsLabelEXT)m_pDevice->GetPhysDevice()->GetInstance()->GetProcAddr("vkCmdEndDebugUtilsLabelEXT");

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

        m_vkCmdBeginDebugUtilsLabelFunc = nullptr;
        m_vkCmdEndDebugUtilsLabelFunc = nullptr;

        Object::Destroy();        
    #endif
    }


    void Profiler::BeginCmdGroup(CmdBuffer& buffer, const char* pGroupName) const
    {
        BeginCmdGroup(buffer, pGroupName, 168, 168, 168, 255);
    }


    void Profiler::BeginCmdGroup(CmdBuffer& buffer, const char* pGroupName, uint8_t r, uint8_t g, uint8_t b, uint8_t a) const
    {
    #ifdef ENG_PROFILING_ENABLED
        CORE_ASSERT(IsCreated());
        CORE_ASSERT(pGroupName != nullptr);

        _ENG_PROFILE_GPU_ASSERT_CMD_BUFFER(buffer, "Attempt to begin GPU command group within not started command buffer: %s", buffer.GetDebugName());

        VkDebugUtilsLabelEXT dbgLabel = {};
        dbgLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        dbgLabel.pLabelName = pGroupName;
        dbgLabel.color[0] = static_cast<float>(r) / 255.f;
        dbgLabel.color[1] = static_cast<float>(g) / 255.f;
        dbgLabel.color[2] = static_cast<float>(b) / 255.f;
        dbgLabel.color[3] = static_cast<float>(a) / 255.f;

        m_vkCmdBeginDebugUtilsLabelFunc(buffer.Get(), &dbgLabel);
    #endif
    }


    void Profiler::EndCmdGroup(CmdBuffer& buffer) const
    {
    #ifdef ENG_PROFILING_ENABLED
        CORE_ASSERT(IsCreated());
        _ENG_PROFILE_GPU_ASSERT_CMD_BUFFER(buffer, "Attempt to end GPU command group within not started command buffer: %s", buffer.GetDebugName());
    
        m_vkCmdEndDebugUtilsLabelFunc(buffer.Get());
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
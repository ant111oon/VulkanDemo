#include "pch.h"

#include "vk_profiler.h"

#ifdef ENG_PROFILING_ENABLED


namespace vkn
{
    static PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabel = nullptr;
    static PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabel = nullptr;


    Profiler::~Profiler()
    {
        Destroy();
    }


    Profiler& Profiler::Create(Device* pDevice)
    {
        if (IsCreated()) {
            CORE_LOG_WARN("Recreation of Vulkan profiler");
            Destroy();
        }

        CORE_ASSERT(pDevice && pDevice->IsCreated());

        m_pDevice = pDevice;

        CmdPoolCreateInfo cmdPoolCreateInfo = {};
        cmdPoolCreateInfo.pDevice = m_pDevice;
        cmdPoolCreateInfo.queueFamilyIndex = m_pDevice->GetQueue().GetFamilyIndex();
        cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolCreateInfo.size = 1;

        Instance& inst = m_pDevice->GetPhysDevice().GetInstance();

        if (vkCmdBeginDebugUtilsLabel == nullptr) {
            vkCmdBeginDebugUtilsLabel = (PFN_vkCmdBeginDebugUtilsLabelEXT)inst.GetProcAddr("vkCmdBeginDebugUtilsLabelEXT");
        }

        if (vkCmdEndDebugUtilsLabel == nullptr) {
            vkCmdEndDebugUtilsLabel = (PFN_vkCmdEndDebugUtilsLabelEXT)inst.GetProcAddr("vkCmdEndDebugUtilsLabelEXT");
        }

        m_cmdPool.Create(cmdPoolCreateInfo);
        CORE_ASSERT(m_cmdPool.IsCreated());
        m_cmdPool.SetDebugName("PROFILER_CMD_POOL");

        m_pCmdBuffer = m_cmdPool.AllocCmdBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        m_pCmdBuffer->SetDebugName("PROFILER_CMD_BUFFER");

        m_context = TracyVkContext(m_pDevice->GetPhysDevice().Get(), m_pDevice->Get(), m_pDevice->GetQueue().Get(), m_pCmdBuffer->Get());

        CORE_ASSERT_MSG(m_context != nullptr, "Failed to create Vulkan profiler");

        const char* pContextName = "Vulkan Queue";
        TracyVkContextName(m_context, pContextName, strlen(pContextName));

        SetCreated(true);

        return *this;
    }


    Profiler& Profiler::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        TracyVkDestroy(m_context);
        m_context = nullptr;

        m_cmdPool.FreeCmdBuffer(*m_pCmdBuffer);
        m_cmdPool.Destroy();

        m_pDevice = nullptr;

        Object::Destroy();

        return *this;
    }


    const Profiler& Profiler::BeginCmdGroup(CmdBuffer& cmd, std::string_view groupName) const
    {
        BeginCmdGroup(cmd, groupName, eng::ProfileColor::Grey51);
        return *this;
    }


    const Profiler& Profiler::BeginCmdGroup(CmdBuffer& cmd, std::string_view groupName, uint32_t color) const
    {
        CORE_ASSERT(IsCreated());

        VK_ASSERT_MSG(cmd.IsStarted(), "Attempt to begin GPU command group within not started command buffer: %s", cmd.GetDebugName());

        VkDebugUtilsLabelEXT dbgLabel = {};
        dbgLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        dbgLabel.pLabelName = groupName.data();
        dbgLabel.color[0] = static_cast<float>((color >> 16) & 0xFF) / 255.f;
        dbgLabel.color[1] = static_cast<float>((color >> 8) & 0xFF)  / 255.f;
        dbgLabel.color[2] = static_cast<float>(color & 0xFF)         / 255.f;
        dbgLabel.color[3] = 1.f;

        vkCmdBeginDebugUtilsLabel(cmd.Get(), &dbgLabel);

        return *this;
    }


    const Profiler& Profiler::EndCmdGroup(CmdBuffer& cmd) const
    {
        CORE_ASSERT(IsCreated());
        VK_ASSERT_MSG(cmd.IsStarted(), "Attempt to end GPU marker scope within not started command buffer: %s", cmd.GetDebugName());
    
        vkCmdEndDebugUtilsLabel(cmd.Get());

        return *this;
    }


    const Profiler& Profiler::CollectCmdStats(CmdBuffer& cmd) const
    {
        VK_ASSERT_MSG(cmd.IsStarted(), "Attempt to collect tracy GPU timings within not started/ended command buffer: %s", cmd.GetDebugName());
        TracyVkCollect(GetProfiler().GetTracyContext(), cmd.Get());

        return *this;
    }


    TracyVkCtx Profiler::GetTracyContext() const
    {
        CORE_ASSERT(IsCreated());
        return m_context;
    }
    

    GpuMarker::GpuMarker(CmdBuffer& cmd, std::string_view name, uint32_t color)
        : m_cmdBuf(cmd)
    {
        VK_ASSERT_MSG(cmd.IsStarted(), "Attempt to begin GPU marker scope within not started command buffer: %s", cmd.GetDebugName());
        vkn::GetProfiler().BeginCmdGroup(m_cmdBuf, name, color);
    }


    GpuMarker::~GpuMarker()
    {
        vkn::GetProfiler().EndCmdGroup(m_cmdBuf);
    }
}

#endif
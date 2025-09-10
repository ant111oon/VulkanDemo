#include "pch.h"

#include "vk_cmd.h"


namespace vkn
{
    CmdBuffer::CmdBuffer(CmdBuffer&& cmdBuffer) noexcept
    {
        *this = std::move(cmdBuffer);
    }


    CmdBuffer& CmdBuffer::operator=(CmdBuffer&& cmdBuffer) noexcept
    {
        if (this == &cmdBuffer) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }

        Object::operator=(std::move(cmdBuffer));

        std::swap(m_pOwner, cmdBuffer.m_pOwner);
        std::swap(m_cmdBuffer, cmdBuffer.m_cmdBuffer);

        return *this;
    }


    void CmdBuffer::Begin(const VkCommandBufferBeginInfo& beginInfo) const
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkBeginCommandBuffer(m_cmdBuffer, &beginInfo));
    }
    
    
    void CmdBuffer::End() const
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkEndCommandBuffer(m_cmdBuffer));
    }


    void CmdBuffer::Reset(VkCommandBufferResetFlags flags)
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkResetCommandBuffer(m_cmdBuffer, flags));
    }


    void CmdBuffer::SetDebugName(const char* pName)
    {
        Object::SetDebugName(m_pOwner->GetDevice()->Get(), (uint64_t)m_cmdBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER, pName);
    }


    const char* CmdBuffer::GetDebugName() const
    {
        return Object::GetDebugName("CommandBuffer");
    }

    
    Device* CmdBuffer::GetDevice() const
    {
        return m_pOwner->GetDevice();
    }


    CmdBuffer::CmdBuffer(CmdPool* pOwnerPool, VkCommandBufferLevel level)
    {
        Allocate(pOwnerPool, level);
    }


    bool CmdBuffer::Allocate(CmdPool* pOwnerPool, VkCommandBufferLevel level)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Command buffer %s is already allocated", GetDebugName());
            return false;
        }

        VK_ASSERT(pOwnerPool && pOwnerPool->IsCreated());

        VkDevice vkDevice = pOwnerPool->GetDevice()->Get();
        VkCommandPool vkCmdPool = pOwnerPool->Get();

        VkCommandBufferAllocateInfo cmdBufferAllocInfo = {};
        cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufferAllocInfo.commandPool = vkCmdPool;
        cmdBufferAllocInfo.level = level;
        cmdBufferAllocInfo.commandBufferCount = 1;

        m_cmdBuffer = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateCommandBuffers(vkDevice, &cmdBufferAllocInfo, &m_cmdBuffer));

        const bool isCreated = m_cmdBuffer != VK_NULL_HANDLE;
        VK_ASSERT(isCreated);

        m_pOwner = pOwnerPool;

        SetCreated(isCreated);

        return isCreated;
    }


    void CmdBuffer::Free()
    {
        if (!IsCreated()) {
            return;
        }

        Object::Destroy();

        VkDevice vkDevice = GetDevice()->Get();

        vkFreeCommandBuffers(vkDevice, m_pOwner->Get(), 1, &m_cmdBuffer);
        m_cmdBuffer = VK_NULL_HANDLE;

        m_pOwner = nullptr;
    }


    CmdPool::CmdPool(const CmdPoolCreateInfo& info)
    {
        Create(info);
    }


    CmdPool::CmdPool(CmdPool&& pool) noexcept
    {
        *this = std::move(pool);
    }


    CmdPool& CmdPool::operator=(CmdPool&& pool) noexcept
    {
        if (this == &pool) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }

        Object::operator=(std::move(pool));

        std::swap(m_pDevice, pool.m_pDevice);
        std::swap(m_pool, pool.m_pool);

        return *this;
    }


    bool CmdPool::Create(const CmdPoolCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Command pool %s is already created", GetDebugName());
            return false;
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());

        VkDevice vkDevice = info.pDevice->Get();

        VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
        cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolCreateInfo.flags = info.flags;
        cmdPoolCreateInfo.queueFamilyIndex = info.queueFamilyIndex;

        m_pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateCommandPool(vkDevice, &cmdPoolCreateInfo, nullptr, &m_pool));

        const bool isCreated =  m_pool != VK_NULL_HANDLE;
        VK_ASSERT(isCreated);

        m_pDevice = info.pDevice;

        SetCreated(isCreated);

        return isCreated;
    }


    void CmdPool::Destroy()
    {
        if (!IsCreated()) {
            return;
        }

        Object::Destroy();

        VkDevice vkDevice = m_pDevice->Get();

        vkDestroyCommandPool(vkDevice, m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;

        m_pDevice = nullptr;
    }


    void CmdPool::Reset(VkCommandPoolResetFlags flags)
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkResetCommandPool(m_pDevice->Get(), m_pool, flags));
    }


    CmdBuffer CmdPool::AllocCmdBuffer(VkCommandBufferLevel level)
    {
        VK_ASSERT(IsCreated());
        return CmdBuffer(this, level);
    }


    void CmdPool::FreeCmdBuffer(CmdBuffer& cmdBuffer)
    {
        VK_ASSERT(IsCreated());
        cmdBuffer.Free();
    }
}
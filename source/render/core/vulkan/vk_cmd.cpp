#include "pch.h"

#include "vk_cmd.h"
#include "vk_query.h"
#include "vk_buffer.h"


namespace vkn
{
    #define VK_CHECK_CMD_BUFFER_STARTED(CMD_BUFFER_PTR) \
        VK_ASSERT_MSG(CMD_BUFFER_PTR->IsStarted(), "Cmd Buffer \'%s\' is not started", CMD_BUFFER_PTR->GetDebugName())

    #define VK_CHECK_CMD_BUFFER_RENDERING_STARTED(CMD_BUFFER_PTR)   \
        VK_CHECK_CMD_BUFFER_STARTED(CMD_BUFFER_PTR);                \
        VK_ASSERT_MSG(CMD_BUFFER_PTR->IsRenderingStarted(), "Cmd Buffer \'%s\' rendering is not started", CMD_BUFFER_PTR->GetDebugName())

    
    bool CmdBuffer::IsValid() const
    {
        return m_pOwner ? (m_pOwner->IsCreated() && IsCreated()) : false;
    }


    CmdBuffer::CmdBuffer(CmdBuffer&& cmdBuffer) noexcept
    {
        *this = std::move(cmdBuffer);
    }


    CmdBuffer::~CmdBuffer()
    {
        Destroy();
    }


    CmdBuffer& CmdBuffer::operator=(CmdBuffer&& cmdBuffer) noexcept
    {
        if (this == &cmdBuffer) {
            return *this;
        }

        if (IsValid()) {
            Destroy();
        }

        Object::operator=(std::move(cmdBuffer));

        std::swap(m_pOwner, cmdBuffer.m_pOwner);
        std::swap(m_cmdBuffer, cmdBuffer.m_cmdBuffer);
        std::swap(m_state, cmdBuffer.m_state);

        return *this;
    }


    CmdBuffer& CmdBuffer::Begin(const VkCommandBufferBeginInfo& beginInfo)
    {
        VK_ASSERT(IsValid());
        VK_ASSERT(!IsStarted());

        VK_CHECK(vkBeginCommandBuffer(m_cmdBuffer, &beginInfo));

        m_state.set(FLAG_IS_STARTED, true);

        return *this;
    }
    
    
    void CmdBuffer::End()
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        VK_CHECK(vkEndCommandBuffer(m_cmdBuffer));

        m_state.set(FLAG_IS_STARTED, false);
    }


    CmdBuffer& CmdBuffer::CmdPipelineBarrier2(const VkDependencyInfo& depInfo)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        vkCmdPipelineBarrier2(m_cmdBuffer, &depInfo);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdResetQueryPool(QueryPool& queryPool, uint32_t firstQuery, uint32_t queryCount)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);
        VK_ASSERT(firstQuery + queryCount <= queryPool.GetQueryCount());

        vkCmdResetQueryPool(m_cmdBuffer, queryPool.Get(), firstQuery, queryCount);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdResetQueryPool(QueryPool& queryPool)
    {
        return CmdResetQueryPool(queryPool, 0, queryPool.GetQueryCount());
    }


    CmdBuffer& CmdBuffer::CmdWriteTimestamp(QueryPool& queryPool, VkPipelineStageFlags2 stage, uint32_t queryIndex)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);
        VK_ASSERT(queryPool.IsQueryIndexValid(queryIndex));

        vkCmdWriteTimestamp2(m_cmdBuffer, stage, queryPool.Get(), queryIndex);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdBeginRendering(const VkRenderingInfo& renderingInfo)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);
        VK_ASSERT(!IsRenderingStarted());

        vkCmdBeginRendering(m_cmdBuffer, &renderingInfo);

        m_state.set(FLAG_IS_RENDERING_STARTED, true);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdEndRendering()
    {
        VK_CHECK_CMD_BUFFER_RENDERING_STARTED(this);

        vkCmdEndRendering(m_cmdBuffer);

        m_state.set(FLAG_IS_RENDERING_STARTED, false);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdSetViewport(uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        vkCmdSetViewport(m_cmdBuffer, firstViewport, viewportCount, pViewports);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdSetScissor(uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        vkCmdSetScissor(m_cmdBuffer, firstScissor, scissorCount, pScissors);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdBindIndexBuffer(vkn::Buffer& idxBuffer, VkDeviceSize offset, VkIndexType idxType)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        vkCmdBindIndexBuffer(m_cmdBuffer, idxBuffer.Get(), offset, idxType);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        VK_CHECK_CMD_BUFFER_RENDERING_STARTED(this);

        vkCmdDraw(m_cmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
    {
        VK_CHECK_CMD_BUFFER_RENDERING_STARTED(this);

        vkCmdDrawIndexed(m_cmdBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);

        return *this;
    }


    void CmdBuffer::Reset(VkCommandBufferResetFlags flags)
    {
        VK_ASSERT(IsValid());
        VK_CHECK(vkResetCommandBuffer(m_cmdBuffer, flags));
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
        VK_ASSERT(pOwnerPool->IsCreated());

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
        if (!IsValid()) {
            return;
        }

        VkDevice vkDevice = GetDevice()->Get();

        vkFreeCommandBuffers(vkDevice, m_pOwner->Get(), 1, &m_cmdBuffer);
        m_cmdBuffer = VK_NULL_HANDLE;

        m_pOwner = nullptr;

        m_state.reset();

        Object::Destroy();
    }


    CmdPool::CmdPool(const CmdPoolCreateInfo& info)
    {
        Create(info);
    }


    CmdPool::CmdPool(CmdPool&& pool) noexcept
    {
        *this = std::move(pool);
    }


    CmdPool::~CmdPool()
    {
        Destroy();
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


    const char* CmdPool::GetDebugName() const
    {
        return Object::GetDebugName("CommandBuffer");
    }
}
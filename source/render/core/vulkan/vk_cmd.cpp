#include "pch.h"

#include "vk_cmd.h"
#include "vk_query.h"
#include "vk_buffer.h"
#include "vk_texture.h"
#include "vk_swapchain.h"


namespace vkn
{
    #define VK_CHECK_CMD_BUFFER_STARTED(CMD_BUFFER_PTR) \
        VK_ASSERT_MSG(CMD_BUFFER_PTR->IsStarted(), "Cmd Buffer \'%s\' is not started", CMD_BUFFER_PTR->GetDebugName())

    #define VK_CHECK_CMD_BUFFER_RENDERING_STARTED(CMD_BUFFER_PTR)   \
        VK_CHECK_CMD_BUFFER_STARTED(CMD_BUFFER_PTR);                \
        VK_ASSERT_MSG(CMD_BUFFER_PTR->IsRenderingStarted(), "Cmd Buffer \'%s\' rendering is not started", CMD_BUFFER_PTR->GetDebugName())


    static VkImageMemoryBarrier2 CreateImageMemoryBarrier2Data(
        VkImage image, 
        VkPipelineStageFlags2 srcStageMask, 
        VkPipelineStageFlags2 dstStageMask, 
        VkAccessFlags2 srcAccessMask, 
        VkAccessFlags2 dstAccessMask, 
        VkImageLayout srcLayout,
        VkImageLayout dstLayout,
        VkImageAspectFlags aspectMask,
        uint32_t baseMipLevel,
        uint32_t mipCount,
        uint32_t baseArrayLayer,
        uint32_t layerCount
    ) {
        VkImageMemoryBarrier2 barrier = {};

        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.image = image;
        barrier.srcStageMask = srcStageMask;
        barrier.srcAccessMask = srcAccessMask;
        barrier.oldLayout = srcLayout;
        barrier.dstStageMask = dstStageMask;
        barrier.dstAccessMask = dstAccessMask;
        barrier.newLayout = dstLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = aspectMask;
        barrier.subresourceRange.baseMipLevel = baseMipLevel;
        barrier.subresourceRange.levelCount = mipCount;
        barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
        barrier.subresourceRange.layerCount = layerCount;

        return barrier;
    }


    BarrierList& BarrierList::Begin()
    {
        VK_ASSERT_MSG(!IsStarted(), "Attempt to begin already started barrier list");
        
        m_state.set(FLAG_IS_STARTED, true);
        
        return *this;
    }


    BarrierList& BarrierList::Reset()
    {
        m_bufferBarriers.clear();
        m_textureBarriers.clear();

        m_state = {};

        return *this;
    }


    BarrierList& BarrierList::End()
    {
        VK_ASSERT_MSG(IsStarted(), "Attempt to end barrier list which wasn't started");
        
        Reset();
        
        return *this;
    }


    const BarrierList::BufferBarrierData& BarrierList::GetBufferBarrierByIdx(size_t i) const
    {
        VK_ASSERT_MSG(IsStarted(), "Attempt to get element from barrier list which wasn't started");
        VK_ASSERT(i < GetBufferBarriersCount());

        return m_bufferBarriers.at(i);
    }


    const BarrierList::TextureBarrierData& BarrierList::GetTextureBarrierByIdx(size_t i) const
    {
        VK_ASSERT_MSG(IsStarted(), "Attempt to get element from barrier list which wasn't started");
        VK_ASSERT(i < GetTextureBarriersCount());

        return m_textureBarriers.at(i);
    }


    const BarrierList::SCTextureBarrierData& BarrierList::GetSCTextureBarrierByIdx(size_t i) const
    {
        VK_ASSERT_MSG(IsStarted(), "Attempt to get element from barrier list which wasn't started");
        VK_ASSERT(i < GetSCTextureBarriersCount());

        return m_scTextureBarriers.at(i);
    }


    BarrierList& BarrierList::AddBufferBarrier(Buffer& buffer, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask, VkDeviceSize offset, VkDeviceSize size)
    {
        VK_ASSERT_MSG(IsStarted(), "Attempt to add barrier in barrier list which wasn't started");
        VK_ASSERT(buffer.IsCreated());

        m_bufferBarriers.emplace_back(BufferBarrierData{ &buffer, dstStageMask, dstAccessMask, offset, size });

        return *this;
    }


    BarrierList& BarrierList::AddTextureBarrier(Texture& texture, VkImageLayout dstLayout, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask,
        VkImageAspectFlags aspectMask, uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount
    ) {
        VK_ASSERT_MSG(IsStarted(), "Attempt to add barrier in barrier list which wasn't started");
        VK_ASSERT(texture.IsCreated());

        m_textureBarriers.emplace_back(
            TextureBarrierData{ 
                dstLayout, 
                dstStageMask, 
                dstAccessMask, 
                aspectMask, 
                baseMip, 
                mipCount == VK_REMAINING_MIP_LEVELS ? texture.GetMipCount() : mipCount, 
                baseLayer, 
                layerCount == VK_REMAINING_ARRAY_LAYERS ? texture.GetLayerCount() : layerCount, 
                &texture
            }
        );

        return *this;
    }


    BarrierList& BarrierList::AddTextureBarrier(SCTexture& texture, VkImageLayout dstLayout, VkPipelineStageFlags2 dstStageMask,
        VkAccessFlags2 dstAccessMask, VkImageAspectFlags aspectMask
    ) {
        VK_ASSERT_MSG(IsStarted(), "Attempt to add barrier in barrier list which wasn't started");
        VK_ASSERT(texture.IsCreated());

        m_scTextureBarriers.emplace_back(
            SCTextureBarrierData{ dstLayout, dstStageMask, dstAccessMask, aspectMask, &texture }
        );

        return *this;
    }

    
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
        
        m_barrierList = std::move(cmdBuffer.m_barrierList);
        cmdBuffer.m_barrierList.Reset();

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
    
    
    CmdBuffer& CmdBuffer::End()
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);
        VK_ASSERT_MSG(!m_barrierList.IsStarted(), "Attempt to end command buffer with started buffer barrier list");

        VK_CHECK(vkEndCommandBuffer(m_cmdBuffer));

        m_state.set(FLAG_IS_STARTED, false);

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


    CmdBuffer& CmdBuffer::CmdSetDepthCompareOp(VkCompareOp op)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        vkCmdSetDepthCompareOp(m_cmdBuffer, op);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdSetDepthWriteEnable(VkBool32 enabled)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        vkCmdSetDepthWriteEnable(m_cmdBuffer, enabled);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdDispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        vkCmdDispatch(m_cmdBuffer, groupCountX, groupCountY, groupCountZ);

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


    CmdBuffer& CmdBuffer::CmdDrawIndexedIndirect(Buffer& buffer, VkDeviceSize offset, Buffer& countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
    {
        VK_CHECK_CMD_BUFFER_RENDERING_STARTED(this);

        vkCmdDrawIndexedIndirectCount(m_cmdBuffer, buffer.Get(), offset, countBuffer.Get(), countBufferOffset, maxDrawCount, stride);

        return *this;
    }


    BarrierList& CmdBuffer::GetBarrierList()
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        return m_barrierList;
    }


    BarrierList& CmdBuffer::BeginBarrierList()
    {
        return GetBarrierList().Begin();
    }


    CmdBuffer& CmdBuffer::CmdPushBarrierList()
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        VK_ASSERT_MSG(m_barrierList.IsStarted(), "Attempt to push buffer barrier list which wasn't started");

        static std::vector<VkBufferMemoryBarrier2> bufferBarriers(m_barrierList.GetBufferBarriersCount());
        bufferBarriers.clear();

        for (size_t i = 0; i < m_barrierList.GetBufferBarriersCount(); ++i) {
            const BarrierList::BufferBarrierData& data = m_barrierList.GetBufferBarrierByIdx(i);

            VkBufferMemoryBarrier2 barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            barrier.buffer = data.pBuffer->Get();
            barrier.srcStageMask = data.pBuffer->GetStageMask();
            barrier.srcAccessMask = data.pBuffer->GetAccessMask();
            barrier.dstStageMask = data.dstStageMask;
            barrier.dstAccessMask = data.dstAccessMask;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.offset = data.offset;
            barrier.size = data.size;

            data.pBuffer->Transit(data.dstStageMask, data.dstAccessMask);

            bufferBarriers.emplace_back(barrier);
        }

        static std::vector<VkImageMemoryBarrier2> textureBarriers(m_barrierList.GetTextureBarriersCount() + m_barrierList.GetSCTextureBarriersCount());
        textureBarriers.clear();

        for (size_t i = 0; i < m_barrierList.GetTextureBarriersCount(); ++i) {
            const BarrierList::TextureBarrierData& data = m_barrierList.GetTextureBarrierByIdx(i);

            Texture* pTexture = data.pTexture;
            const Texture::AccessState& currState = pTexture->GetAccessState(data.baseLayer, data.baseMip);

        #ifdef ENG_BUILD_DEBUG
            for (uint32_t layer = 0; layer < data.layerCount; ++layer) {
                for (uint32_t mip = 0; mip < data.mipCount; ++mip) {
                    VK_ASSERT_MSG(currState == pTexture->GetAccessState(data.baseLayer + layer, data.baseMip + mip),
                        "Texture %s has different access state fro required layers and mips", pTexture->GetDebugName());
                }
            }
        #endif

            VkImageMemoryBarrier2 barrier = CreateImageMemoryBarrier2Data(
                pTexture->Get(),
                currState.stageMask, data.dstStageMask,
                currState.accessMask, data.dstAccessMask,
                currState.layout, data.dstLayout,
                data.dstAspectMask, data.baseMip, data.mipCount, data.baseLayer, data.layerCount
            );

            pTexture->Transit(data.baseMip, data.mipCount, data.baseLayer, data.layerCount, 
                data.dstLayout, data.dstStageMask, data.dstAccessMask);

            textureBarriers.emplace_back(barrier);
        }

        for (size_t i = 0; i < m_barrierList.GetSCTextureBarriersCount(); ++i) {
            const BarrierList::SCTextureBarrierData& data = m_barrierList.GetSCTextureBarrierByIdx(i);

            VkImageMemoryBarrier2 barrier = CreateImageMemoryBarrier2Data(
                data.pTexture->Get(),
                data.pTexture->GetStageMask(), data.dstStageMask,
                data.pTexture->GetAccessMask(), data.dstAccessMask,
                data.pTexture->GetLayout(), data.dstLayout,
                data.dstAspectMask, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS
            );

            data.pTexture->Transit(data.dstLayout, data.dstStageMask, data.dstAccessMask);

            textureBarriers.emplace_back(barrier);
        }

        VkDependencyInfo dependencyInfo = {};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.bufferMemoryBarrierCount = bufferBarriers.size();
        dependencyInfo.pBufferMemoryBarriers = bufferBarriers.data();
        dependencyInfo.imageMemoryBarrierCount = textureBarriers.size();
        dependencyInfo.pImageMemoryBarriers = textureBarriers.data();

        vkCmdPipelineBarrier2(m_cmdBuffer, &dependencyInfo);

        m_barrierList.End();

        return *this;
    }


    CmdBuffer& CmdBuffer::Reset(VkCommandBufferResetFlags flags)
    {
        VK_ASSERT(IsValid());
        VK_CHECK(vkResetCommandBuffer(m_cmdBuffer, flags));

        return *this;
    }

    
    Device* CmdBuffer::GetDevice() const
    {
        return m_pOwner->GetDevice();
    }


    CmdBuffer::CmdBuffer(CmdPool* pOwnerPool, VkCommandBufferLevel level)
    {
        Allocate(pOwnerPool, level);
    }


    CmdBuffer& CmdBuffer::Allocate(CmdPool* pOwnerPool, VkCommandBufferLevel level)
    {
        VK_ASSERT(pOwnerPool->IsCreated());

        if (IsCreated()) {
            VK_LOG_WARN("Recreation of command buffer %s", GetDebugName());
            m_pOwner->FreeCmdBuffer(*this);
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

        VK_ASSERT(m_cmdBuffer != VK_NULL_HANDLE);

        m_pOwner = pOwnerPool;

        SetCreated(true);

        return *this;
    }


    CmdBuffer& CmdBuffer::Free()
    {
        if (!IsValid()) {
            return *this;
        }

        VkDevice vkDevice = GetDevice()->Get();

        vkFreeCommandBuffers(vkDevice, m_pOwner->Get(), 1, &m_cmdBuffer);
        m_cmdBuffer = VK_NULL_HANDLE;

        m_pOwner = nullptr;

        m_state.reset();

        Object::Destroy();

        return *this;
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


    CmdPool& CmdPool::Create(const CmdPoolCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of command pool %s", GetDebugName());
            Destroy();
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());

        VkDevice vkDevice = info.pDevice->Get();

        VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
        cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolCreateInfo.flags = info.flags;
        cmdPoolCreateInfo.queueFamilyIndex = info.queueFamilyIndex;

        m_pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateCommandPool(vkDevice, &cmdPoolCreateInfo, nullptr, &m_pool));

        VK_ASSERT(m_pool != VK_NULL_HANDLE);

        m_pDevice = info.pDevice;

        SetCreated(true);

        return *this;
    }


    CmdPool& CmdPool::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        Object::Destroy();

        VkDevice vkDevice = m_pDevice->Get();

        vkDestroyCommandPool(vkDevice, m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;

        m_pDevice = nullptr;

        return *this;
    }


    CmdPool& CmdPool::Reset(VkCommandPoolResetFlags flags)
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkResetCommandPool(m_pDevice->Get(), m_pool, flags));

        return *this;
    }


    CmdBuffer CmdPool::AllocCmdBuffer(VkCommandBufferLevel level)
    {
        VK_ASSERT(IsCreated());
        return CmdBuffer(this, level);
    }


    CmdPool& CmdPool::FreeCmdBuffer(CmdBuffer& cmdBuffer)
    {
        VK_ASSERT(IsCreated());
        cmdBuffer.Free();

        return *this;
    }
}
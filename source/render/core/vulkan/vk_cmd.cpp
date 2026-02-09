#include "pch.h"

#include "vk_cmd.h"
#include "vk_query.h"
#include "vk_buffer.h"
#include "vk_texture.h"
#include "vk_swapchain.h"

#include "core/profiler/cpu_profiler.h"


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
        m_scTextureBarriers.clear();

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
        return m_pOwner ? (m_pOwner->IsCreated() && IsCreated() && IsValidID(m_ID)) : false;
    }


    CmdBuffer::CmdBuffer(CmdBuffer&& cmdBuffer) noexcept
    {
        *this = std::move(cmdBuffer);
    }


    CmdBuffer::~CmdBuffer()
    {
        Free();
    }


    CmdBuffer& CmdBuffer::operator=(CmdBuffer&& cmdBuffer) noexcept
    {
        if (this == &cmdBuffer) {
            return *this;
        }

        if (IsValid()) {
            Free();
        }

        Object::operator=(std::move(cmdBuffer));

        m_barrierList.Swap(cmdBuffer.m_barrierList);

        std::swap(m_pOwner, cmdBuffer.m_pOwner);
        std::swap(m_cmdBuffer, cmdBuffer.m_cmdBuffer);
        std::swap(m_blitCache, cmdBuffer.m_blitCache);
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


    CmdBuffer& CmdBuffer::CmdBlitTexture(const Texture& srcTexture, Texture& dstTexture, std::span<const BlitInfo> regions, VkFilter filter)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);
        VK_ASSERT(regions.size() >= 1);

        VkBlitImageInfo2 blitInfo = {};
        blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
        blitInfo.srcImage = srcTexture.Get();
        blitInfo.dstImage = dstTexture.Get();
        
        const VkImageSubresourceLayers& srcSubres = regions[0].srcSubresource;
        blitInfo.srcImageLayout = srcTexture.GetAccessState(srcSubres.baseArrayLayer, srcSubres.mipLevel).layout;

        const VkImageSubresourceLayers& dstSubres = regions[0].dstSubresource;
        blitInfo.dstImageLayout = dstTexture.GetAccessState(dstSubres.baseArrayLayer, dstSubres.mipLevel).layout;

        m_blitCache.resize(regions.size());
        for (size_t i = 0; i < m_blitCache.size(); ++i) {
            VkImageBlit2& blit = m_blitCache[i];
            blit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
            blit.pNext = nullptr;
            blit.srcSubresource = regions[i].srcSubresource;
            blit.srcOffsets[0] = regions[i].srcOffsets[0];
            blit.srcOffsets[1] = regions[i].srcOffsets[1];
            blit.dstSubresource = regions[i].dstSubresource;
            blit.dstOffsets[0] = regions[i].dstOffsets[0];
            blit.dstOffsets[1] = regions[i].dstOffsets[1];
        }

        blitInfo.regionCount = regions.size();
        blitInfo.pRegions = m_blitCache.data();
        
        blitInfo.filter = filter;

        vkCmdBlitImage2(m_cmdBuffer, &blitInfo);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdBlitTexture(const Texture& srcTexture, Texture& dstTexture, const BlitInfo& region, VkFilter filter)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        std::span<const BlitInfo> regions(&region, 1);
        return CmdBlitTexture(srcTexture, dstTexture, regions, filter);
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
        ENG_PROFILE_SCOPED_MARKER_C("CmdBuffer::CmdPushBarrierList", 255, 255, 0, 255);

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

            pTexture->Transit(data.baseMip, data.mipCount, data.baseLayer, data.layerCount, data.dstLayout, data.dstStageMask, data.dstAccessMask);

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


    CmdBuffer::CmdBuffer(CmdPool* pOwnerPool, VkCommandBufferLevel level, ID id)
    {
        Allocate(pOwnerPool, level, id);
    }


    CmdBuffer& CmdBuffer::Allocate(CmdPool* pOwnerPool, VkCommandBufferLevel level, ID id)
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
        m_ID = id;

        SetCreated(true);

        return *this;
    }


    CmdBuffer& CmdBuffer::Free()
    {
        if (!IsValid()) {
            return *this;
        }
        
        vkFreeCommandBuffers(GetDevice()->Get(), m_pOwner->Get(), 1, &m_cmdBuffer);
        m_cmdBuffer = VK_NULL_HANDLE;

        m_barrierList = {};
        m_blitCache = {};

        m_pOwner = nullptr;
        m_ID = INVALID_ID;

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

        std::swap(m_allocatedBuffers, pool.m_allocatedBuffers);
        std::swap(m_freeIds, pool.m_freeIds);

        return *this;
    }


    CmdPool& CmdPool::Create(const CmdPoolCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of command pool %s", GetDebugName());
            Destroy();
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());
        VK_ASSERT_MSG(info.size >= 1, "Command pool size must be >= 1");

        VkDevice vkDevice = info.pDevice->Get();

        VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
        cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolCreateInfo.flags = info.flags;
        cmdPoolCreateInfo.queueFamilyIndex = info.queueFamilyIndex;

        m_pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateCommandPool(vkDevice, &cmdPoolCreateInfo, nullptr, &m_pool));

        VK_ASSERT(m_pool != VK_NULL_HANDLE);

        m_pDevice = info.pDevice;

        m_allocatedBuffers.reserve(info.size);
        m_freeIds.reserve(info.size);

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

        m_allocatedBuffers.clear();
        m_allocatedBuffers.shrink_to_fit();        
        m_freeIds = {};

        m_pDevice = nullptr;

        return *this;
    }


    CmdPool& CmdPool::Reset(VkCommandPoolResetFlags flags)
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkResetCommandPool(m_pDevice->Get(), m_pool, flags));

        return *this;
    }


    CmdBuffer* CmdPool::AllocCmdBuffer(VkCommandBufferLevel level)
    {
        VK_ASSERT(IsCreated());

        const BufferID id = AllocCmdBufferID();
        VK_ASSERT_MSG(CmdBuffer::IsValidID(id), "Out of ID's pool: (%zu)", m_freeIds.capacity());
        
        CmdBuffer& buffer = m_allocatedBuffers[id];
        VK_ASSERT(!buffer.IsValid());

        buffer.Allocate(this, level, id);

        return &buffer;
    }


    CmdPool& CmdPool::FreeCmdBuffer(CmdBuffer& cmdBuffer)
    {
        VK_ASSERT(IsCreated());

        const BufferID ID = cmdBuffer.GetID();

        CmdBuffer& buffer = m_allocatedBuffers[ID];
        VK_ASSERT(cmdBuffer.GetID() == buffer.GetID());

        buffer.Free();

        FreeCmdBufferID(ID);

        return *this;
    }


    CmdPool::BufferID CmdPool::AllocCmdBufferID()
    {
        VK_ASSERT(IsCreated());

        if (!m_freeIds.empty()) {
            const BufferID ID = m_freeIds.back();

            m_freeIds.pop_back();
            return ID;
        }

        VK_ASSERT_MSG(m_allocatedBuffers.size() + 1 <= m_allocatedBuffers.capacity(), "Preallocated cmd buffers pool overflow");

        const BufferID ID = m_allocatedBuffers.size();
        
        m_allocatedBuffers.emplace_back();
        
        return ID;
    }


    void CmdPool::FreeCmdBufferID(BufferID id)
    {
        VK_ASSERT(IsCreated());
        VK_ASSERT_MSG(m_freeIds.size() + 1 <= m_freeIds.capacity(), "Preallocated cmd buffer IDs pool overflow");

        m_freeIds.emplace_back(id);
    }
}
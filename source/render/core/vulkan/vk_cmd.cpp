#include "pch.h"

#include "vk_cmd.h"
#include "vk_query.h"
#include "vk_buffer.h"
#include "vk_texture.h"
#include "vk_swapchain.h"
#include "vk_descriptor.h"
#include "vk_pso.h"


namespace vkn
{
    #define VK_CHECK_CMD_BUFFER_STARTED(CMD_BUFFER_PTR) \
        VK_ASSERT_MSG(CMD_BUFFER_PTR->IsStarted(), "Cmd Buffer \'%s\' is not started", CMD_BUFFER_PTR->GetDebugName().data())

    #define VK_CHECK_CMD_BUFFER_RENDERING_STARTED(CMD_BUFFER_PTR)   \
        VK_CHECK_CMD_BUFFER_STARTED(CMD_BUFFER_PTR);                \
        VK_ASSERT_MSG(CMD_BUFFER_PTR->IsRenderingStarted(), "Cmd Buffer \'%s\' rendering is not started", CMD_BUFFER_PTR->GetDebugName().data())

    static PFN_vkCmdBindDescriptorBuffersEXT vkCmdBindDescriptorBuffers = nullptr;
    static PFN_vkCmdSetDescriptorBufferOffsetsEXT vkCmdSetDescriptorBufferOffsets = nullptr;
    static PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonMode = nullptr;


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


    static VkRenderingAttachmentInfo RenderAttachmentInfoToVkRenderingAttachmentInfo(const RenderAttachmentInfo& info)
    {
        VkRenderingAttachmentInfo res = {};

        res.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        res.pNext = info.pNext;
        res.resolveMode = info.resolveMode;

        if (std::holds_alternative<TextureView*>(info.view)) {
            TextureView* pView = std::get<TextureView*>(info.view);
            VK_ASSERT(pView && pView->IsCreated());
            
            const Texture& owner = pView->GetOwner();
            const TextureView::SubresourceRange& range = pView->GetSubresourceRange();

            res.imageView = pView->Get();
            res.imageLayout = owner.GetAccessTracker().GetState(range.baseArrayLayer, range.baseMipLevel).layout;
        } else {
            SCTextureView* pView = std::get<SCTextureView*>(info.view);
            VK_ASSERT(pView && pView->IsCreated());

            res.imageView = pView->Get();
            res.imageLayout = pView->GetOwner().GetAccessTracker().GetState(0, 0).layout;
        }
        
        if (std::holds_alternative<TextureView*>(info.resolveView)) {
            TextureView* pView = std::get<TextureView*>(info.resolveView);
            
            if (pView) {
                VK_ASSERT(pView->IsCreated());
                
                const Texture& owner = pView->GetOwner();
                const TextureView::SubresourceRange& range = pView->GetSubresourceRange();
    
                res.resolveImageView = pView->Get();
                res.resolveImageLayout = owner.GetAccessTracker().GetState(range.baseArrayLayer, range.baseMipLevel).layout;
            }
        } else {
            SCTextureView* pView = std::get<SCTextureView*>(info.resolveView);
            
            if (pView) {
                VK_ASSERT(pView->IsCreated());
    
                res.resolveImageView = pView->Get();
                res.resolveImageLayout = pView->GetOwner().GetAccessTracker().GetState(0, 0).layout;
            }
        }
        
        res.loadOp = info.loadOp;
        res.storeOp = info.storeOp;
        res.clearValue = info.clearValue;

        return res;
    }


    BarrierList& BarrierList::Begin()
    {
        VK_ASSERT_MSG(!IsStarted(), "Attempt to begin already started barrier list");
        VK_ASSERT_MSG(IsValidOwner(), "Invalid Command Buffer Owner");
        
        m_state.set(FLAG_IS_STARTED, true);
        
        return *this;
    }


    BarrierList& BarrierList::Push()
    {
        VK_ASSERT_MSG(IsValidOwner(), "Invalid Command Buffer Owner");

        m_pCmdBufferOwner->CmdPushBarrierList();

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


    BarrierList& BarrierList::AddTextureBarrier(
        Texture& texture, 
        VkImageLayout dstLayout, 
        VkPipelineStageFlags2 dstStageMask, 
        VkAccessFlags2 dstAccessMask,
        VkImageAspectFlags aspectMask, 
        uint32_t baseMip, uint32_t mipCount, 
        uint32_t baseLayer, uint32_t layerCount
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


    BarrierList& BarrierList::Swap(BarrierList& list) noexcept
    {
        std::swap(m_pCmdBufferOwner, list.m_pCmdBufferOwner);
        std::swap(m_bufferBarriers, list.m_bufferBarriers);
        std::swap(m_textureBarriers, list.m_textureBarriers);
        std::swap(m_scTextureBarriers, list.m_scTextureBarriers);
        std::swap(m_state, list.m_state);

        return *this;
    }

    bool BarrierList::IsValidOwner() const
    {
        return m_pCmdBufferOwner && m_pCmdBufferOwner->IsCreated();
    }


    bool RenderInfo::HasDepthAttachment() const
    {
        return std::holds_alternative<TextureView*>(depthAttachment.view)
            ? std::get<TextureView*>(depthAttachment.view) != nullptr
            : std::get<SCTextureView*>(depthAttachment.view) != nullptr;
    }


    bool RenderInfo::HasStencilAttachment() const
    {
        return std::holds_alternative<TextureView*>(stencilAttachment.view)
            ? std::get<TextureView*>(stencilAttachment.view) != nullptr
            : std::get<SCTextureView*>(stencilAttachment.view) != nullptr;
    }


    PushDescriptor PushDescriptor::ConstantBuffer(uint32_t binding, uint32_t arrayElement, const Buffer& buffer, VkDeviceSize offset, VkDeviceSize size)
    {
        VK_ASSERT(buffer.IsCreated());
        VK_ASSERT(buffer.IsConstantBuffer());

        PushDescriptor result(binding, arrayElement);

        result.m_resource = ConstantBufferRes {
            .pBuffer = &buffer,
            .offset = offset,
            .size = size
        };

        return result;
    }

    
    PushDescriptor PushDescriptor::StorageBuffer(uint32_t binding, uint32_t arrayElement, const Buffer &buffer, VkDeviceSize offset, VkDeviceSize size)
    {
        VK_ASSERT(buffer.IsCreated());
        VK_ASSERT(buffer.IsStorageBuffer());

        PushDescriptor result(binding, arrayElement);

        result.m_resource = StorageBufferRes {
            .pBuffer = &buffer,
            .offset = offset,
            .size = size
        };

        return result;
    }

    
    PushDescriptor PushDescriptor::SampledTexture(uint32_t binding, uint32_t arrayElement, const TextureView& view, VkImageLayout layout)
    {
        VK_ASSERT(view.IsCreated());

        PushDescriptor result(binding, arrayElement);

        result.m_resource = SampledTextureRes {
            .pView = &view,
            .layout = layout
        };

        return result;
    }

    
    PushDescriptor PushDescriptor::StorageTexture(uint32_t binding, uint32_t arrayElement, const TextureView& view, VkImageLayout layout)
    {
        VK_ASSERT(view.IsCreated());

        PushDescriptor result(binding, arrayElement);

        result.m_resource = StorageTextureRes {
            .pView = &view,
            .layout = layout
        };

        return result;
    }

    
    PushDescriptor PushDescriptor::Sampler(uint32_t binding, uint32_t arrayElement, const vkn::Sampler& sampler)
    {
        VK_ASSERT(sampler.IsCreated());

        PushDescriptor result(binding, arrayElement);

        result.m_resource = SamplerRes {
            .pSampler = &sampler
        };

        return result;
    }


    PushDescriptor::PushDescriptor(uint32_t binding, uint32_t arrayElement)
        : m_binding(binding), m_arrayElement(arrayElement), m_resource(ConstantBufferRes{})
    {
    }

    
    void PushDescriptor::Fill(VkWriteDescriptorSet& write, VkDescriptorBufferInfo& bufferInfoCache, VkDescriptorImageInfo& imageInfoCache) const
    {
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

        write.dstBinding = m_binding;
        write.dstArrayElement = m_arrayElement;
        write.descriptorCount = 1;

        if (auto pRes = std::get_if<PushDescriptor::StorageBufferRes>(&m_resource)) {
            bufferInfoCache.buffer = pRes->pBuffer->Get();
            bufferInfoCache.offset = pRes->offset;

            bufferInfoCache.range = pRes->size == VK_WHOLE_SIZE ? pRes->pBuffer->GetMemorySize() - pRes->offset : pRes->size;

            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.pBufferInfo = &bufferInfoCache;
        } else if (auto pRes = std::get_if<PushDescriptor::ConstantBufferRes>(&m_resource)) {
            bufferInfoCache.buffer = pRes->pBuffer->Get();
            bufferInfoCache.offset = pRes->offset;

            bufferInfoCache.range = pRes->size == VK_WHOLE_SIZE ? pRes->pBuffer->GetMemorySize() - pRes->offset : pRes->size;

            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo = &bufferInfoCache;
        } else if (auto pRes = std::get_if<PushDescriptor::SampledTextureRes>(&m_resource)) {
            imageInfoCache.imageView = pRes->pView->Get();
            imageInfoCache.imageLayout = pRes->layout;

            write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            write.pImageInfo = &imageInfoCache;
        } else if (auto pRes = std::get_if<PushDescriptor::StorageTextureRes>(&m_resource)) {
            imageInfoCache.imageView = pRes->pView->Get();
            imageInfoCache.imageLayout = pRes->layout;

            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            write.pImageInfo = &imageInfoCache;
        } else if (auto pRes = std::get_if<PushDescriptor::SamplerRes>(&m_resource)) {
            imageInfoCache.sampler = pRes->pSampler->Get();

            write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            write.pImageInfo = &imageInfoCache;
        } else {
            VK_ASSERT_FAIL("Invalid push descriptor resource type");
        }
    }


    bool CmdBuffer::IsValid() const
    {
        return m_pOwner ? (m_pOwner->IsCreated() && IsCreated() && IsValidID(m_ID)) : false;
    }


    CmdBuffer::CmdBuffer(CmdBuffer&& cmdBuffer) noexcept
    {
        *this = std::move(cmdBuffer);
    }


    CmdBuffer::CmdBuffer()
    {
        m_barrierList.m_pCmdBufferOwner = this;
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

        m_barrierList.Swap(cmdBuffer.m_barrierList);

        std::swap(m_pOwner, cmdBuffer.m_pOwner);
        std::swap(m_pushDescriptorsCache, cmdBuffer.m_pushDescriptorsCache);
        std::swap(m_blitCache, cmdBuffer.m_blitCache);
        std::swap(m_bufImageCopyCache, cmdBuffer.m_bufImageCopyCache);
        std::swap(m_texSubresCache, cmdBuffer.m_texSubresCache);
        std::swap(m_pDescrBufferBindingCache, cmdBuffer.m_pDescrBufferBindingCache);
        std::swap(m_pPSOCache, cmdBuffer.m_pPSOCache);
        std::swap(m_pIndexBufferCache, cmdBuffer.m_pIndexBufferCache);
        std::swap(m_state, cmdBuffer.m_state);

        Base::operator=(std::move(cmdBuffer));

        return *this;
    }


    CmdBuffer& CmdBuffer::Begin(VkCommandBufferUsageFlags flags)
    {
        VK_ASSERT(IsValid());
        VK_ASSERT(!IsStarted());

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = flags;

        VK_CHECK(vkBeginCommandBuffer(Get(), &beginInfo));

        ResetCache();
        m_state.set(FLAG_IS_STARTED, true);

        return *this;
    }
    
    
    CmdBuffer& CmdBuffer::End()
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);
        VK_ASSERT_MSG(!m_barrierList.IsStarted(), "Attempt to end command buffer with started buffer barrier list");

        VK_CHECK(vkEndCommandBuffer(Get()));

        m_state.set(FLAG_IS_STARTED, false);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdResetQueryPool(QueryPool& queryPool, uint32_t firstQuery, uint32_t queryCount)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);
        VK_ASSERT(firstQuery + queryCount <= queryPool.GetQueryCount());

        vkCmdResetQueryPool(Get(), queryPool.Get(), firstQuery, queryCount);

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

        vkCmdWriteTimestamp2(Get(), stage, queryPool.Get(), queryIndex);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdBeginRendering(const RenderInfo& info)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);
        VK_ASSERT(!IsRenderingStarted());

        VkRenderingInfo renderingInfo = {};
        renderingInfo.sType      = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.pNext      = info.pNext;
        renderingInfo.flags      = info.flags;
        renderingInfo.renderArea = info.renderArea;
        renderingInfo.layerCount = info.layerCount;
        renderingInfo.viewMask   = info.viewMask;

        const uint32_t colorAttachmentsCount = info.GetColorAttachmentsCount();

        renderingInfo.colorAttachmentCount = colorAttachmentsCount;

        std::array<VkRenderingAttachmentInfo, 8> colorAttachments;
        VK_ASSERT(colorAttachmentsCount <= colorAttachments.size());

        for (size_t i = 0; i < colorAttachmentsCount; ++i) {
            colorAttachments[i] = RenderAttachmentInfoToVkRenderingAttachmentInfo(info.colorAttachments[i]);
        }

        renderingInfo.pColorAttachments = colorAttachmentsCount > 0 ? colorAttachments.data() : nullptr;

        VkRenderingAttachmentInfo depthAttachment = {};
        if (info.HasDepthAttachment()) {
            depthAttachment = RenderAttachmentInfoToVkRenderingAttachmentInfo(info.depthAttachment);
            renderingInfo.pDepthAttachment = &depthAttachment;
        }

        VkRenderingAttachmentInfo stencilAttachment = {};
        if (info.HasStencilAttachment()) {
            stencilAttachment = RenderAttachmentInfoToVkRenderingAttachmentInfo(info.stencilAttachment);
            renderingInfo.pStencilAttachment = &depthAttachment;
        }

        vkCmdBeginRendering(Get(), &renderingInfo);

        m_state.set(FLAG_IS_RENDERING_STARTED, true);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdEndRendering()
    {
        VK_CHECK_CMD_BUFFER_RENDERING_STARTED(this);

        vkCmdEndRendering(Get());

        m_state.set(FLAG_IS_RENDERING_STARTED, false);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdBindPSO(PSO& pso)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        if (m_pPSOCache == &pso) {
            return *this;
        }
        
        vkCmdBindPipeline(Get(), pso.GetBindPoint(), pso.Get());

        m_pPSOCache = &pso;

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdSetViewport(uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        vkCmdSetViewport(Get(), firstViewport, viewportCount, pViewports);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdSetViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
    {
        VkViewport viewport = {};
        viewport.x = x;
        viewport.y = y;
        viewport.width = width;
        viewport.height = height;
        viewport.minDepth = minDepth;
        viewport.maxDepth = maxDepth;

        return CmdSetViewport(0, 1, &viewport);
    }


    CmdBuffer& CmdBuffer::CmdSetScissor(uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        vkCmdSetScissor(Get(), firstScissor, scissorCount, pScissors);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdSetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
    {
        VkRect2D scissors = {};
        scissors.offset.x = x;
        scissors.offset.y = y;
        scissors.extent.width = width;
        scissors.extent.height = height;

        return CmdSetScissor(0, 1, &scissors);
    }


    CmdBuffer& CmdBuffer::CmdSetDepthCompareOp(VkCompareOp op)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        vkCmdSetDepthCompareOp(Get(), op);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdSetDepthWriteEnable(VkBool32 enabled)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        vkCmdSetDepthWriteEnable(Get(), enabled);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdSetPrimitiveTopology(VkPrimitiveTopology topology)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        vkCmdSetPrimitiveTopology(Get(), topology);
        
        return *this;
    }

    
    CmdBuffer& CmdBuffer::CmdSetPolygonMode(VkPolygonMode mode)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        if (!vkCmdSetPolygonMode) {
            vkCmdSetPolygonMode = (PFN_vkCmdSetPolygonModeEXT)GetDevice().GetProcAddr("vkCmdSetPolygonModeEXT");
        }

        vkCmdSetPolygonMode(Get(), mode);
        
        return *this;
    }


    CmdBuffer& CmdBuffer::CmdBlitTexture(const Texture& srcTexture, Texture& dstTexture, std::span<const TextureBlitInfo> regions, VkFilter filter)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);
        VK_ASSERT(regions.size() >= 1);

        VkBlitImageInfo2 blitInfo = {};
        blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
        blitInfo.srcImage = srcTexture.Get();
        blitInfo.dstImage = dstTexture.Get();
        
        const VkImageSubresourceLayers& srcSubres = regions[0].srcSubresource;
        blitInfo.srcImageLayout = srcTexture.GetAccessTracker().GetState(srcSubres.baseArrayLayer, srcSubres.mipLevel).layout;

        const VkImageSubresourceLayers& dstSubres = regions[0].dstSubresource;
        blitInfo.dstImageLayout = dstTexture.GetAccessTracker().GetState(dstSubres.baseArrayLayer, dstSubres.mipLevel).layout;

        m_blitCache.resize(regions.size());
        for (size_t i = 0; i < m_blitCache.size(); ++i) {
            const TextureBlitInfo& region = regions[i];
            VkImageBlit2& blit = m_blitCache[i];

            blit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
            blit.pNext = nullptr;
            blit.srcSubresource = region.srcSubresource;
            blit.srcOffsets[0] = region.srcOffsets[0];
            blit.srcOffsets[1] = region.srcOffsets[1];
            blit.dstSubresource = region.dstSubresource;
            blit.dstOffsets[0] = region.dstOffsets[0];
            blit.dstOffsets[1] = region.dstOffsets[1];
        }

        blitInfo.regionCount = regions.size();
        blitInfo.pRegions = m_blitCache.data();
        
        blitInfo.filter = filter;

        vkCmdBlitImage2(Get(), &blitInfo);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdBlitTexture(const Texture& srcTexture, Texture& dstTexture, const TextureBlitInfo& region, VkFilter filter)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        std::span<const TextureBlitInfo> regions(&region, 1);
        return CmdBlitTexture(srcTexture, dstTexture, regions, filter);
    }


    CmdBuffer& CmdBuffer::CmdFillBuffer(Buffer& buffer, uint32_t value, VkDeviceSize offset, VkDeviceSize size)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        size = size == VK_WHOLE_SIZE ? buffer.GetMemorySize() : size;
        VK_ASSERT(offset + size <= buffer.GetMemorySize());

        vkCmdFillBuffer(Get(), buffer.Get(), offset, size, value);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdClearTexture(Texture& texture, const TextureClearInfo& clear, std::span<const TextureClearRange> ranges)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        VK_ASSERT(!ranges.empty());

        m_texSubresCache.resize(ranges.size());

        const bool isColorTex = texture.IsColor();
        const bool isDepth = texture.IsDepth();
        const bool isStencil = texture.IsStencil();
        const bool isDepthStencil = texture.IsDepthStencil();

        VkImageAspectFlags aspectMask = {};
        if (isColorTex) {
            aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        } else if (isDepth) {
            aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        } else if (isStencil) {
            aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        } else if (isDepthStencil) {
            aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        } else {
            VK_ASSERT_FAIL("Invalid texture %s aspect", texture.GetDebugName().data());
        }

        for (size_t i = 0; i < ranges.size(); ++i) {
            m_texSubresCache[i].aspectMask = aspectMask;
            m_texSubresCache[i].baseMipLevel = ranges[i].baseMipLevel;
            m_texSubresCache[i].levelCount = ranges[i].levelCount;
            m_texSubresCache[i].baseArrayLayer = ranges[i].baseArrayLayer;
            m_texSubresCache[i].layerCount = ranges[i].layerCount;
        }

        TextureAccessTracker& tracker = texture.GetAccessTracker();

        const VkImageLayout layout = tracker.GetState(ranges[0].baseArrayLayer, ranges[0].baseMipLevel).layout;

        if (isColorTex) {
            VkClearColorValue clearValue = {};
            clearValue.float32[0] = clear.r;
            clearValue.float32[1] = clear.g;
            clearValue.float32[2] = clear.b;
            clearValue.float32[3] = clear.a;

        #ifdef ENG_ASSERT_ENABLED
            for (const TextureClearRange& range : ranges) {
                VK_ASSERT(
                    tracker.CheckLayoutConsistency(range.baseArrayLayer, range.layerCount, range.baseMipLevel, range.levelCount, VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR) ||
                    tracker.CheckLayoutConsistency(range.baseArrayLayer, range.layerCount, range.baseMipLevel, range.levelCount, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ||
                    tracker.CheckLayoutConsistency(range.baseArrayLayer, range.layerCount, range.baseMipLevel, range.levelCount, VK_IMAGE_LAYOUT_GENERAL)
                );
            }
        #endif

            vkCmdClearColorImage(Get(), texture.Get(), layout, &clearValue, m_texSubresCache.size(), m_texSubresCache.data());
        } else {
            VkClearDepthStencilValue clearValue = {};
            clearValue.depth = clear.depth;
            clearValue.stencil = clear.stencil;

        #ifdef ENG_ASSERT_ENABLED
            for (const TextureClearRange& range : ranges) {
                VK_ASSERT(
                    tracker.CheckLayoutConsistency(range.baseArrayLayer, range.layerCount, range.baseMipLevel, range.levelCount, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ||
                    tracker.CheckLayoutConsistency(range.baseArrayLayer, range.layerCount, range.baseMipLevel, range.levelCount, VK_IMAGE_LAYOUT_GENERAL)
                );
            }
        #endif

            vkCmdClearDepthStencilImage(Get(), texture.Get(), layout, &clearValue, m_texSubresCache.size(), m_texSubresCache.data());
        }

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdClearTexture(Texture& texture, const TextureClearInfo& clear, const TextureClearRange& range)
    {
        std::span<const TextureClearRange> ranges(&range, 1);
        return CmdClearTexture(texture, clear, ranges);
    }


    CmdBuffer& CmdBuffer::CmdCopyBuffer(const Buffer& srcBuffer, Buffer& dstBuffer, std::span<const VkBufferCopy> regions)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);
        VK_ASSERT(regions.size() >= 1);

    #ifdef ENG_BUILD_DEBUG
        const VkDeviceSize srcBuffSize = srcBuffer.GetMemorySize();
        const VkDeviceSize dstBuffSize = dstBuffer.GetMemorySize();

        for (size_t i = 0; i < regions.size(); ++i) {
            const VkBufferCopy& region = regions[i];

            VK_ASSERT_MSG(region.srcOffset + region.size <= srcBuffSize, "COPY REGION %zu: src offset + size > src buffer size", i);
            VK_ASSERT_MSG(region.dstOffset + region.size <= dstBuffSize, "COPY REGION %zu: dst offset + size > dst buffer size", i);
        }
    #endif

        vkCmdCopyBuffer(Get(), srcBuffer.Get(), dstBuffer.Get(), regions.size(), regions.data());

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdCopyBuffer(const Buffer& srcBuffer, Buffer& dstBuffer, const VkBufferCopy& region)
    {
        std::span<const VkBufferCopy> regions(&region, 1);
        return CmdCopyBuffer(srcBuffer, dstBuffer, regions);
    }


    CmdBuffer& CmdBuffer::CmdCopyBuffer(const Buffer& srcBuffer, Buffer& dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset)
    {
        VkBufferCopy region = {};
        region.size = size;
        region.srcOffset = srcOffset;
        region.dstOffset = dstOffset;

        return CmdCopyBuffer(srcBuffer, dstBuffer, region);
    }


    CmdBuffer& CmdBuffer::CmdCopyBuffer(const Buffer& srcBuffer, Buffer& dstBuffer)
    {
        return CmdCopyBuffer(srcBuffer, dstBuffer, srcBuffer.GetMemorySize());
    }


    CmdBuffer& CmdBuffer::CmdCopyBuffer(const Buffer& srcBuffer, Texture& dstTexture, std::span<const BufferToTextureCopyInfo> regions)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);
        VK_ASSERT(regions.size() >= 1);

        VkCopyBufferToImageInfo2 copyInfo = {};
        copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
        copyInfo.srcBuffer = srcBuffer.Get();
        copyInfo.dstImage = dstTexture.Get();

        const VkImageSubresourceLayers& dstSubres = regions[0].texSubresource;
        copyInfo.dstImageLayout = dstTexture.GetAccessTracker().GetState(dstSubres.baseArrayLayer, dstSubres.mipLevel).layout;

        m_bufImageCopyCache.resize(regions.size());
        for (size_t i = 0; i < m_bufImageCopyCache.size(); ++i) {
            const BufferToTextureCopyInfo& region = regions[i];
            VkBufferImageCopy2& copy = m_bufImageCopyCache[i];

            copy.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
            copy.pNext = nullptr;
            copy.bufferOffset = region.bufOffset;
            copy.bufferRowLength = region.bufRowLength;
            copy.bufferImageHeight = region.bufImageHeight;
            copy.imageSubresource = region.texSubresource;
            copy.imageOffset = region.texOffset;
            copy.imageExtent = region.texExtent;
        }

        copyInfo.regionCount = m_bufImageCopyCache.size();
        copyInfo.pRegions = m_bufImageCopyCache.data();

        vkCmdCopyBufferToImage2(Get(), &copyInfo);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdCopyBuffer(const Buffer& srcBuffer, Texture& dstTexture, const BufferToTextureCopyInfo& region)
    {
        std::span<const BufferToTextureCopyInfo> regions(&region, 1);
        return CmdCopyBuffer(srcBuffer, dstTexture, regions);
    }


    CmdBuffer& CmdBuffer::CmdCopyTexture(const Texture& srcTexture, Texture& dstTexture, std::span<const VkImageCopy> regions)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);
        VK_ASSERT(regions.size() >= 1);

        const TextureAccessTracker& srcTracker = srcTexture.GetAccessTracker();
        const TextureAccessTracker& dstTracker = dstTexture.GetAccessTracker();

        const VkImageSubresourceLayers& srcSubres = regions[0].srcSubresource;
        const VkImageSubresourceLayers& dstSubres = regions[0].dstSubresource;

        VK_ASSERT_MSG(
            srcTracker.CheckLayoutConsistency(srcSubres.baseArrayLayer, srcSubres.layerCount, srcSubres.mipLevel, 1),
            "Src texture %s has inconsisten layout in subresource range: [baseLayer: %u, layerCount: %u, mip: %u",
            srcTexture.GetDebugName().data(), srcSubres.baseArrayLayer, srcSubres.layerCount, srcSubres.mipLevel
        );

        VK_ASSERT_MSG(
            dstTracker.CheckLayoutConsistency(dstSubres.baseArrayLayer, dstSubres.layerCount, dstSubres.mipLevel, 1),
            "Dst texture %s has inconsisten layout in subresource range: [baseLayer: %u, layerCount: %u, mip: %u",
            dstTexture.GetDebugName().data(), dstSubres.baseArrayLayer, dstSubres.layerCount, dstSubres.mipLevel
        );

        const TextureAccessTracker::State& srcState = srcTracker.GetState(srcSubres.baseArrayLayer, srcSubres.mipLevel);
        const TextureAccessTracker::State& dstState = dstTracker.GetState(dstSubres.baseArrayLayer, dstSubres.mipLevel);

        vkCmdCopyImage(Get(), srcTexture.Get(), srcState.layout, dstTexture.Get(), dstState.layout, regions.size(), regions.data());

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdCopyTexture(const Texture& srcTexture, Texture& dstTexture, const VkImageCopy& region)
    {
        std::span<const VkImageCopy> regions(&region, 1);
        return CmdCopyTexture(srcTexture, dstTexture, regions);
    }


    CmdBuffer& CmdBuffer::CmdPushDescriptors(const PSO& pso, uint32_t set, std::span<const PushDescriptor> descriptors)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        VK_ASSERT(pso.IsCreated());
        VK_ASSERT(!descriptors.empty());

        m_pushDescriptorsCache.Reserve(descriptors.size());
        m_pushDescriptorsCache.Clear();

        std::vector<VkWriteDescriptorSet>& writesCache = m_pushDescriptorsCache.writes;
        std::vector<VkDescriptorBufferInfo>& bufferCache = m_pushDescriptorsCache.bufferInfos;
        std::vector<VkDescriptorImageInfo>& texCache = m_pushDescriptorsCache.imageInfos;

        for (const PushDescriptor& descriptor : descriptors) {
            VkWriteDescriptorSet& write = writesCache.emplace_back();
            VkDescriptorBufferInfo& bufferWrite = bufferCache.emplace_back();
            VkDescriptorImageInfo& texWrite = texCache.emplace_back();

            descriptor.Fill(write, bufferWrite, texWrite);
        }

        vkCmdPushDescriptorSet(Get(), pso.GetBindPoint(), pso.GetLayout().Get(), set, writesCache.size(), writesCache.data());

        return *this;
    }

    
    CmdBuffer& CmdBuffer::CmdPushDescriptors(const PSO& pso, uint32_t set, const PushDescriptor& descriptor)
    {
        return CmdPushDescriptors(pso, set, std::span(&descriptor, 1));
    }


    CmdBuffer &CmdBuffer::CmdDispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        vkCmdDispatch(Get(), groupCountX, groupCountY, groupCountZ);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdDispatchIndirect(Buffer& argBuffer, VkDeviceSize offset)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        vkCmdDispatchIndirect(Get(), argBuffer.Get(), offset);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdBindIndexBuffer(vkn::Buffer& idxBuffer, VkDeviceSize offset, VkIndexType idxType)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        if (m_pIndexBufferCache == &idxBuffer) {
            return *this;
        }

        vkCmdBindIndexBuffer(Get(), idxBuffer.Get(), offset, idxType);

        m_pIndexBufferCache = &idxBuffer;

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        VK_CHECK_CMD_BUFFER_RENDERING_STARTED(this);

        vkCmdDraw(Get(), vertexCount, instanceCount, firstVertex, firstInstance);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
    {
        VK_CHECK_CMD_BUFFER_RENDERING_STARTED(this);

        vkCmdDrawIndexed(Get(), indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdDrawIndexedIndirect(Buffer& argBuffer, VkDeviceSize argBufferOffset, Buffer& countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t argStride)
    {
        VK_CHECK_CMD_BUFFER_RENDERING_STARTED(this);

        vkCmdDrawIndexedIndirectCount(Get(), argBuffer.Get(), argBufferOffset, countBuffer.Get(), countBufferOffset, maxDrawCount, argStride);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdBindDescriptorBuffer(DescriptorBuffer& buffer)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);
        VK_ASSERT(buffer.IsCreated());

        if (vkCmdBindDescriptorBuffers == nullptr) {
            vkCmdBindDescriptorBuffers = (PFN_vkCmdBindDescriptorBuffersEXT)GetDevice().GetProcAddr("vkCmdBindDescriptorBuffersEXT");
        }

        m_pDescrBufferBindingCache = &buffer;

        VkDescriptorBufferBindingInfoEXT bindingInfo = {};
        bindingInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
        bindingInfo.address = buffer.GetBuffer().GetDeviceAddress();
        bindingInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

        vkCmdBindDescriptorBuffers(Get(), 1, &bindingInfo);

        return *this;
    }

    
    CmdBuffer& CmdBuffer::CmdBindDescriptorBufferSets(const PSO& pso, const DescriptorBufferSetBindingInfo& bindigInfo)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        if (!vkCmdSetDescriptorBufferOffsets) {
            vkCmdSetDescriptorBufferOffsets = (PFN_vkCmdSetDescriptorBufferOffsetsEXT)GetDevice().GetProcAddr("vkCmdSetDescriptorBufferOffsetsEXT");
        }

        VK_ASSERT(pso.IsCreated());
        VK_ASSERT_MSG(m_pDescrBufferBindingCache != nullptr, "Call CmdBindDescriptorBuffer before CmdBindDescriptorBufferSet");
        VK_ASSERT(bindigInfo.elemIndex < m_pDescrBufferBindingCache->GetSetCount());

        const VkDeviceSize offset = m_pDescrBufferBindingCache->GetSetOffset(bindigInfo.elemIndex);

        constexpr uint32_t bufferIdx = 0;
        vkCmdSetDescriptorBufferOffsets(Get(), pso.GetBindPoint(), pso.GetLayout().Get(), bindigInfo.shaderSetIdx, 1, &bufferIdx, &offset);

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdBindDescriptorBufferSets(const PSO& pso, std::span<const DescriptorBufferSetBindingInfo> bindigInfos)
    {
        for (const DescriptorBufferSetBindingInfo& info : bindigInfos) {
            CmdBindDescriptorBufferSets(pso, info);
        }

        return *this;
    }


    CmdBuffer& CmdBuffer::CmdPushConstants(PSO& pso, VkShaderStageFlags stagesMask, const void* pData, VkDeviceSize size, VkDeviceSize offset)
    {
        VK_CHECK_CMD_BUFFER_STARTED(this);

        vkCmdPushConstants(Get(), pso.GetLayout().Get(), stagesMask, offset, size, pData);

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

            BufferAccessTracker& tracker = data.pBuffer->GetAccessTracker();
            const BufferAccessTracker::State& state = tracker.GetState();

            VkBufferMemoryBarrier2 barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            barrier.buffer = data.pBuffer->Get();
            barrier.srcStageMask = state.stageMask;
            barrier.srcAccessMask = state.accessMask;
            barrier.dstStageMask = data.dstStageMask;
            barrier.dstAccessMask = data.dstAccessMask;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.offset = data.offset;
            barrier.size = data.size;

            tracker.Transit(data.dstStageMask, data.dstAccessMask);

            bufferBarriers.emplace_back(barrier);
        }

        static std::vector<VkImageMemoryBarrier2> textureBarriers(m_barrierList.GetTextureBarriersCount() + m_barrierList.GetSCTextureBarriersCount());
        textureBarriers.clear();

        for (size_t i = 0; i < m_barrierList.GetTextureBarriersCount(); ++i) {
            const BarrierList::TextureBarrierData& data = m_barrierList.GetTextureBarrierByIdx(i);

            Texture* pTexture = data.pTexture;
            
            TextureAccessTracker& accessTracker = pTexture->GetAccessTracker();
            accessTracker.CheckLayoutConsistency(data.baseLayer, data.layerCount, data.baseMip, data.mipCount);

            const TextureAccessTracker::State& currState = accessTracker.GetState(data.baseLayer, data.baseMip);

            VkImageMemoryBarrier2 barrier = CreateImageMemoryBarrier2Data(
                pTexture->Get(),
                currState.stageMask, data.dstStageMask,
                currState.accessMask, data.dstAccessMask,
                currState.layout, data.dstLayout,
                data.dstAspectMask, data.baseMip, data.mipCount, data.baseLayer, data.layerCount
            );

            accessTracker.Transit(data.baseMip, data.mipCount, data.baseLayer, data.layerCount, data.dstLayout, data.dstStageMask, data.dstAccessMask);

            textureBarriers.emplace_back(barrier);
        }

        for (size_t i = 0; i < m_barrierList.GetSCTextureBarriersCount(); ++i) {
            const BarrierList::SCTextureBarrierData& data = m_barrierList.GetSCTextureBarrierByIdx(i);

            TextureAccessTracker& tracker = data.pTexture->GetAccessTracker();
            const TextureAccessTracker::State& accessState = tracker.GetState(0, 0);

            VkImageMemoryBarrier2 barrier = CreateImageMemoryBarrier2Data(
                data.pTexture->Get(),
                accessState.stageMask, data.dstStageMask,
                accessState.accessMask, data.dstAccessMask,
                accessState.layout, data.dstLayout,
                data.dstAspectMask, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS
            );

            tracker.Transit(0, 1, 0, 1, data.dstLayout, data.dstStageMask, data.dstAccessMask);

            textureBarriers.emplace_back(barrier);
        }

        VkDependencyInfo dependencyInfo = {};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.bufferMemoryBarrierCount = bufferBarriers.size();
        dependencyInfo.pBufferMemoryBarriers = bufferBarriers.data();
        dependencyInfo.imageMemoryBarrierCount = textureBarriers.size();
        dependencyInfo.pImageMemoryBarriers = textureBarriers.data();

        vkCmdPipelineBarrier2(Get(), &dependencyInfo);

        m_barrierList.End();

        return *this;
    }


    CmdBuffer& CmdBuffer::Reset(VkCommandBufferResetFlags flags)
    {
        VK_ASSERT(IsValid());
        VK_CHECK(vkResetCommandBuffer(Get(), flags));

        return *this;
    }

    
    Device& CmdBuffer::GetDevice() const
    {
        return m_pOwner->GetDevice();
    }


    CmdPool& CmdBuffer::GetOwnerPool() const
    {
        VK_ASSERT(IsCreated());
        return *m_pOwner;
    }


    bool CmdBuffer::IsStarted() const
    {
        VK_ASSERT(IsValid());
        return m_state.test(FLAG_IS_STARTED);
    }


    bool CmdBuffer::IsRenderingStarted() const
    {
        VK_ASSERT(IsValid());
        return m_state.test(FLAG_IS_RENDERING_STARTED);
    }


    CmdBuffer::CmdBuffer(CmdPool* pOwnerPool, VkCommandBufferLevel level, ID id)
    {
        Allocate(pOwnerPool, level, id);
    }


    CmdBuffer& CmdBuffer::Allocate(CmdPool* pOwnerPool, VkCommandBufferLevel level, ID id)
    {
        VK_ASSERT(pOwnerPool->IsCreated());

        if (IsCreated()) {
            VK_LOG_WARN("Recreation of command buffer %s", GetDebugName().data());
            m_pOwner->FreeCmdBuffer(*this);
        }

        VK_ASSERT(pOwnerPool && pOwnerPool->IsCreated());

        VkDevice vkDevice = pOwnerPool->GetDevice().Get();
        VkCommandPool vkCmdPool = pOwnerPool->Get();

        VkCommandBufferAllocateInfo cmdBufferAllocInfo = {};
        cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufferAllocInfo.commandPool = vkCmdPool;
        cmdBufferAllocInfo.level = level;
        cmdBufferAllocInfo.commandBufferCount = 1;

        Base::Create([vkDevice, &cmdBufferAllocInfo](VkCommandBuffer& cmdBuffer) {
            VK_CHECK(vkAllocateCommandBuffers(vkDevice, &cmdBufferAllocInfo, &cmdBuffer));
            return cmdBuffer != VK_NULL_HANDLE;
        });

        VK_ASSERT(IsCreated());

        m_pOwner = pOwnerPool;
        m_ID = id;

        return *this;
    }


    CmdBuffer& CmdBuffer::Free()
    {
        if (!IsValid()) {
            return *this;
        }
        
        m_barrierList = {};

        m_pushDescriptorsCache = {};
        m_blitCache = {};
        m_bufImageCopyCache = {};
        m_texSubresCache = {};
        m_pDescrBufferBindingCache = nullptr;
        m_pPSOCache = nullptr;
        m_pIndexBufferCache = nullptr;

        m_ID = INVALID_ID;

        m_state.reset();

        Base::Destroy([device = GetDevice().Get(), pool = GetOwnerPool().Get()](VkCommandBuffer& cmdBuffer) {
            vkFreeCommandBuffers(device, pool, 1, &cmdBuffer);
        });

        m_pOwner = nullptr;

        return *this;
    }


    CmdBuffer& CmdBuffer::ResetCache()
    {
        m_pushDescriptorsCache.Clear();
        m_blitCache.clear();
        m_bufImageCopyCache.clear();
        m_texSubresCache.clear();
        m_pDescrBufferBindingCache = nullptr;
        m_pPSOCache = nullptr;
        m_pIndexBufferCache = nullptr;

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

        std::swap(m_pDevice, pool.m_pDevice);

        std::swap(m_allocatedBuffers, pool.m_allocatedBuffers);
        std::swap(m_freeIds, pool.m_freeIds);

        Base::operator=(std::move(pool));

        return *this;
    }


    CmdPool& CmdPool::Create(const CmdPoolCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of command pool %s", GetDebugName().data());
            Destroy();
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());
        VK_ASSERT_MSG(info.size >= 1, "Command pool size must be >= 1");

        VkDevice vkDevice = info.pDevice->Get();

        VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
        cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolCreateInfo.flags = info.flags;
        cmdPoolCreateInfo.queueFamilyIndex = info.queueFamilyIndex;

        Base::Create([vkDevice, &cmdPoolCreateInfo](VkCommandPool& pool) {
            VK_CHECK(vkCreateCommandPool(vkDevice, &cmdPoolCreateInfo, nullptr, &pool));
            return pool != VK_NULL_HANDLE;
        });

        VK_ASSERT(IsCreated());

        m_pDevice = info.pDevice;

        m_allocatedBuffers.reserve(info.size);
        m_freeIds.reserve(info.size);

        return *this;
    }


    CmdPool& CmdPool::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        m_allocatedBuffers.clear();
        m_allocatedBuffers.shrink_to_fit();        
        m_freeIds = {};

        Base::Destroy([vkDevice = m_pDevice->Get()](VkCommandPool& pool) {
            vkDestroyCommandPool(vkDevice, pool, nullptr);
        });

        m_pDevice = nullptr;

        return *this;
    }


    CmdPool& CmdPool::Reset(VkCommandPoolResetFlags flags)
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkResetCommandPool(m_pDevice->Get(), Get(), flags));

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


    Device& CmdPool::GetDevice() const
    {
        VK_ASSERT(IsCreated());
        return *m_pDevice;
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
    

    void CmdBuffer::PushDescriptorsCache::Reserve(size_t capacity)
    {
        writes.reserve(capacity);
        bufferInfos.reserve(capacity);
        imageInfos.reserve(capacity);
    }
    
    
    void CmdBuffer::PushDescriptorsCache::Clear()
    {
        writes.clear();
        bufferInfos.clear();
        imageInfos.clear();
    }
}
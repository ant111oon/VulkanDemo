#pragma once

#include "vk_device.h"
#include "core/core.h"

#include <span>


namespace vkn
{
    class CmdPool;
    class QueryPool;
    class Buffer;
    class Texture;
    class TextureView;
    class SCTexture;
    class SCTextureView;
    class DescriptorBuffer;
    class PSOLayout;
    class PSO;


    class BarrierList
    {
        friend class CmdBuffer;

    private:
        enum StateFlags
        {
            FLAG_IS_STARTED,
            FLAG_COUNT,
        };

        struct BufferBarrierData
        {
            Buffer*               pBuffer;
            VkPipelineStageFlags2 dstStageMask;
            VkAccessFlags2        dstAccessMask;
            VkDeviceSize          offset;
            VkDeviceSize          size;
        };

        struct TextureBarrierDataBase
        {
            VkImageLayout         dstLayout;
            VkPipelineStageFlags2 dstStageMask;
            VkAccessFlags2        dstAccessMask;
            VkImageAspectFlags    dstAspectMask;
        };

        struct TextureBarrierData : TextureBarrierDataBase
        {
            uint32_t baseMip;
            uint32_t mipCount;
            uint32_t baseLayer;
            uint32_t layerCount;
            Texture* pTexture;
        };

        struct SCTextureBarrierData : TextureBarrierDataBase
        {
            SCTexture* pTexture;
        };

    public:
        ENG_DECL_CLASS_NO_COPIABLE(BarrierList);

        BarrierList() = default;

        BarrierList& Begin();
        BarrierList& Push();

        BarrierList& AddBufferBarrier(
            Buffer& buffer,
            VkPipelineStageFlags2 dstStageMask,
            VkAccessFlags2 dstAccessMask, 
            VkDeviceSize offset = 0,
            VkDeviceSize size = VK_WHOLE_SIZE);

        BarrierList& AddTextureBarrier(
            Texture& texture,
            VkImageLayout dstLayout,
            VkPipelineStageFlags2 dstStageMask,
            VkAccessFlags2 dstAccessMask, 
            VkImageAspectFlags aspectMask, 
            uint32_t baseMip = 0, 
            uint32_t mipCount = VK_REMAINING_MIP_LEVELS,
            uint32_t baseLayer = 0, 
            uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS);

        BarrierList& AddTextureBarrier(
            SCTexture& texture,
            VkImageLayout dstLayout,
            VkPipelineStageFlags2 dstStageMask,
            VkAccessFlags2 dstAccessMask, 
            VkImageAspectFlags aspectMask);

        size_t GetBufferBarriersCount() const { return m_bufferBarriers.size(); }
        size_t GetTextureBarriersCount() const { return m_textureBarriers.size(); }
        size_t GetSCTextureBarriersCount() const { return m_scTextureBarriers.size(); }
        
        bool IsStarted() const { return m_state.test(FLAG_IS_STARTED); }

    private:
        BarrierList(BarrierList&& list) noexcept = default;
        BarrierList& operator=(BarrierList&& list) noexcept = default;

        BarrierList& Reset();
        BarrierList& End();

        const BufferBarrierData& GetBufferBarrierByIdx(size_t i) const;
        const TextureBarrierData& GetTextureBarrierByIdx(size_t i) const;
        const SCTextureBarrierData& GetSCTextureBarrierByIdx(size_t i) const;

        BarrierList& Swap(BarrierList& list) noexcept;

        bool IsValidOwner() const;

    private:
        CmdBuffer* m_pCmdBufferOwner = nullptr;

        std::vector<BufferBarrierData> m_bufferBarriers;
        std::vector<TextureBarrierData> m_textureBarriers;
        std::vector<SCTextureBarrierData> m_scTextureBarriers;

        std::bitset<FLAG_COUNT> m_state = {};
    };


    struct BlitInfo
    {
        VkImageSubresourceLayers srcSubresource;
        VkOffset3D               srcOffsets[2];
        VkImageSubresourceLayers dstSubresource;
        VkOffset3D               dstOffsets[2];
    };


    struct BufferToTextureCopyInfo
    {
        VkDeviceSize             bufOffset;
        uint32_t                 bufRowLength;
        uint32_t                 bufImageHeight;
        VkImageSubresourceLayers texSubresource;
        VkOffset3D               texOffset;
        VkExtent3D               texExtent;
    };


    struct DescriptorSetBindingInfo
    {
        uint32_t bufferSetIdx; // index of set inside bound descriptor buffer
        uint32_t shaderSetIdx; // index of set in shader
    };


    class CmdBuffer : public Object
    {
        friend class CmdPool;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(CmdBuffer);

        CmdBuffer();
        ~CmdBuffer();
    
        CmdBuffer(CmdBuffer&& cmdBuffer) noexcept;
        CmdBuffer& operator=(CmdBuffer&& cmdBuffer) noexcept;

        CmdBuffer& Reset(VkCommandBufferResetFlags flags = 0);

        CmdBuffer& Begin(VkCommandBufferUsageFlags flags = 0);
        CmdBuffer& End();

        CmdBuffer& CmdResetQueryPool(QueryPool& queryPool, uint32_t firstQuery, uint32_t queryCount);
        CmdBuffer& CmdResetQueryPool(QueryPool& queryPool);
        CmdBuffer& CmdWriteTimestamp(QueryPool& queryPool, VkPipelineStageFlags2 stage, uint32_t queryIndex);

        CmdBuffer& CmdBeginRendering(const VkRenderingInfo& renderingInfo);
        CmdBuffer& CmdEndRendering();

        CmdBuffer& CmdBindPSO(PSO& pso);

        CmdBuffer& CmdSetViewport(uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports);
        CmdBuffer& CmdSetViewport(float x, float y, float width, float height, float minDepth = 0.f, float maxDepth = 1.f);

        CmdBuffer& CmdSetScissor(uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors);
        CmdBuffer& CmdSetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height);

        CmdBuffer& CmdSetDepthCompareOp(VkCompareOp op);
        CmdBuffer& CmdSetDepthWriteEnable(VkBool32 enabled);

        CmdBuffer& CmdBlitTexture(const Texture& srcTexture, Texture& dstTexture, std::span<const BlitInfo> regions, VkFilter filter);
        CmdBuffer& CmdBlitTexture(const Texture& srcTexture, Texture& dstTexture, const BlitInfo& region, VkFilter filter);
        
        CmdBuffer& CmdFillBuffer(Buffer& buffer, uint32_t value, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

        CmdBuffer& CmdCopyBuffer(const Buffer& srcBuffer, Buffer& dstBuffer, std::span<const VkBufferCopy> regions);
        CmdBuffer& CmdCopyBuffer(const Buffer& srcBuffer, Buffer& dstBuffer, const VkBufferCopy& region);
        CmdBuffer& CmdCopyBuffer(const Buffer& srcBuffer, Buffer& dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
        CmdBuffer& CmdCopyBuffer(const Buffer& srcBuffer, Buffer& dstBuffer);

        CmdBuffer& CmdCopyBuffer(const Buffer& srcBuffer, Texture& dstTexture, std::span<const BufferToTextureCopyInfo> regions);
        CmdBuffer& CmdCopyBuffer(const Buffer& srcBuffer, Texture& dstTexture, const BufferToTextureCopyInfo& region);

        CmdBuffer& CmdBindIndexBuffer(vkn::Buffer& idxBuffer, VkDeviceSize offset, VkIndexType idxType);

        CmdBuffer& CmdBindDescriptorBuffer(DescriptorBuffer& buffer);
        CmdBuffer& CmdBindDescriptorBufferSets(const PSO& pso, const DescriptorSetBindingInfo& bindigInfo);
        CmdBuffer& CmdBindDescriptorBufferSets(const PSO& pso, std::span<const DescriptorSetBindingInfo> bindigInfos);

        CmdBuffer& CmdPushConstants(PSO& pso, VkShaderStageFlags stagesMask, const void* pData, VkDeviceSize size, VkDeviceSize offset = 0);
        
        template <typename T>
        CmdBuffer& CmdPushConstants(PSO& pso, VkShaderStageFlags stagesMask, const T& data, VkDeviceSize offset = 0)
        {
            static_assert(std::is_trivially_copyable_v<T>, "T must be a trivially copyable type");
            return CmdPushConstants(pso, stagesMask, &data, sizeof(T), offset);
        }

        CmdBuffer& CmdDispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

        CmdBuffer& CmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
        CmdBuffer& CmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
        CmdBuffer& CmdDrawIndexedIndirect(Buffer& argBuffer, VkDeviceSize argBufferOffset, Buffer& countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t argStride);

        BarrierList& GetBarrierList();
        BarrierList& BeginBarrierList();
        CmdBuffer& CmdPushBarrierList(); // Post list of barriers to command buffer

        Device& GetDevice() const;

        template <typename... Args>
        CmdBuffer& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(GetDevice(), (uint64_t)m_cmdBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER, pFmt, std::forward<Args>(args)...);
            return *this;
        }

        const char* GetDebugName() const
        {
            return Object::GetDebugName("CommandBuffer");
        }

        CmdPool& GetOwnerPool() const
        {
            VK_ASSERT(IsCreated());
            return *m_pOwner;
        }

        const VkCommandBuffer& Get() const
        {
            VK_ASSERT(IsValid());
            return m_cmdBuffer;
        }

        bool IsStarted() const
        {
            VK_ASSERT(IsValid());
            return m_state.test(FLAG_IS_STARTED);
        }

        bool IsRenderingStarted() const
        {
            VK_ASSERT(IsValid());
            return m_state.test(FLAG_IS_RENDERING_STARTED);
        }

        bool IsValid() const;

    private:
        enum StateFlags
        {
            FLAG_IS_STARTED,
            FLAG_IS_RENDERING_STARTED,
            FLAG_COUNT,
        };

        using ID = uint16_t;

    private:
        static inline bool IsValidID(ID id) { return id != INVALID_ID; }

    private:
        CmdBuffer(CmdPool* pOwnerPool, VkCommandBufferLevel level, ID id);

        CmdBuffer& Allocate(CmdPool* pOwnerPool, VkCommandBufferLevel level, ID id);
        CmdBuffer& Free();

        CmdBuffer& ResetCache();

        ID GetID() const { return m_ID; }

    private:
        static constexpr ID INVALID_ID = static_cast<ID>(-1);

    private:
        CmdPool* m_pOwner = nullptr;
        VkCommandBuffer m_cmdBuffer = VK_NULL_HANDLE;

        BarrierList m_barrierList;

        std::vector<VkImageBlit2> m_blitCache;
        std::vector<VkBufferImageCopy2> m_bufImageCopyCache;
        DescriptorBuffer* m_pDescrBufferBindingCache;
        PSO* m_pPSOCache;

        ID m_ID = INVALID_ID;

        std::bitset<FLAG_COUNT> m_state = {};
    };


    struct CmdPoolCreateInfo
    {
        Device* pDevice;

        VkCommandPoolCreateFlags flags;
        uint32_t queueFamilyIndex;
        
        uint16_t size;
    };


    class CmdPool : public Object
    {
    public:
        ENG_DECL_CLASS_NO_COPIABLE(CmdPool);

        CmdPool() = default;
        CmdPool(const CmdPoolCreateInfo& info);

        ~CmdPool();

        CmdPool(CmdPool&& pool) noexcept;
        CmdPool& operator=(CmdPool&& pool) noexcept;

        CmdPool& Create(const CmdPoolCreateInfo& info);
        CmdPool& Destroy();

        CmdPool& Reset(VkCommandPoolResetFlags flags = 0);

        CmdBuffer* AllocCmdBuffer(VkCommandBufferLevel level);
        CmdPool& FreeCmdBuffer(CmdBuffer& cmdBuffer);

        template <typename... Args>
        CmdPool& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(GetDevice(), (uint64_t)m_pool, VK_OBJECT_TYPE_COMMAND_POOL, pFmt, std::forward<Args>(args)...);
            return *this;
        }

        const char* GetDebugName() const
        {
            return Object::GetDebugName("CommandPool");
        }

        Device& GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return *m_pDevice;
        }

        const VkCommandPool& Get() const
        {
            VK_ASSERT(IsCreated());
            return m_pool;
        }

    private:
        using BufferID = CmdBuffer::ID;

    private:
        BufferID AllocCmdBufferID();
        void FreeCmdBufferID(BufferID id);

    private:
        Device* m_pDevice = nullptr;
        VkCommandPool m_pool = VK_NULL_HANDLE;

        std::vector<CmdBuffer> m_allocatedBuffers;
        std::vector<BufferID> m_freeIds;
    };
}
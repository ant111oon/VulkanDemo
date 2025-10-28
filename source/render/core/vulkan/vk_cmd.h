#pragma once

#include "vk_object.h"
#include "vk_device.h"


namespace vkn
{
    class CmdPool;
    class QueryPool;
    class Buffer;


    class CmdBuffer : public Object
    {
        friend class CmdPool;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(CmdBuffer);

        CmdBuffer() = default;
    
        CmdBuffer(CmdBuffer&& cmdBuffer) noexcept;
        CmdBuffer& operator=(CmdBuffer&& cmdBuffer) noexcept;

        void Reset(VkCommandBufferResetFlags flags = 0);

        CmdBuffer& Begin(const VkCommandBufferBeginInfo& beginInfo);
        void End();
        
        CmdBuffer& CmdPipelineBarrier2(const VkDependencyInfo& depInfo);

        CmdBuffer& CmdResetQueryPool(QueryPool& queryPool, uint32_t firstQuery, uint32_t queryCount);
        CmdBuffer& CmdResetQueryPool(QueryPool& queryPool);
        CmdBuffer& CmdWriteTimestamp(QueryPool& queryPool, VkPipelineStageFlags2 stage, uint32_t queryIndex);

        CmdBuffer& CmdBeginRendering(const VkRenderingInfo& renderingInfo);
        CmdBuffer& CmdEndRendering();

        CmdBuffer& CmdSetViewport(uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports);
        CmdBuffer& CmdSetScissor(uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors);
        
        CmdBuffer& CmdBindIndexBuffer(vkn::Buffer& idxBuffer, VkDeviceSize offset, VkIndexType idxType);

        CmdBuffer& CmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
        CmdBuffer& CmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
        
        template <typename... Args>
        void SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(*GetDevice(), (uint64_t)m_cmdBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER, pFmt, std::forward<Args>(args)...);
        }

        const char* GetDebugName() const;

        Device* GetDevice() const;

        CmdPool* GetOwnerPool() const
        {
            VK_ASSERT(IsCreated());
            return m_pOwner;
        }

        VkCommandBuffer Get() const
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
        CmdBuffer(CmdPool* pOwnerPool, VkCommandBufferLevel level);

        bool Allocate(CmdPool* pOwnerPool, VkCommandBufferLevel level);
        void Free();

    private:
        enum StateFlags
        {
            FLAG_IS_STARTED,
            FLAG_IS_RENDERING_STARTED,
            FLAG_COUNT,
        };

    private:
        CmdPool* m_pOwner = nullptr;
        VkCommandBuffer m_cmdBuffer = VK_NULL_HANDLE;

        std::bitset<FLAG_COUNT> m_state = {};
    };


    struct CmdPoolCreateInfo
    {
        Device* pDevice;

        uint32_t queueFamilyIndex;
        VkCommandPoolCreateFlags flags;
    };


    class CmdPool : public Object
    {
    public:
        ENG_DECL_CLASS_NO_COPIABLE(CmdPool);

        CmdPool() = default;
        CmdPool(const CmdPoolCreateInfo& info);

        CmdPool(CmdPool&& pool) noexcept;
        CmdPool& operator=(CmdPool&& pool) noexcept;

        bool Create(const CmdPoolCreateInfo& info);
        void Destroy();

        void Reset(VkCommandPoolResetFlags flags = 0);

        CmdBuffer AllocCmdBuffer(VkCommandBufferLevel level);
        void FreeCmdBuffer(CmdBuffer& cmdBuffer);

        template <typename... Args>
        void SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(*GetDevice(), (uint64_t)m_pool, VK_OBJECT_TYPE_COMMAND_POOL, pFmt, std::forward<Args>(args)...);
        }

        const char* GetDebugName() const;

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

        VkCommandPool Get() const
        {
            VK_ASSERT(IsCreated());
            return m_pool;
        }

    private:
        Device* m_pDevice = nullptr;
        VkCommandPool m_pool = VK_NULL_HANDLE;
    };
}
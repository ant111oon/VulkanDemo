#pragma once

#include "vk_object.h"
#include "vk_device.h"


namespace vkn
{
    class CmdPool;
    class QueryPool;


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
        CmdBuffer& CmdWriteTimestamp(QueryPool& queryPool, VkPipelineStageFlags2 stage, uint32_t queryIndex);

        CmdBuffer& BeginRendering(const VkRenderingInfo& renderingInfo);
        CmdBuffer& EndRendering();

        CmdBuffer& CmdSetViewport(uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports);
        CmdBuffer& CmdSetScissor(uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors);

        CmdBuffer& CmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
        CmdBuffer& CmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
        

        void SetDebugName(const char* pName);
        const char* GetDebugName() const;

        CmdPool* GetOwnerPool() const
        {
            VK_ASSERT(IsCreated());
            return m_pOwner;
        }

        VkCommandBuffer Get() const
        {
            VK_ASSERT(IsCreated());
            return m_cmdBuffer;
        }

        Device* GetDevice() const;

        bool IsStarted() const { return m_state.test(FLAG_IS_STARTED); }
        bool IsRenderingStarted() const { return m_state.test(FLAG_IS_RENDERING_STARTED); }

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

        void SetDebugName(const char* pName) { Object::SetDebugName(m_pDevice->Get(), (uint64_t)m_pool, VK_OBJECT_TYPE_COMMAND_POOL, pName); }
        const char* GetDebugName() const { return Object::GetDebugName("CommandBuffer"); }

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
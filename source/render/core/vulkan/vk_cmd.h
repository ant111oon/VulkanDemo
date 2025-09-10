#pragma once

#include "vk_object.h"
#include "vk_device.h"


namespace vkn
{
    class CmdPool;


    class CmdBuffer : public Object
    {
        friend class CmdPool;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(CmdBuffer);

        CmdBuffer() = default;
    
        CmdBuffer(CmdBuffer&& cmdBuffer) noexcept;
        CmdBuffer& operator=(CmdBuffer&& cmdBuffer) noexcept;

        void Reset(VkCommandBufferResetFlags flags = 0);

        void Begin(const VkCommandBufferBeginInfo& beginInfo) const;
        void End() const;

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

    private:
        CmdBuffer(CmdPool* pOwnerPool, VkCommandBufferLevel level);

        bool Allocate(CmdPool* pOwnerPool, VkCommandBufferLevel level);
        void Free();

    private:
        CmdPool* m_pOwner = nullptr;
        VkCommandBuffer m_cmdBuffer = VK_NULL_HANDLE;
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
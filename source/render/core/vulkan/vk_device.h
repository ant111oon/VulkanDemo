#pragma once


#include "vk_phys_device.h"
#include "vk_surface.h"
#include "vk_object.h"

#include <span>


namespace vkn
{
    class CmdBuffer;
    class Fence;
    class Semaphore;
    class Swapchain;


    struct QueueSyncData
    {
        Semaphore*            pSemaphore = nullptr;
        VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
    };


    class Queue : public Object
    {
        friend class Device;
    public:
        ENG_DECL_CLASS_NO_COPIABLE(Queue);

        Queue() = default;
        ~Queue();

        Queue& Submit(std::span<CmdBuffer*> cmdBuffers, Fence* pFinishFence = nullptr, 
            std::span<QueueSyncData> waitSemaphores = {}, 
            std::span<QueueSyncData> signalSemaphores = {}
        );

        Queue& Submit(CmdBuffer& cmdBuffer, Fence* pFinishFence = nullptr, 
            QueueSyncData* pWaitSemaphore = nullptr, 
            QueueSyncData* pSignalSemaphore = nullptr
        );

        VkResult Present(Swapchain& swapchain, uint32_t imageIndex, Semaphore* pWaitSemaphores);
        VkResult Present(Swapchain& swapchain, uint32_t imageIndex, std::span<Semaphore*> waitSemaphores = {});

        const char* GetDebugName() const
        {
            return Object::GetDebugName("Queue");
        }

        Device& GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return *m_pOwner;
        }

        const VkQueue& Get() const
        {
            VK_ASSERT(IsCreated());
            return m_queue;
        }

        uint32_t GetFamilyIndex() const
        {
            VK_ASSERT(IsCreated());
            return m_familyIndex;
        }

    private:
        Queue(Device* pOwner, VkQueue queue, uint32_t familyIndex);

        Queue(Queue&& queue) noexcept;
        Queue& operator=(Queue&& queue) noexcept;

        Queue& Create(Device* pOwner, VkQueue queue, uint32_t familyIndex);
        Queue& Destroy();

        template <typename... Args>
        Queue& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(GetDevice(), (uint64_t)m_queue, VK_OBJECT_TYPE_QUEUE, pFmt, std::forward<Args>(args)...);
            return *this;
        }

    private:
        Device* m_pOwner = nullptr;

        VkQueue m_queue = VK_NULL_HANDLE;
        uint32_t m_familyIndex = UINT32_MAX;

        std::vector<VkSemaphore> m_presentSemaphoreCache;
        std::vector<VkCommandBufferSubmitInfo> m_cmdBuffCache;
        std::vector<VkSemaphoreSubmitInfo> m_waitSemaphoreCache;
        std::vector<VkSemaphoreSubmitInfo> m_signalSemaphoreCache;
    };


    struct DeviceCreateInfo
    {
        PhysicalDevice* pPhysDevice;
        Surface* pSurface;

        const VkPhysicalDeviceFeatures* pFeatures;
        const VkPhysicalDeviceFeatures2* pFeatures2;

        std::span<const char* const> extensions;

        float queuePriority;
    };


    class Device : public Object
    {
        friend Device& GetDevice();

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Device);
        ENG_DECL_CLASS_NO_MOVABLE(Device);

        ~Device();

        Device& Create(const DeviceCreateInfo& info);
        Device& Destroy();

        const Device& WaitIdle() const;

        PFN_vkVoidFunction GetProcAddr(const char* pFuncName) const;

        const VkDevice& Get() const
        {
            VK_ASSERT(IsCreated());
            return m_device;
        }

        PhysicalDevice& GetPhysDevice() const
        {
            VK_ASSERT(IsCreated());
            return *m_pPhysDevice;
        }

        const Queue& GetQueue() const
        {
            VK_ASSERT(IsCreated());
            return m_queue;
        }

        Queue& GetQueue()
        {
            VK_ASSERT(IsCreated());
            return m_queue;
        }

    private:
        Device() = default;

    private:
        PhysicalDevice* m_pPhysDevice = nullptr;
        VkDevice m_device = VK_NULL_HANDLE;

        Queue m_queue;
    };


    ENG_FORCE_INLINE Device& GetDevice()
    {
        static Device device;
        return device;
    }
}
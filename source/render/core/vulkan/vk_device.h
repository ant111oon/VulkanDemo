#pragma once


#include "vk_phys_device.h"
#include "vk_surface.h"

#include "vk_utils.h"

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


    class Queue : public Handle<VkQueue>
    {
        friend class Device;

    public:
        using Base = Handle<VkQueue>;

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

        Device& GetDevice() const;

        uint32_t GetFamilyIndex() const;

    private:
        Queue(Device* pOwner, VkQueue queue, uint32_t familyIndex);

        Queue(Queue&& queue) noexcept;
        Queue& operator=(Queue&& queue) noexcept;

        Queue& Create(Device* pOwner, VkQueue queue, uint32_t familyIndex);
        Queue& Destroy();

    private:
        Device* m_pOwner = nullptr;

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


    class Device : public Handle<VkDevice>
    {
        friend Device& GetDevice();

    public:
        using Base = Handle<VkDevice>;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Device);
        ENG_DECL_CLASS_NO_MOVABLE(Device);

        ~Device();

        Device& Create(const DeviceCreateInfo& info);
        Device& Destroy();

        const Device& WaitIdle() const;

        template <typename Handle, typename... Args>
        Device& SetObjDebugName(Handle& handle, std::string_view fmt, Args&&... args)
        {
            VK_ASSERT(IsCreated());

            handle.SetDebugName(fmt, std::forward<Args>(args)...);
            utils::SetHandleGPUName(*this, handle, fmt, std::forward<Args>(args)...);

            return *this;
        }

        PFN_vkVoidFunction GetProcAddr(std::string_view procName) const;

        PhysicalDevice& GetPhysDevice() const;

        const Queue& GetQueue() const;
        Queue& GetQueue();

    private:
        Device() = default;

    private:
        PhysicalDevice* m_pPhysDevice = nullptr;
        Queue m_queue;
    };


    ENG_FORCE_INLINE Device& GetDevice()
    {
        static Device device;
        return device;
    }
}
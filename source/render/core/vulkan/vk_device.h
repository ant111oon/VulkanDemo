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


    template<typename VkHandle>
    class DeviceResource : public Handle<VkHandle>
    {
        friend class Device;

    public:
        DeviceResource() = default;
        ~DeviceResource();

        DeviceResource(DeviceResource&& handle) noexcept;
        DeviceResource& operator=(DeviceResource&& handle) noexcept;

        DeviceResource& SetDebugName(std::string_view name);

        template <typename... Args>
        DeviceResource& SetDebugName(std::string_view fmt, Args&&... args);

        const std::string_view GetDebugName() const;

        Device& GetDevice() const;

    protected:
        template <typename CreatorFunc, typename... Args>
        DeviceResource& Create(Device* pOwner, const CreatorFunc& Func);

        template <typename DestroyerFunc>
        DeviceResource& Destroy(const DestroyerFunc& Func);

    private:
        using Base = Handle<VkHandle>;

    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        std::string m_debugName = "<unnamed>";
    #endif

        Device* m_pOwnerDevice = nullptr;
    };


    struct QueueSyncData
    {
        Semaphore*            pSemaphore = nullptr;
        VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
    };


    // Is created only by Device
    class Queue final : public DeviceResource<VkQueue>
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

        uint32_t GetFamilyIndex() const;

    private:
        Queue(Device* pOwner, VkQueue queue, uint32_t familyIndex);

        Queue(Queue&& queue) noexcept;
        Queue& operator=(Queue&& queue) noexcept;

        Queue& Create(Device* pOwner, VkQueue queue, uint32_t familyIndex);
        Queue& Destroy();

    private:
        using Base = DeviceResource<VkQueue>;

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


    class Device final : public Handle<VkDevice>
    {
    public:
        static Device& Inst();

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Device);
        ENG_DECL_CLASS_NO_MOVABLE(Device);

        ~Device();

        Device& Create(const DeviceCreateInfo& info);
        Device& Destroy();

        const Device& WaitIdle() const;

        PFN_vkVoidFunction GetProcAddr(std::string_view procName) const;

        PhysicalDevice& GetPhysDevice() const;

        const Queue& GetQueue() const;
        Queue& GetQueue();

    private:
        Device() = default;

    private:
        using Base = Handle<VkDevice>;

    private:
        PhysicalDevice* m_pPhysDevice = nullptr;
        Queue m_queue;
    };
}

#include "vk_device.hpp"
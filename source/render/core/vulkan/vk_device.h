#pragma once


#include "vk_phys_device.h"
#include "vk_surface.h"
#include "vk_object.h"

#include <span>


namespace vkn
{
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

        bool Create(const DeviceCreateInfo& info);
        void Destroy();

        void WaitIdle() const;

        VkDevice Get() const
        {
            VK_ASSERT(IsCreated());
            return m_device;
        }

        PhysicalDevice* GetPhysDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pPhysDevice;
        }

        VkQueue GetQueue() const
        {
            VK_ASSERT(IsCreated());
            return m_queue;
        }

        uint32_t GetQueueFamilyIndex() const
        {
            VK_ASSERT(IsCreated());
            return m_queueFamilyIndex;
        }

    private:
        Device() = default;

    private:
        PhysicalDevice* m_pPhysDevice = nullptr;
        VkDevice m_device = VK_NULL_HANDLE;

        VkQueue m_queue = VK_NULL_HANDLE;
        uint32_t m_queueFamilyIndex = UINT32_MAX;
    };


    ENG_FORCE_INLINE Device& GetDevice()
    {
        static Device device;
        return device;
    }
}
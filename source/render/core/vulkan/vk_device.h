#pragma once

#include "vk_core.h"

#include <bitset>
#include <span>


namespace vkn
{
    class PhysicalDevice;
    class Surface;
    

    struct DeviceCreateInfo
    {
        PhysicalDevice* pPhysDevice;
        Surface* pSurface;

        const VkPhysicalDeviceFeatures* pFeatures;
        const VkPhysicalDeviceFeatures2* pFeatures2;

        std::span<const char* const> extensions;

        float queuePriority;
    };


    class Device
    {
        friend Device& GetDevice();

    public:
        Device(const Device& device) = delete;
        Device(Device&& device) = delete;

        Device& operator=(const Device& device) = delete;
        Device& operator=(Device&& device) = delete;

        bool Create(const DeviceCreateInfo& info);
        void Destroy();

        VkDevice& Get()
        {
            VK_ASSERT(IsCreated());
            return m_device;
        }

        PhysicalDevice* GetPhysDevice()
        {
            VK_ASSERT(IsCreated());
            return m_pPhysDevice;
        }

        VkQueue& GetQueue()
        {
            VK_ASSERT(IsCreated());
            return m_queue;
        }

        uint32_t GetQueueFamilyIndex()
        {
            VK_ASSERT(IsCreated());
            return m_queueFamilyIndex;
        }

        bool IsCreated() const { return m_state.test(FLAG_IS_CREATED); }

    private:
        Device() = default;

    private:
        enum StateFlags
        {
            FLAG_IS_CREATED,
            FLAG_COUNT,
        };

    private:
        PhysicalDevice* m_pPhysDevice = nullptr;
        VkDevice m_device = VK_NULL_HANDLE;

        VkQueue m_queue = VK_NULL_HANDLE;
        uint32_t m_queueFamilyIndex = UINT32_MAX;

        std::bitset<FLAG_COUNT> m_state = {};
    };


    ENG_FORCE_INLINE Device& GetDevice()
    {
        static Device device;
        return device;
    }
}
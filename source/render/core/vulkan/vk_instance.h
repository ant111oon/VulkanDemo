#pragma once

#include "vk_core.h"

#include <bitset>
#include <span>


namespace vkn
{
    struct InstanceDebugMessengerCreateInfo
    {
        PFN_vkDebugUtilsMessengerCallbackEXT pMessageCallback;
        void* pUserData;
        VkDebugUtilsMessageSeverityFlagsEXT messageSeverity;
        VkDebugUtilsMessageTypeFlagsEXT messageType;
        VkDebugUtilsMessengerCreateFlagsEXT flags;
    };


    struct InstanceCreateInfo
    {
        const char* pApplicationName;
        const char* pEngineName;
        uint32_t applicationVersion;
        uint32_t engineVersion;
        uint32_t apiVersion;

        std::span<const char* const> extensions;
        std::span<const char* const> layers;

        const InstanceDebugMessengerCreateInfo* pDbgMessengerCreateInfo;
    };


    class Instance
    {
        friend Instance& GetInstance();

    public:
        Instance(const Instance& inst) = delete;
        Instance(Instance&& inst) = delete;

        Instance& operator=(const Instance& inst) = delete;
        Instance& operator=(Instance&& inst) = delete;

        bool Create(const InstanceCreateInfo& info);
        void Destroy();

        VkInstance& Get()
        {
            VK_ASSERT(IsCreated());
            return m_instance;
        }

        bool IsCreated() const { return m_flags.test(FLAG_IS_CREATED); }

    private:
        Instance() = default;

    private:
        enum
        {
            FLAG_IS_CREATED,
            FLAG_COUNT,
        };

    private:
        VkInstance m_instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_dbgMessenger = VK_NULL_HANDLE;

        std::bitset<FLAG_COUNT> m_flags = {};
    };


    ENG_FORCE_INLINE Instance& GetInstance()
    {
        static Instance instance;
        return instance;
    }
}
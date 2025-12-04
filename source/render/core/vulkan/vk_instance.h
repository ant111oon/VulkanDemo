#pragma once

#include "vk_object.h"

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


    class Instance : public Object
    {
        friend Instance& GetInstance();

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Instance);
        ENG_DECL_CLASS_NO_MOVABLE(Instance);

        ~Instance();

        Instance& Create(const InstanceCreateInfo& info);
        Instance& Destroy();

        PFN_vkVoidFunction GetProcAddr(const char* pFuncName) const;

        VkInstance Get() const
        {
            VK_ASSERT(IsCreated());
            return m_instance;
        }

        uint32_t GetApiVersion() const
        {
            VK_ASSERT(IsCreated());
            return m_apiVersion;
        }

    private:
        Instance() = default;

    private:
        VkInstance m_instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_dbgMessenger = VK_NULL_HANDLE;

        uint32_t m_apiVersion = UINT32_MAX;
    };


    ENG_FORCE_INLINE Instance& GetInstance()
    {
        static Instance instance;
        return instance;
    }
}
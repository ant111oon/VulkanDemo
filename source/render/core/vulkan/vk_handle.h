#pragma once

#include "vk_core.h"

#include <string>
#include <string_view>
#include <bitset>

#include <concepts>
#include <type_traits>


namespace vkn
{
    template <typename VkHandle>
    class Handle
    {
    public:
        friend class Device;

        using Type = VkHandle;

    public:
        ~Handle();

        const VkHandle&        Get() const;
        const std::string_view GetDebugName() const;
        
        bool IsCreated() const;

    protected:
        Handle() = default;

        Handle(const Handle& handle) = delete;
        Handle& operator=(const Handle& handle) = delete;

        Handle(Handle&& handle) noexcept;
        Handle& operator=(Handle&& handle) noexcept;

        template <typename CreatorFunc>
        void Create(const CreatorFunc& Func);

        template <typename DestroyerFunc>
        void Destroy(const DestroyerFunc& Func); 

    private:
        // Available only via vkn::Device
        void SetDebugName(std::string_view name);

        // Available only via vkn::Device
        template <typename... Args>
        void SetDebugName(std::string_view fmt, Args&&... args);

    private:
        enum HandleStateBits
        {
            HANDLE_BIT_IS_CREATED,
            HANDLE_BIT_COUNT,
        };

    private:
    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        std::string m_debugName;
    #endif

        VkHandle m_handle = VK_NULL_HANDLE;

        std::bitset<HANDLE_BIT_COUNT> m_handleState = {};
    };
}

#include "vk_handle.hpp"
#pragma once

#include "vk_core.h"
#include "core/core.h"

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
        using HandleType = VkHandle;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Handle);

        Handle() = default;
        ~Handle();

        Handle(Handle&& handle) noexcept;
        Handle& operator=(Handle&& handle) noexcept;

        const VkHandle& Get() const;
        bool IsCreated() const;

    protected:
        template <typename CreatorFunc>
        Handle& Create(const CreatorFunc& Func);

        template <typename DestroyerFunc>
        Handle& Destroy(const DestroyerFunc& Func); 

    private:
        enum HandleStateBits
        {
            HANDLE_BIT_IS_CREATED,
            HANDLE_BIT_COUNT,
        };

    private:
        VkHandle m_handle = VK_NULL_HANDLE;

        std::bitset<HANDLE_BIT_COUNT> m_handleState = {};
    };
}

#include "vk_handle.hpp"
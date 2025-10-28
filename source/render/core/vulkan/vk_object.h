#pragma once

#include "vk_core.h"

#include <array>
#include <bitset>


namespace vkn
{
    class Device;

    class Object
    {
    public:
        Object() = default;

        Object(const Object& obj) = default;
        Object& operator=(const Object& obj) = default;

        Object(Object&& obj) noexcept;
        Object& operator=(Object&& obj) noexcept;

        bool IsCreated() const { return m_internalState.test(INTERNAL_BIT_IS_CREATED); }

    protected:
        void Destroy();

        void SetCreated(bool isCreated) { m_internalState.set(INTERNAL_BIT_IS_CREATED, isCreated); }

        template <typename... Args>
        void SetDebugName(Device& device, uint64_t objectHandle, VkObjectType objectType, const char* pFmt, Args&&... args)
        {
        #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
            char name[MAX_OBJ_DBG_NAME_LENGTH] = {0};
            sprintf_s(name, pFmt, std::forward<Args>(args)...);

            SetDebugName(device, objectHandle, objectType, name);
        #endif
        }
        
        void SetDebugName(Device& device, uint64_t objectHandle, VkObjectType objectType, const char* pName);  
        const char* GetDebugName(const char* pReleaseName) const;

    private:
        enum InternalStateBits
        {
            INTERNAL_BIT_IS_CREATED,
            INTERNAL_BIT_COUNT,
        };

    private:
    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        static inline constexpr size_t MAX_OBJ_DBG_NAME_LENGTH = 60;

        std::array<char, MAX_OBJ_DBG_NAME_LENGTH> m_debugName = {};
    #endif

        std::bitset<INTERNAL_BIT_COUNT> m_internalState = {};
    };
}
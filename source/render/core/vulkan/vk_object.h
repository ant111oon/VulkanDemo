#pragma once

#include "vk_core.h"

#include <array>
#include <bitset>


namespace vkn
{
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
        
        void SetDebugName(VkDevice vkDevice, uint64_t objectHandle, VkObjectType objectType, const char* pName);  
        const char* GetDebugName(const char* pReleaseName) const;

    private:
        enum InternalStateBits
        {
            INTERNAL_BIT_IS_CREATED,
            INTERNAL_BIT_COUNT,
        };

    private:
        static inline constexpr size_t MAX_OBJ_DBG_NAME_LENGTH = 60;

    #ifdef ENG_BUILD_DEBUG
        std::array<char, MAX_OBJ_DBG_NAME_LENGTH> m_debugName = {};
    #endif

        std::bitset<INTERNAL_BIT_COUNT> m_internalState = {};
    };
}
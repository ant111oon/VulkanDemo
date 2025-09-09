#include "pch.h"

#include "vk_object.h"
#include "vk_utils.h"


namespace vkn
{
    Object::Object(Object&& obj) noexcept
    {
        *this = std::move(obj);
    }


    Object& Object::operator=(Object&& obj) noexcept
    {
        if (IsCreated()) {
            Destroy();
        }

    #ifdef ENG_BUILD_DEBUG
        m_debugName.swap(obj.m_debugName);
    #endif
        std::swap(m_internalState, obj.m_internalState);

        return *this;
    }


    void Object::Destroy()
    {
    #ifdef ENG_BUILD_DEBUG
        m_debugName.fill('\0');
    #endif
        SetCreated(false);
    }


    void Object::SetDebugName(VkDevice vkDevice, uint64_t objectHandle, VkObjectType objectType, const char* pName)
    {
    #ifdef ENG_BUILD_DEBUG
        VK_ASSERT(IsCreated());
        VK_ASSERT(pName);
        
        const size_t nameLength = strlen(pName);
        VK_ASSERT_MSG(nameLength < MAX_OBJ_DBG_NAME_LENGTH, "Debug name %s is too long: %zu (max length: %zu)", pName, nameLength, MAX_OBJ_DBG_NAME_LENGTH - 1);

        m_debugName.fill('\0');
        memcpy_s(m_debugName.data(), m_debugName.size(), pName, nameLength);

        utils::SetObjectName(vkDevice, objectHandle, objectType, pName);
    #endif
    }


    const char* Object::GetDebugName(const char* pReleaseName) const
    {
    #ifdef ENG_BUILD_DEBUG
        return m_debugName.data();
    #else
        return pReleaseName;
    #endif
    }
}
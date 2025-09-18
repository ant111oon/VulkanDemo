#pragma once

#include "../vk_object.h"


namespace vkn
{
    class Device;

    struct MemAllocatorCreateInfo
    {
        Device* pDevice;

        VmaAllocatorCreateFlags flags;
    };


    class MemAllocator : public Object
    {
        friend MemAllocator& GetMemAllocator();

    public:
        ENG_DECL_CLASS_NO_COPIABLE(MemAllocator);
        ENG_DECL_CLASS_NO_MOVABLE(MemAllocator);

        bool Create(const MemAllocatorCreateInfo& info);
        void Destroy();

        VmaAllocator Get() const
        {
            VK_ASSERT(IsCreated());
            return m_allocator;
        }

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

    private:
        MemAllocator() = default;

    private:
        Device* m_pDevice = nullptr;

        VmaAllocator m_allocator = VK_NULL_HANDLE;
    };


    ENG_FORCE_INLINE MemAllocator& GetMemAllocator()
    {
        static MemAllocator allocator;
        return allocator;
    }
}
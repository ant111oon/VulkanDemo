namespace vkn
{
    template <typename VkHandle>
    inline const std::string_view DeviceResource<VkHandle>::GetDebugName() const
    {
    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        return m_debugName;
    #else
        return "<unnamed>";
    #endif
    }


    template <typename VkHandle>
    inline Device& DeviceResource<VkHandle>::GetDevice() const
    {
        VK_ASSERT(IsCreated());
        return *m_pOwnerDevice;
    }


    template <typename VkHandle>
    inline DeviceResource<VkHandle>::~DeviceResource()
    {
        VK_ASSERT_MSG(!IsCreated(), "Vulkan DeviceResource must be destroyed derived class");
    }


    template <typename VkHandle>
    inline DeviceResource<VkHandle>::DeviceResource(DeviceResource &&handle) noexcept
    {
        *this == std::move(handle);
    }


    template <typename VkHandle>
    inline DeviceResource<VkHandle>& DeviceResource<VkHandle>::operator=(DeviceResource&& handle) noexcept
    {
    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        std::swap(m_debugName, handle.m_debugName);
    #endif

        std::swap(m_pOwnerDevice, handle.m_pOwnerDevice);

        Base::operator=(std::move(handle));

        return *this;
    }


    template <typename VkHandle>
    template <typename CreatorFunc, typename... Args>
    inline DeviceResource<VkHandle>& DeviceResource<VkHandle>::Create(Device* pOwner, const CreatorFunc& Func)
    {
        VK_ASSERT_MSG(!IsCreated(), "Vulkan DeviceResource must be destroyed by derived class");

        Base::Create(Func);
        
        VK_ASSERT_MSG(pOwner && pOwner->IsCreated(), "Invalid vulkan owner devicen");
        m_pOwnerDevice = pOwner;

        return *this;
    }


    template <typename VkHandle>
    template <typename DestroyerFunc>
    inline DeviceResource<VkHandle>& DeviceResource<VkHandle>::Destroy(const DestroyerFunc &Func)
    {
        if (!IsCreated()) {
            return *this;
        }

    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        m_debugName = "<unnamed>";
    #endif

        m_pOwnerDevice = nullptr;

        Base::Destroy(Func);

        return *this;
    }


    template <typename VkHandle>
    inline DeviceResource<VkHandle>& DeviceResource<VkHandle>::SetDebugName(std::string_view name)
    {
        VK_ASSERT(IsCreated());

    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        m_debugName = name;
        utils::SetHandleGPUName(GetDevice(), (uint64_t)Get(), utils::GetObjectType<typename Base::HandleType>(), name);
    #endif

        return *this;
    }


    template <typename VkHandle>
    template <typename... Args>
    inline DeviceResource<VkHandle>& DeviceResource<VkHandle>::SetDebugName(std::string_view fmt, Args &&...args)
    {
    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        char name[512] = { '\0' };
        sprintf_s(name, fmt.data(), std::forward<Args>(args)...);

        SetDebugName(name);
    #endif

        return *this;
    }
}


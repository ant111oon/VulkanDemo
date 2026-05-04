namespace vkn
{
    template <typename VkHandle>
    inline Handle<VkHandle>::~Handle()
    {
        VK_ASSERT_MSG(!IsCreated(), "Vulkan Handle must be destroyed by Handle derived class");
    }
    
    template <typename VkHandle>
    inline const VkHandle &Handle<VkHandle>::Get() const
    {
        VK_ASSERT(IsCreated());
        return m_handle;
    }


    template <typename VkHandle>
    inline const std::string_view Handle<VkHandle>::GetDebugName() const
    {
    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        return m_debugName;
    #else
        return "<VkHandle>";
    #endif
    }


    template <typename VkHandle>
    inline bool Handle<VkHandle>::IsCreated() const
    {
        return m_handleState.test(HANDLE_BIT_IS_CREATED);
    }


    template <typename VkHandle>
    inline Handle<VkHandle>::Handle(Handle&& handle) noexcept
    {
        *this == std::move(handle);
    }


    template <typename VkHandle>
    inline Handle<VkHandle>& Handle<VkHandle>::operator=(Handle&& handle) noexcept
    {
        VK_ASSERT_MSG(!IsCreated(), "Vulkan Handle must be destroyed by Handle derived class");

    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        std::swap(m_debugName, handle.m_debugName);
    #endif

        std::swap(m_handle, handle.m_handle);
        std::swap(m_handleState, handle.m_handleState);

        return *this;
    }


    template <typename VkHandle>
    template <typename CreatorFunc>
    inline void Handle<VkHandle>::Create(const CreatorFunc& Func)
    {
        static_assert(std::invocable<CreatorFunc, VkHandle&> && std::convertible_to<std::invoke_result_t<CreatorFunc, VkHandle&>, bool>,
            "CreatorFunc signature must be \"bool Func(VkHandle&)\"");

        VK_ASSERT_MSG(!IsCreated(), "Vulkan Handle must be destroyed by Handle derived class");

        const bool isCreated = Func(m_handle);
        m_handleState.set(HANDLE_BIT_IS_CREATED, isCreated);
    }


    template <typename VkHandle>
    template <typename DestroyerFunc>
    inline void Handle<VkHandle>::Destroy(const DestroyerFunc& Func)
    {
        static_assert(std::invocable<DestroyerFunc, VkHandle&>, "DestroyerFunc signature must be \"AnyType Func(VkHandle&)\"");

        Func(m_handle);
        m_handle = VK_NULL_HANDLE;
        m_handleState.set(HANDLE_BIT_IS_CREATED, false);
    }


    template <typename VkHandle>
    inline void Handle<VkHandle>::SetDebugName(std::string_view name)
    {
        VK_ASSERT(IsCreated());

    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        m_debugName = name;
    #endif
    }
    

    template <typename VkHandle>
    template <typename... Args>
    inline void Handle<VkHandle>::SetDebugName(std::string_view fmt, Args&&... args)
    {
    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        char name[256] = { '\0' };
        sprintf_s(name, fmt.data(), std::forward<Args>(args)...);

        SetDebugName(name);
    #endif
    }
}
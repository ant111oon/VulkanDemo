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
        std::swap(m_handle, handle.m_handle);
        std::swap(m_handleState, handle.m_handleState);

        return *this;
    }


    template <typename VkHandle>
    template <typename CreatorFunc>
    inline Handle<VkHandle>& Handle<VkHandle>::Create(const CreatorFunc& Func)
    {
        static_assert(std::invocable<CreatorFunc, VkHandle&> && std::convertible_to<std::invoke_result_t<CreatorFunc, VkHandle&>, bool>,
            "CreatorFunc signature must be \"bool Func(VkHandle&)\"");

        VK_ASSERT_MSG(!IsCreated(), "Vulkan Handle must be destroyed by derived class");

        const bool isCreated = Func(m_handle);
        m_handleState.set(HANDLE_BIT_IS_CREATED, isCreated);

        return *this;
    }


    template <typename VkHandle>
    template <typename DestroyerFunc>
    inline Handle<VkHandle>& Handle<VkHandle>::Destroy(const DestroyerFunc& Func)
    {
        static_assert(std::invocable<DestroyerFunc, VkHandle&>, "DestroyerFunc signature must be \"AnyType Func(VkHandle&)\"");

        if (!IsCreated()) {
            return *this;
        }

        Func(m_handle);

        m_handle = VK_NULL_HANDLE;
        m_handleState.reset();

        return *this;
    }
}
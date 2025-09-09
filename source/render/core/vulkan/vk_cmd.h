#pragma once

#include "vk_object.h"
#include "vk_device.h"


namespace vkn
{
    // class CmdPool;


    // class CmdBuffer : public Object
    // {
    // public:
    //     ENG_DECL_CLASS_NO_COPIABLE(CmdBuffer);
    

    // private:
    //     CmdPool* m_pOwner = nullptr;

    //     VkCommandBuffer m_cmdBuffer = VK_NULL_HANDLE;
    // };


    // struct CmdPoolCreateInfo
    // {
    //     uint32_t queueFamilyIndex;
    //     VkCommandPoolCreateFlags flags;
    // };


    // class CmdPool : public Object
    // {
    // public:
    //     ENG_DECL_CLASS_NO_COPIABLE(CmdPool);

    //     CmdPool() = default;
    //     CmdPool(const CmdPoolCreateInfo& info);

    //     CmdPool(CmdPool&& pool) noexcept;
    //     CmdPool& operator=(CmdPool&& pool) noexcept;

    //     bool Create(const CmdPoolCreateInfo& info);
    //     void Destroy();

    // private:
    //     Device* m_pDevice = nullptr;

    //     VkCommandPool m_pool = VK_NULL_HANDLE;
    // };
}
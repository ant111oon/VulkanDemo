#pragma once

#ifndef ENG_BUILD_RELEASE
    #define ENG_DEBUG_UI_ENABLED
#endif


#ifdef ENG_DEBUG_UI_ENABLED

#include "core/core.h" 

#ifndef ENG_GFX_API_VULKAN
    #error Invalid graphics API
#endif

#include "render/core/vulkan/vk_device.h"
#include "render/core/vulkan/vk_cmd.h"

#include "render/core/vulkan/vk_texture.h"

#include "core/platform/window/window.h"

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_win32.h>

#include <unordered_map>
#include <bitset>


namespace eng
{
    class DebugUI
    {
    public:
        ENG_DECL_CLASS_NO_COPIABLE(DebugUI);
        ENG_DECL_CLASS_NO_MOVABLE(DebugUI);
    
        DebugUI() = default;
        ~DebugUI();
    
        DebugUI& Create(Window& window, vkn::Device& device, VkFormat rtFormat);
        DebugUI& Destroy();
    
        DebugUI& ProcessEvent(const WndEvent& event);
    
        DebugUI& BeginFrame(float dt);
        DebugUI& EndFrame();
    
        DebugUI& Render(vkn::CmdBuffer& cmdBuffer);
    
        ImTextureID AddTexture(const vkn::TextureView& view, const vkn::Sampler& sampler, VkImageLayout layout);
        DebugUI& AddTexture(const vkn::TextureView& view, const vkn::Sampler& sampler, VkImageLayout layout, ImTextureID& outID);

        DebugUI& RemoveTexture(ImTextureID ID);

        bool IsAnyWindowFocused() const
        {
            CORE_ASSERT(IsCreated());
            return ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
        }
    
        bool IsCreated() const
        {
            return m_state.test(BIT_IS_CREATED);
        }
    
    private:
        enum StateBits
        {
            BIT_IS_CREATED,
            BIT_COUNT,
        };

        using TextureHash = uint64_t;
    
    private:
        Window* m_pWindow = nullptr;
    
        ImGuiContext* m_pContext = nullptr;
        ImGuiIO* m_pIO = nullptr;

        std::unordered_map<TextureHash, ImTextureID> m_hashToTextureID;
        std::unordered_map<ImTextureID,TextureHash> m_textureIDToHash;

        std::bitset<BIT_COUNT> m_state = {};
    };
}

#endif
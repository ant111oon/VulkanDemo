#pragma once

#ifndef ENG_BUILD_RELEASE
    #define ENG_DEBUG_UI_ENABLED
#endif


#include "core/core.h" 

#ifndef ENG_GFX_API_VULKAN
    #error Invalid graphics API
#endif

#include "render/core/vulkan/vk_device.h"
#include "render/core/vulkan/vk_cmd.h"

#include "render/core/vulkan/vk_texture.h"

#include "core/platform/window/window.h"


#ifdef ENG_DEBUG_UI_ENABLED
    #include <imgui.h>
    #include <backends/imgui_impl_vulkan.h>
    #include <backends/imgui_impl_win32.h>

    #include <unordered_map>
    #include <bitset>
#endif


namespace eng
{
    class DbgUI
    {
    public:
    #ifdef ENG_DEBUG_UI_ENABLED
        using TexID = ImTextureID;
    #else
        using TexID = uint64_t;
    #endif

    public:
        ENG_DECL_CLASS_NO_COPIABLE(DbgUI);
        ENG_DECL_CLASS_NO_MOVABLE(DbgUI);
    
        DbgUI() = default;
        ~DbgUI();
    
        DbgUI& Create(Window& window, vkn::Device& device, VkFormat rtFormat);
        DbgUI& Destroy();
    
        DbgUI& ProcessEvent(const WndEvent& event);
    
        DbgUI& BeginFrame(float dt);
        DbgUI& EndFrame();
    
        DbgUI& Render(vkn::CmdBuffer& cmdBuffer);
    
        TexID AddTexture(const vkn::TextureView& view, const vkn::Sampler& sampler);
        DbgUI& AddTexture(const vkn::TextureView& view, const vkn::Sampler& sampler, TexID& outID);

        DbgUI& RemoveTexture(TexID ID);

        bool IsAnyWindowFocused() const;
    
        bool IsCreated() const;
    
    #ifdef ENG_DEBUG_UI_ENABLED
    private:
        enum StateBits
        {
            BIT_IS_CREATED,
            BIT_COUNT,
        };

        using TextureHash = uint64_t;

        struct Hasher0
        {
            static constexpr uint64_t operator()(TextureHash texHash) { return texHash; }
        };

        struct Hasher1
        {
            static constexpr uint64_t operator()(TexID ID) { return ID; }
        };
    
    private:
        Window* m_pWindow = nullptr;
    
        ImGuiContext* m_pContext = nullptr;
        ImGuiIO* m_pIO = nullptr;

        std::unordered_map<TextureHash, TexID, Hasher0> m_hashToTextureID;
        std::unordered_map<TexID, TextureHash, Hasher1> m_textureIDToHash;

        std::bitset<BIT_COUNT> m_state = {};
    #endif
    };
}
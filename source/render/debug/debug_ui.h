#pragma once

#include "core/core.h" 

#ifndef ENG_GFX_API_VULKAN
    #error Invalid graphics API
#endif

#include "render/core/vulkan/vk_device.h"
#include "render/core/vulkan/vk_cmd.h"

#include "core/platform/window/window.h"

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_win32.h>

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
    
    private:
        Window* m_pWindow = nullptr;
    
        ImGuiContext* m_pContext = nullptr;
        ImGuiIO* m_pIO = nullptr;
    
        std::bitset<BIT_COUNT> m_state = {};
    };
}
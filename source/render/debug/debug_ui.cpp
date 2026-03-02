#include "pch.h"
#include "debug_ui.h"


#ifdef ENG_DEBUG_UI_ENABLED

#include "core/utils/hash.h"


namespace eng
{
    static ImGuiMouseButton_ WndMouseButtonTypeToImGui(WndMouseButtonType type)
    {
        switch (type) {
            case WndMouseButtonType::LEFT: return ImGuiMouseButton_Left;
            case WndMouseButtonType::RIGHT: return ImGuiMouseButton_Right;
            case WndMouseButtonType::MIDDLE: return ImGuiMouseButton_Middle;
            default:
                CORE_ASSERT_FAIL("Invalid mouse button type");
                return ImGuiMouseButton_COUNT;
        }
    }


    static ImGuiKey WndKeyToImGui(WndKey key)
    {
        switch (key) {
            // Letters
            case WndKey::KEY_A: return ImGuiKey_A;
            case WndKey::KEY_B: return ImGuiKey_B;
            case WndKey::KEY_C: return ImGuiKey_C;
            case WndKey::KEY_D: return ImGuiKey_D;
            case WndKey::KEY_E: return ImGuiKey_E;
            case WndKey::KEY_F: return ImGuiKey_F;
            case WndKey::KEY_G: return ImGuiKey_G;
            case WndKey::KEY_H: return ImGuiKey_H;
            case WndKey::KEY_I: return ImGuiKey_I;
            case WndKey::KEY_J: return ImGuiKey_J;
            case WndKey::KEY_K: return ImGuiKey_K;
            case WndKey::KEY_L: return ImGuiKey_L;
            case WndKey::KEY_M: return ImGuiKey_M;
            case WndKey::KEY_N: return ImGuiKey_N;
            case WndKey::KEY_O: return ImGuiKey_O;
            case WndKey::KEY_P: return ImGuiKey_P;
            case WndKey::KEY_Q: return ImGuiKey_Q;
            case WndKey::KEY_R: return ImGuiKey_R;
            case WndKey::KEY_S: return ImGuiKey_S;
            case WndKey::KEY_T: return ImGuiKey_T;
            case WndKey::KEY_U: return ImGuiKey_U;
            case WndKey::KEY_V: return ImGuiKey_V;
            case WndKey::KEY_W: return ImGuiKey_W;
            case WndKey::KEY_X: return ImGuiKey_X;
            case WndKey::KEY_Y: return ImGuiKey_Y;
            case WndKey::KEY_Z: return ImGuiKey_Z;
            // Numbers
            case WndKey::KEY_0: return ImGuiKey_0;
            case WndKey::KEY_1: return ImGuiKey_1;
            case WndKey::KEY_2: return ImGuiKey_2;
            case WndKey::KEY_3: return ImGuiKey_3;
            case WndKey::KEY_4: return ImGuiKey_4;
            case WndKey::KEY_5: return ImGuiKey_5;
            case WndKey::KEY_6: return ImGuiKey_6;
            case WndKey::KEY_7: return ImGuiKey_7;
            case WndKey::KEY_8: return ImGuiKey_8;
            case WndKey::KEY_9: return ImGuiKey_9;

            case WndKey::KEY_ESCAPE:    return ImGuiKey_Escape;
            case WndKey::KEY_ENTER:     return ImGuiKey_Enter;
            case WndKey::KEY_TAB:       return ImGuiKey_Tab;
            case WndKey::KEY_BACKSPACE: return ImGuiKey_Backspace;
            case WndKey::KEY_INSERT:    return ImGuiKey_Insert;
            case WndKey::KEY_DELETE:    return ImGuiKey_Delete;

            case WndKey::KEY_RIGHT: return ImGuiKey_RightArrow;
            case WndKey::KEY_LEFT:  return ImGuiKey_LeftArrow;
            case WndKey::KEY_DOWN:  return ImGuiKey_DownArrow;
            case WndKey::KEY_UP:    return ImGuiKey_UpArrow;

            case WndKey::KEY_PAGE_UP:   return ImGuiKey_PageUp;
            case WndKey::KEY_PAGE_DOWN: return ImGuiKey_PageDown;
            case WndKey::KEY_HOME:      return ImGuiKey_Home;
            case WndKey::KEY_END:       return ImGuiKey_End;

            case WndKey::KEY_CAPS_LOCK:     return ImGuiKey_CapsLock;
            case WndKey::KEY_SCROLL_LOCK:   return ImGuiKey_ScrollLock;
            case WndKey::KEY_NUM_LOCK:      return ImGuiKey_NumLock;
            case WndKey::KEY_PRINT_SCREEN:  return ImGuiKey_PrintScreen;
            case WndKey::KEY_PAUSE:         return ImGuiKey_Pause;

            case WndKey::KEY_F1:  return ImGuiKey_F1;
            case WndKey::KEY_F2:  return ImGuiKey_F2;
            case WndKey::KEY_F3:  return ImGuiKey_F3;
            case WndKey::KEY_F4:  return ImGuiKey_F4;
            case WndKey::KEY_F5:  return ImGuiKey_F5;
            case WndKey::KEY_F6:  return ImGuiKey_F6;
            case WndKey::KEY_F7:  return ImGuiKey_F7;
            case WndKey::KEY_F8:  return ImGuiKey_F8;
            case WndKey::KEY_F9:  return ImGuiKey_F9;
            case WndKey::KEY_F10: return ImGuiKey_F10;
            case WndKey::KEY_F11: return ImGuiKey_F11;
            case WndKey::KEY_F12: return ImGuiKey_F12;

            case WndKey::KEY_LEFT_SHIFT:    return ImGuiKey_LeftShift;
            case WndKey::KEY_LEFT_CONTROL:  return ImGuiKey_LeftCtrl;
            case WndKey::KEY_LEFT_ALT:      return ImGuiKey_LeftAlt;
            case WndKey::KEY_RIGHT_SHIFT:   return ImGuiKey_RightShift;
            case WndKey::KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
            case WndKey::KEY_RIGHT_ALT:     return ImGuiKey_RightAlt;

            case WndKey::KEY_LEFT_WIN:     return ImGuiKey_LeftSuper;
            case WndKey::KEY_RIGHT_WIN:     return ImGuiKey_RightSuper;

            default:
                return ImGuiKey_None;
        }
    }


    constexpr char WndKeyToChar(WndKey key)
    {
        switch (key) {
            case WndKey::KEY_SPACE:      return ' ';
            case WndKey::KEY_APOSTROPHE: return '\'';
            case WndKey::KEY_COMMA:      return ',';
            case WndKey::KEY_MINUS:      return '-';
            case WndKey::KEY_DOT:        return '.';
            case WndKey::KEY_SLASH:      return '/';

            case WndKey::KEY_0: return '0';
            case WndKey::KEY_1: return '1';
            case WndKey::KEY_2: return '2';
            case WndKey::KEY_3: return '3';
            case WndKey::KEY_4: return '4';
            case WndKey::KEY_5: return '5';
            case WndKey::KEY_6: return '6';
            case WndKey::KEY_7: return '7';
            case WndKey::KEY_8: return '8';
            case WndKey::KEY_9: return '9';

            case WndKey::KEY_SEMICOLON: return ';';
            case WndKey::KEY_EQUAL:     return '=';
            
            case WndKey::KEY_A: return 'a';
            case WndKey::KEY_B: return 'b';
            case WndKey::KEY_C: return 'c';
            case WndKey::KEY_D: return 'd';
            case WndKey::KEY_E: return 'e';
            case WndKey::KEY_F: return 'f';
            case WndKey::KEY_G: return 'g';
            case WndKey::KEY_H: return 'h';
            case WndKey::KEY_I: return 'i';
            case WndKey::KEY_J: return 'j';
            case WndKey::KEY_K: return 'k';
            case WndKey::KEY_L: return 'l';
            case WndKey::KEY_M: return 'm';
            case WndKey::KEY_N: return 'n';
            case WndKey::KEY_O: return 'o';
            case WndKey::KEY_P: return 'p';
            case WndKey::KEY_Q: return 'q';
            case WndKey::KEY_R: return 'r';
            case WndKey::KEY_S: return 's';
            case WndKey::KEY_T: return 't';
            case WndKey::KEY_U: return 'u';
            case WndKey::KEY_V: return 'v';
            case WndKey::KEY_W: return 'w';
            case WndKey::KEY_X: return 'x';
            case WndKey::KEY_Y: return 'y';
            case WndKey::KEY_Z: return 'z';

            case WndKey::KEY_LEFT_BRACKET:   return '[';
            case WndKey::KEY_BACKSLASH:      return '\\';
            case WndKey::KEY_RIGHT_BRACKET:  return ']';
            case WndKey::KEY_GRAVE_ACCENT:   return '`';

            case WndKey::KEY_TAB:            return '\t';
            case WndKey::KEY_ENTER:          return '\r';
            case WndKey::KEY_BACKSPACE:      return '\b';

            default: return 0;
        }
    }


    static void UpdateKeyModifiers(ImGuiIO* pIO, WndKey key, bool down)
    {
        if (key == WndKey::KEY_LEFT_SHIFT || key == WndKey::KEY_RIGHT_SHIFT) {
            pIO->AddKeyEvent(ImGuiMod_Shift, down);
        } else if (key == WndKey::KEY_LEFT_CONTROL || key == WndKey::KEY_RIGHT_CONTROL) {
            pIO->AddKeyEvent(ImGuiMod_Ctrl, down);
        } else if (key == WndKey::KEY_LEFT_ALT || key == WndKey::KEY_RIGHT_ALT) {
            pIO->AddKeyEvent(ImGuiMod_Alt, down);
        } else if (key == WndKey::KEY_LEFT_WIN || key == WndKey::KEY_RIGHT_WIN) {
            pIO->AddKeyEvent(ImGuiMod_Super, down);
        }   
    }


    DebugUI::~DebugUI()
    {
        Destroy();
    }


    DebugUI& DebugUI::Create(Window& window, vkn::Device& device, VkFormat rtFormat)
    {
        if (IsCreated()) {
            CORE_LOG_WARN("DebugUI is already created");
            return *this;
        }

        CORE_ASSERT(window.IsCreated());

        IMGUI_CHECKVERSION();
        m_pContext = ImGui::CreateContext();
        CORE_ASSERT(m_pContext);

        m_pIO = &ImGui::GetIO();
        m_pIO->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        m_pIO->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        m_pWindow = &window;

        ImGui::GetStyle().Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
        ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
        ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        ImGui::GetStyle().Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        ImGui::GetStyle().Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

    #ifdef ENG_OS_WINDOWS
        if (!ImGui_ImplWin32_Init(window.GetNativeHandle())) {
            CORE_ASSERT_FAIL("Failed to initialize ImGui Win32 part");
        }

        ImGui::GetPlatformIO().Platform_CreateVkSurface = [](ImGuiViewport* viewport, ImU64 vkInstance, const void* vkAllocator, ImU64* outVkSurface)
        {
            VkWin32SurfaceCreateInfoKHR createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            createInfo.hwnd = (HWND)viewport->PlatformHandleRaw;
            createInfo.hinstance = ::GetModuleHandle(nullptr);
            return (int)vkCreateWin32SurfaceKHR((VkInstance)vkInstance, &createInfo, (VkAllocationCallbacks*)vkAllocator, (VkSurfaceKHR*)outVkSurface);
        };
    #endif

        vkn::PhysicalDevice& physDevice = device.GetPhysDevice();
        vkn::Instance& inst = physDevice.GetInstance();

        ImGui_ImplVulkan_InitInfo imGuiInitInfo = {};
        imGuiInitInfo.ApiVersion = inst.GetApiVersion();
        imGuiInitInfo.Instance = inst.Get();
        imGuiInitInfo.PhysicalDevice = physDevice.Get();
        imGuiInitInfo.Device = device.Get();
        imGuiInitInfo.QueueFamily = device.GetQueue().GetFamilyIndex();
        imGuiInitInfo.Queue = device.GetQueue().Get();
        imGuiInitInfo.DescriptorPoolSize = 1000;
        imGuiInitInfo.MinImageCount = 3;
        imGuiInitInfo.ImageCount = 3;
        imGuiInitInfo.PipelineCache = VK_NULL_HANDLE;

        imGuiInitInfo.UseDynamicRendering = true;
        imGuiInitInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT; // 0 defaults to VK_SAMPLE_COUNT_1_BIT
    #ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
        imGuiInitInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        
        imGuiInitInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        imGuiInitInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &rtFormat;
    #else
        #error Vulkan Dynamic Rendering Is Not Supported. Get Vulkan SDK Latests.
    #endif
        imGuiInitInfo.CheckVkResultFn = [](VkResult error) { VK_CHECK(error); };
        imGuiInitInfo.MinAllocationSize = 1024 * 1024;

        if (!ImGui_ImplVulkan_Init(&imGuiInitInfo)) {
            CORE_ASSERT_FAIL("Failed to initialize ImGui Vulkan part");
        }

        m_state.set(BIT_IS_CREATED, true);

        return *this;
    }


    DebugUI& DebugUI::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        while (!m_hashToTextureID.empty()) {
            RemoveTexture(m_hashToTextureID.begin()->second);
        }

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(m_pContext);

        m_hashToTextureID = {};
        m_textureIDToHash = {};

        m_pContext = nullptr;
        m_pIO = nullptr;
        m_pWindow = nullptr;

        m_state.set(BIT_IS_CREATED, false);

        return *this;
    }


    DebugUI& DebugUI::ProcessEvent(const WndEvent& event)
    {
        CORE_ASSERT(IsCreated());

        if (event.Is<WndCursorEvent>()) {
            const WndCursorEvent& e = event.Get<WndCursorEvent>();

            m_pIO->AddMousePosEvent(e.x, e.y);
        } else if (event.Is<WndMouseButtonEvent>()) {
            const WndMouseButtonEvent& e = event.Get<WndMouseButtonEvent>();

            m_pIO->AddMouseButtonEvent(WndMouseButtonTypeToImGui(e.type), e.state != WndMouseButtonState::RELEASED);
        } else if (event.Is<WndMouseWheelEvent>()) {
            const WndMouseWheelEvent& e = event.Get<WndMouseWheelEvent>();

            m_pIO->AddMouseWheelEvent(0, e.delta);
        } else if (event.Is<WndKeyEvent>()) {
            const WndKeyEvent& e = event.Get<WndKeyEvent>();

            if (ImGuiKey key = WndKeyToImGui(e.key); key != ImGuiKey_None) {
                m_pIO->AddKeyEvent(key, true);
            }

            if (char ch = WndKeyToChar(e.key); ch != 0) {
                m_pIO->AddInputCharacter(ch);
            }

            UpdateKeyModifiers(m_pIO, e.key, e.IsPressed());
        } else if (event.Is<WndActiveEvent>()) {
            const WndActiveEvent& e = event.Get<WndActiveEvent>();

            m_pIO->AddFocusEvent(e.isActive);
        } else if (event.Is<WndResizeEvent>()) {
            const WndResizeEvent& e = event.Get<WndResizeEvent>();

            m_pIO->DisplaySize = ImVec2(static_cast<float>(e.width), static_cast<float>(e.height));
        }

        return *this;
    }


    DebugUI& DebugUI::BeginFrame(float dt)
    {
        CORE_ASSERT(IsCreated());

        m_pIO->DisplaySize = ImVec2(static_cast<float>(m_pWindow->GetWidth()), static_cast<float>(m_pWindow->GetHeight()));
        m_pIO->DeltaTime = dt > 0.f ? dt : 1.f / 60.f;

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        return *this;
    }


    DebugUI& DebugUI::EndFrame()
    {
        CORE_ASSERT(IsCreated());

        ImGui::EndFrame();

        return *this;
    }


    DebugUI& DebugUI::Render(vkn::CmdBuffer& cmdBuffer)
    {
        CORE_ASSERT(IsCreated());

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer.Get());

        return *this;
    }


    ImTextureID DebugUI::AddTexture(const vkn::TextureView& view, const vkn::Sampler& sampler)
    {
        ImTextureID ID;
        AddTexture(view, sampler, ID);

        return ID;
    }


    DebugUI& DebugUI::AddTexture(const vkn::TextureView& view, const vkn::Sampler& sampler, ImTextureID& outID)
    {
        CORE_ASSERT(IsCreated());
 
        static constexpr VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        eng::HashBuilder builder;
        builder.AddValue((uintptr_t)view.Get());
        builder.AddValue((uintptr_t)sampler.Get());

        const TextureHash hash = builder.Value();

        const auto it = m_hashToTextureID.find(hash);

        if (it == m_hashToTextureID.cend()) {
            outID = (ImTextureID)ImGui_ImplVulkan_AddTexture(sampler.Get(), view.Get(), layout);
            
            m_hashToTextureID[hash] = outID;
            m_textureIDToHash[outID] = hash;
        } else {
            outID = it->second;
        }

        return *this;
    }


    DebugUI& DebugUI::RemoveTexture(ImTextureID ID)
    {
        CORE_ASSERT(IsCreated());

        const auto hashIt = m_textureIDToHash.find(ID);

        if (hashIt == m_textureIDToHash.cend()) {
            return *this;
        }

        ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)ID);
            
        const uint64_t hash = m_textureIDToHash[ID];
        
        m_textureIDToHash.erase(ID);
        m_hashToTextureID.erase(hash);

        return *this;
    }
}

#endif
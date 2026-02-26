#pragma once

#ifdef ENG_OS_WINDOWS

#include "core/platform/window/window.h"
#include "core/platform/native/win32/window/win32_window.h"

#include <vector>
#include <functional>


namespace eng
{
    class Win32Window final : public Window
    {
    public:
        Win32Window() = default;
        Win32Window(const WindowInitInfo& initInfo);

        ~Win32Window() override;
    
        bool Create(const WindowInitInfo& initInfo) override;
        void Destroy() override;
    
        void PullEvents() override;
        
        void* GetNativeHandle() override { return m_HWND; }
        const void* GetNativeHandle() const override { return m_HWND; }
    
        void SetVisible(bool visible) override;
        void SetCursorHidden(bool hidden) override;
    
        void SetCursorRelativeMode(bool relative) override;
    
    protected:
        void UpdateTitleInternal() override;
    
    private:
        static LRESULT WndProcSetup(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        static LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
        static bool RegisterWndClass(HINSTANCE hInst);
    
        static void FetchMouseEventCommonInfo(WPARAM wParam, LPARAM lParam, int16_t& x, int16_t& y, bool& isCtrlDown, bool& isShiftDown, bool& isLButtonDown, bool& isMButtonDown, bool& isRButtonDown) noexcept;
    
    private:
        void SetCursorPosition(int16_t x, int16_t y);
    
        LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    
        LRESULT OnCloseEvent();
        LRESULT OnActiveEvent(WPARAM wParam);
        LRESULT OnSizeEvent(WPARAM wParam, LPARAM lParam);
    
        template <typename OnMouseEventType>
        LRESULT OnMouseEvent(WPARAM wParam, LPARAM lParam, WndMouseButtonType type, WndMouseButtonState state)
        {
            int16_t x = 0, y = 0;
            bool isCtrlDown = false, isShiftDown = false, isLButtonDown = false, isMButtonDown = false, isRButtonDown = false;
    
            FetchMouseEventCommonInfo(wParam, lParam, x, y, isCtrlDown, isShiftDown, isLButtonDown, isMButtonDown, isRButtonDown);
    
            SetCursorPosition(x, y);
    
            PushEvent<OnMouseEventType>(type, state, x, y, isCtrlDown, isShiftDown, isLButtonDown, isMButtonDown, isRButtonDown);
    
            return 0;
        }
    
        LRESULT OnMouseCursorEvent(WPARAM wParam, LPARAM lParam)
        {
            int16_t x = 0, y = 0;
            bool isCtrlDown = false, isShiftDown = false, isLButtonDown = false, isMButtonDown = false, isRButtonDown = false;
    
            FetchMouseEventCommonInfo(wParam, lParam, x, y, isCtrlDown, isShiftDown, isLButtonDown, isMButtonDown, isRButtonDown);
    
            SetCursorPosition(x, y);
    
            PushEvent<WndCursorEvent>(x, y, isCtrlDown, isShiftDown, isLButtonDown, isMButtonDown, isRButtonDown);
    
            return 0;
        }
    
        LRESULT OnMouseWheelEvent(WPARAM wParam, LPARAM lParam);
        LRESULT OnKeyEvent(WPARAM wParam, LPARAM lParam, bool isKeyDown);
    
    private:
        static inline constexpr const char* P_WND_CLASS_NAME = "WindowClass";
        static inline bool s_isWindowClassRegistered = false;
    
    private:
        HWND m_HWND = nullptr;
    };
}

#endif
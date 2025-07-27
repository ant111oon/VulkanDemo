#pragma once

#include "core/platform/window/window.h"

#if defined(ENG_OS_WINDOWS)

#include "core/platform/native/win32/win32_includes.h"


class Win32Window final : public BaseWindow
{
public:
    ~Win32Window() override;

    bool Init(const WindowInitInfo& initInfo) override;
    void Destroy() override;

    void ProcessEvents() const override;
    
    void* GetNativeHandle() override { return m_HWND; }
    const void* GetNativeHandle() const override { return m_HWND; }

    void SetVisible(bool visible) override;

    HINSTANCE GetHINST() const { return m_HINST; }

private:
    static LRESULT WndProcSetup(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static bool RegisterWndClass(HINSTANCE hInst);

    static void FetchMouseEventCommonInfo(WPARAM wParam, LPARAM lParam, int16_t& x, int16_t& y, bool& isCtrlDown, bool& isShiftDown, bool& isLButtonDown, bool& isMButtonDown, bool& isRButtonDown) noexcept;

private:
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    LRESULT OnCloseEvent();
    LRESULT OnActiveEvent(WPARAM wParam);
    LRESULT OnSizeEvent(WPARAM wParam, LPARAM lParam);

    template <typename OnMouseEventType>
    LRESULT OnMouseEvent(WPARAM wParam, LPARAM lParam)
    {
        int16_t x = 0, y = 0;
        bool isCtrlDown = false, isShiftDown = false, isLButtonDown = false, isMButtonDown = false, isRButtonDown = false;

        FetchMouseEventCommonInfo(wParam, lParam, x, y, isCtrlDown, isShiftDown, isLButtonDown, isMButtonDown, isRButtonDown);

        PushEvent<OnMouseEventType>(x, y, isCtrlDown, isShiftDown, isLButtonDown, isMButtonDown, isRButtonDown);

        return 0;
    }

    LRESULT OnMouseWheelEvent(WPARAM wParam, LPARAM lParam);
    LRESULT OnKeyEvent(WPARAM wParam, LPARAM lParam, bool isKeyDown);

private:
    static inline constexpr const char* P_WND_CLASS_NAME = "WindowClass";
    static inline bool s_isWindowClassRegistered = false;

private:
    HWND m_HWND = nullptr;
    HINSTANCE m_HINST = nullptr;   
};

#endif
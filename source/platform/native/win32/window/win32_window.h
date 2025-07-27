#pragma once

#include "platform/window/window.h"

#if defined(ENG_OS_WINDOWS)

#include "platform/native/win32/win32_includes.h"


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

private:
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    static inline constexpr const char* P_WND_CLASS_NAME = "WindowClass";
    static inline bool s_isWindowClassRegistered = false;

private:
    HWND m_HWND = nullptr;
    HINSTANCE m_HINST = nullptr;   
};

#endif
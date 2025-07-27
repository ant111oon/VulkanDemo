#include "pch.h"

#include "win32_window.h"
#include "core/utils/assert.h"


#define WIN32_ASSERT_MSG(COND, FMT, ...) ENG_ASSERT_MSG(COND, "WIN32", FMT, __VA_ARGS__)
#define WIN32_ASSERT(COND)               WIN32_ASSERT_MSG(COND, #COND)
#define WIN32_ASSERT_FAIL(FMT, ...)      WIN32_ASSERT_MSG(false, FMT, __VA_ARGS__)


#if defined(ENG_OS_WINDOWS)


static Win32Window* win32GetWndInst(HWND hWnd) noexcept
{
    LONG_PTR pWndLongPtr = GetWindowLongPtr(hWnd, GWLP_USERDATA);
    return reinterpret_cast<Win32Window*>(pWndLongPtr);
}


LRESULT Win32Window::WndProcSetup(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_NCCREATE) {
        const CREATESTRUCTW* pCreateStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        Win32Window* pWnd = reinterpret_cast<Win32Window*>(pCreateStruct->lpCreateParams);
        pWnd->m_HWND = hWnd;

        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWnd));
        SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Win32Window::WndProc));

        return pWnd->HandleMessage(uMsg, wParam, lParam);
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


LRESULT Win32Window::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32Window* pWnd = win32GetWndInst(hWnd);
    WIN32_ASSERT(pWnd);

    return pWnd->HandleMessage(uMsg, wParam, lParam);
}


bool Win32Window::RegisterWndClass(HINSTANCE hInst)
{
    if (s_isWindowClassRegistered) {
        return true;
    }

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.hInstance = hInst;
    wc.lpszClassName = P_WND_CLASS_NAME;
    wc.lpfnWndProc = Win32Window::WndProcSetup;
    
    s_isWindowClassRegistered = RegisterClassEx(&wc) != 0;
    WIN32_ASSERT_MSG(s_isWindowClassRegistered, "Win32 window class registeration failed");

    return s_isWindowClassRegistered;
}


LRESULT Win32Window::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(m_HWND, &ps);
            EndPaint(m_HWND, &ps);
            return 0;
        }

        case WM_CLOSE:
            SetClosedState(true);
            PushEvent<WndCloseEvent>();
            return 0;

        // case WM_ACTIVATE:
        //     SetActiveState(LOWORD(wParam) != WA_INACTIVE);            
        //     return 0;
        // case WM_SIZE:
        // case WM_SETFOCUS:
        // case WM_MOUSEMOVE:
        // case WM_LBUTTONDOWN:
        // case WM_LBUTTONUP:
        // case WM_LBUTTONDBLCLK:
        // case WM_RBUTTONDOWN:
        // case WM_RBUTTONUP:
        // case WM_RBUTTONDBLCLK:
        // case WM_MOUSEWHEEL:
        // case WM_KEYDOWN:
        // case WM_KEYUP:
        // case WM_SETFOCUS:
        // case WM_KILLFOCUS:

        default:
            return DefWindowProc(m_HWND, uMsg, wParam, lParam);
    }
}


Win32Window::~Win32Window()
{
    Destroy();
}


bool Win32Window::Init(const WindowInitInfo& initInfo)
{
    if (IsInitialized()) {
        return true;
    }

    m_HINST = GetModuleHandle(nullptr);
    WIN32_ASSERT(m_HINST);

    RegisterWndClass(m_HINST);

    SetWidth(initInfo.width);
    SetHeight(initInfo.height);

    m_HWND = CreateWindowEx(
        0, 
        P_WND_CLASS_NAME,
        initInfo.title.data(), 
        WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT, 
        CW_USEDEFAULT, 
        static_cast<int>(GetWidth()), 
        static_cast<int>(GetHeight()), 
        nullptr, 
        nullptr, 
        m_HINST, 
        this
    );

    WIN32_ASSERT_MSG(m_HWND != nullptr, "Win32 window creation failed");

    SetInitializedState(true);
    SetVisible(true);

    return true;
}


void Win32Window::Destroy()
{
    if (!IsInitialized()) {
        return;
    }

    DestroyWindow(m_HWND);
    UnregisterClass(P_WND_CLASS_NAME, m_HINST);

    m_HWND = nullptr;
    m_HINST = nullptr;

    BaseWindow::Destroy();
}


void Win32Window::ProcessEvents() const
{
    WIN32_ASSERT(IsInitialized());

    MSG msg = {};

    while(PeekMessage(&msg, m_HWND, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}


void Win32Window::SetVisible(bool visible)
{
    WIN32_ASSERT(IsInitialized());

    if (visible == IsVisible()) {
        return;
    }

    ShowWindow(m_HWND, visible ? SW_SHOW : SW_HIDE);
    SetVisibleState(visible);
}

#endif
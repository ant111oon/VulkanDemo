#include "pch.h"

#include "win32_window.h"
#include "core/utils/assert.h"


#if defined(ENG_OS_WINDOWS)

#define WIN32_ASSERT_MSG(COND, FMT, ...) ENG_ASSERT_MSG(COND, "WIN32", FMT, __VA_ARGS__)
#define WIN32_ASSERT(COND)               WIN32_ASSERT_MSG(COND, #COND)
#define WIN32_ASSERT_FAIL(FMT, ...)      WIN32_ASSERT_MSG(false, FMT, __VA_ARGS__)


static std::wstring Utf8ToUtf16(std::string_view strUTF8)
{
    if (strUTF8.empty()) {
        return {};
    }

    const size_t size = MultiByteToWideChar(CP_UTF8, 0, strUTF8.data(), strUTF8.size(), nullptr, 0);
    std::wstring string(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, strUTF8.data(), strUTF8.size(), string.data(), size);
    
    return string;
}


static WORD win32ResolveActualVK(WPARAM wParam, LPARAM lParam)
{
    const WORD vk = LOWORD(wParam);
    const WORD keyFlags = HIWORD(lParam);
    const bool isExtendedKey = (keyFlags & KF_EXTENDED) != 0;
    
    WORD sc = LOBYTE(keyFlags);
    if (isExtendedKey)
        sc = MAKEWORD(sc, 0xE0);

    switch (vk) {
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
        case VK_RETURN:
        case VK_DELETE:
        case VK_HOME:
            return LOWORD(MapVirtualKeyW(sc, MAPVK_VSC_TO_VK_EX));
    }

    return vk;
}


static WndKey win32VirtualKeyToWndKey(WORD vk) noexcept
{
    switch (vk) {
        case VK_SPACE: return WndKey::KEY_SPACE;
        case VK_OEM_7: return WndKey::KEY_APOSTROPHE;
        case VK_OEM_COMMA: return WndKey::KEY_COMMA;
        case VK_OEM_MINUS: return WndKey::KEY_MINUS;
        case VK_OEM_PERIOD: return WndKey::KEY_DOT;
        case VK_OEM_2: return WndKey::KEY_SLASH;
        case '0': return WndKey::KEY_0;
        case '1': return WndKey::KEY_1;
        case '2': return WndKey::KEY_2;
        case '3': return WndKey::KEY_3;
        case '4': return WndKey::KEY_4;
        case '5': return WndKey::KEY_5;
        case '6': return WndKey::KEY_6;
        case '7': return WndKey::KEY_7;
        case '8': return WndKey::KEY_8;
        case '9': return WndKey::KEY_9;
        case VK_OEM_1: return WndKey::KEY_SEMICOLON;
        case VK_OEM_PLUS: return WndKey::KEY_EQUAL;
        case 'A': return WndKey::KEY_A;
        case 'B': return WndKey::KEY_B;
        case 'C': return WndKey::KEY_C;
        case 'D': return WndKey::KEY_D;
        case 'E': return WndKey::KEY_E;
        case 'F': return WndKey::KEY_F;
        case 'G': return WndKey::KEY_G;
        case 'H': return WndKey::KEY_H;
        case 'I': return WndKey::KEY_I;
        case 'J': return WndKey::KEY_J;
        case 'K': return WndKey::KEY_K;
        case 'L': return WndKey::KEY_L;
        case 'M': return WndKey::KEY_M;
        case 'N': return WndKey::KEY_N;
        case 'O': return WndKey::KEY_O;
        case 'P': return WndKey::KEY_P;
        case 'Q': return WndKey::KEY_Q;
        case 'R': return WndKey::KEY_R;
        case 'S': return WndKey::KEY_S;
        case 'T': return WndKey::KEY_T;
        case 'U': return WndKey::KEY_U;
        case 'V': return WndKey::KEY_V;
        case 'W': return WndKey::KEY_W;
        case 'X': return WndKey::KEY_X;
        case 'Y': return WndKey::KEY_Y;
        case 'Z': return WndKey::KEY_Z;
        case VK_OEM_4: return WndKey::KEY_LEFT_BRACKET;
        case VK_OEM_5: return WndKey::KEY_BACKSLASH;
        case VK_OEM_6: return WndKey::KEY_RIGHT_BRACKET;
        case VK_OEM_3: return WndKey::KEY_GRAVE_ACCENT;
        case VK_ESCAPE: return WndKey::KEY_ESCAPE;
        case VK_RETURN: return WndKey::KEY_ENTER;
        case VK_TAB: return WndKey::KEY_TAB;
        case VK_BACK: return WndKey::KEY_BACKSPACE;
        case VK_INSERT: return WndKey::KEY_INSERT;
        case VK_DELETE: return WndKey::KEY_DELETE;
        case VK_RIGHT: return WndKey::KEY_RIGHT;
        case VK_LEFT: return WndKey::KEY_LEFT;
        case VK_DOWN: return WndKey::KEY_DOWN;
        case VK_UP: return WndKey::KEY_UP;
        case VK_PRIOR: return WndKey::KEY_PAGE_UP;
        case VK_NEXT: return WndKey::KEY_PAGE_DOWN;
        case VK_HOME: return WndKey::KEY_HOME;
        case VK_END: return WndKey::KEY_END;
        case VK_CAPITAL: return WndKey::KEY_CAPS_LOCK;
        case VK_SCROLL: return WndKey::KEY_SCROLL_LOCK;
        case VK_NUMLOCK: return WndKey::KEY_NUM_LOCK;
        case VK_SNAPSHOT: return WndKey::KEY_PRINT_SCREEN;
        case VK_PAUSE: return WndKey::KEY_PAUSE;
        case VK_F1: return WndKey::KEY_F1;
        case VK_F2: return WndKey::KEY_F2;
        case VK_F3: return WndKey::KEY_F3;
        case VK_F4: return WndKey::KEY_F4;
        case VK_F5: return WndKey::KEY_F5;
        case VK_F6: return WndKey::KEY_F6;
        case VK_F7: return WndKey::KEY_F7;
        case VK_F8: return WndKey::KEY_F8;
        case VK_F9: return WndKey::KEY_F9;
        case VK_F10: return WndKey::KEY_F10;
        case VK_F11: return WndKey::KEY_F11;
        case VK_F12: return WndKey::KEY_F12;
        case VK_F13: return WndKey::KEY_F13;
        case VK_F14: return WndKey::KEY_F14;
        case VK_F15: return WndKey::KEY_F15;
        case VK_F16: return WndKey::KEY_F16;
        case VK_F17: return WndKey::KEY_F17;
        case VK_F18: return WndKey::KEY_F18;
        case VK_F19: return WndKey::KEY_F19;
        case VK_F20: return WndKey::KEY_F20;
        case VK_F21: return WndKey::KEY_F21;
        case VK_F22: return WndKey::KEY_F22;
        case VK_F23: return WndKey::KEY_F23;
        case VK_F24: return WndKey::KEY_F24;
        case VK_NUMPAD0: return WndKey::KEY_KP_0;
        case VK_NUMPAD1: return WndKey::KEY_KP_1;
        case VK_NUMPAD2: return WndKey::KEY_KP_2;
        case VK_NUMPAD3: return WndKey::KEY_KP_3;
        case VK_NUMPAD4: return WndKey::KEY_KP_4;
        case VK_NUMPAD5: return WndKey::KEY_KP_5;
        case VK_NUMPAD6: return WndKey::KEY_KP_6;
        case VK_NUMPAD7: return WndKey::KEY_KP_7;
        case VK_NUMPAD8: return WndKey::KEY_KP_8;
        case VK_NUMPAD9: return WndKey::KEY_KP_9;
        case VK_DECIMAL: return WndKey::KEY_KP_DECIMAL;
        case VK_DIVIDE: return WndKey::KEY_KP_DIVIDE;
        case VK_MULTIPLY: return WndKey::KEY_KP_MULTIPLY;
        case VK_SUBTRACT: return WndKey::KEY_KP_SUBTRACT;
        case VK_ADD: return WndKey::KEY_KP_ADD;
        case VK_LSHIFT: return WndKey::KEY_LEFT_SHIFT;
        case VK_LCONTROL: return WndKey::KEY_LEFT_CONTROL;
        case VK_LMENU: return WndKey::KEY_LEFT_ALT;
        case VK_RSHIFT: return WndKey::KEY_RIGHT_SHIFT;
        case VK_RCONTROL: return WndKey::KEY_RIGHT_CONTROL;
        case VK_RMENU: return WndKey::KEY_RIGHT_ALT;
        case VK_MEDIA_PREV_TRACK: return WndKey::KEY_MEDIA_PREV_TRACK;
        case VK_MEDIA_NEXT_TRACK: return WndKey::KEY_MEDIA_NEXT_TRACK;
        case VK_MEDIA_PLAY_PAUSE: return WndKey::KEY_MEDIA_PLAY_PAUSE;
        default:
            return WndKey::KEY_COUNT;
    }
}


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

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = hInst;
    wc.lpszClassName = P_WND_CLASS_NAME;
    wc.lpfnWndProc = Win32Window::WndProcSetup;
    
    s_isWindowClassRegistered = RegisterClassExW(&wc) != 0;
    WIN32_ASSERT_MSG(s_isWindowClassRegistered, "Win32 window class registeration failed");

    return s_isWindowClassRegistered;
}


void Win32Window::FetchMouseEventCommonInfo(WPARAM wParam, LPARAM lParam, int16_t& x, int16_t& y, bool &isCtrlDown, bool &isShiftDown, bool &isLButtonDown, bool &isMButtonDown, bool &isRButtonDown) noexcept
{
    x = static_cast<int16_t>(LOWORD(lParam));
    y = static_cast<int16_t>(HIWORD(lParam));

    isCtrlDown = (wParam & MK_CONTROL) != 0;
    isShiftDown = (wParam & MK_SHIFT) != 0;
    isLButtonDown = (wParam & MK_LBUTTON) != 0;
    isMButtonDown = (wParam & MK_MBUTTON) != 0;
    isRButtonDown = (wParam & MK_RBUTTON) != 0;
}


LRESULT Win32Window::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_CLOSE:
            return OnCloseEvent();
        case WM_ACTIVATE:
            return OnActiveEvent(wParam);
        case WM_SIZE:
            return OnSizeEvent(wParam, lParam);
        case WM_MOUSEMOVE:
            return OnMouseEvent<WndCursorEvent>(wParam, lParam);
        case WM_LBUTTONDOWN:
            return OnMouseEvent<WndMouseLButtonDownEvent>(wParam, lParam);
        case WM_LBUTTONUP:
            return OnMouseEvent<WndMouseLButtonUpEvent>(wParam, lParam);
        case WM_LBUTTONDBLCLK:
            return OnMouseEvent<WndMouseLButtonDblClkEvent>(wParam, lParam);
        case WM_RBUTTONDOWN:
            return OnMouseEvent<WndMouseRButtonDownEvent>(wParam, lParam);
        case WM_RBUTTONUP:
            return OnMouseEvent<WndMouseRButtonUpEvent>(wParam, lParam);
        case WM_RBUTTONDBLCLK:
            return OnMouseEvent<WndMouseRButtonDblClkEvent>(wParam, lParam);
        case WM_MBUTTONDOWN:
            return OnMouseEvent<WndMouseMButtonDownEvent>(wParam, lParam);
        case WM_MBUTTONUP:
            return OnMouseEvent<WndMouseMButtonUpEvent>(wParam, lParam);
        case WM_MBUTTONDBLCLK:
            return OnMouseEvent<WndMouseMButtonDblClkEvent>(wParam, lParam);
        case WM_MOUSEWHEEL:
            return OnMouseWheelEvent(wParam, lParam);
        case WM_KEYDOWN:
            return OnKeyEvent(wParam, lParam, true);
        case WM_KEYUP:
            return OnKeyEvent(wParam, lParam, false);
        case WM_SYSKEYDOWN:
            OnKeyEvent(wParam, lParam, true);
            return DefWindowProc(m_HWND, uMsg, wParam, lParam);
        case WM_SYSKEYUP:
            OnKeyEvent(wParam, lParam, false);
            return DefWindowProc(m_HWND, uMsg, wParam, lParam);
        default:
            return DefWindowProc(m_HWND, uMsg, wParam, lParam);
    }
}


LRESULT Win32Window::OnCloseEvent()
{
    SetClosedState(true);
    PushEvent<WndCloseEvent>();
    
    return 0;
}


LRESULT Win32Window::OnActiveEvent(WPARAM wParam)
{
    const bool isActive = LOWORD(wParam) != WA_INACTIVE;

    SetActiveState(isActive);
    PushEvent<WndActiveEvent>(isActive);
    
    return 0;
}


LRESULT Win32Window::OnSizeEvent(WPARAM wParam, LPARAM lParam)
{
    const uint16_t width  = static_cast<uint16_t>(LOWORD(lParam));
    const uint16_t height = static_cast<uint16_t>(HIWORD(lParam));

    SetWidth(width);
    SetHeight(height);

    WndResizeEventType type = WndResizeEventType::RESTORED;

    switch (wParam) {
        case SIZE_MINIMIZED:
            SetMinimizedState(true);
            type = WndResizeEventType::MINIMIZED;
            break;
        case SIZE_MAXIMIZED:
            SetMaximizedState(true);
            type = WndResizeEventType::MAXIMIZED;
            break;
        case SIZE_RESTORED:
            ResetSizeState();
            break;
    }

    PushEvent<WndResizeEvent>(width, height, type);

    return 0;
}


LRESULT Win32Window::OnMouseWheelEvent(WPARAM wParam, LPARAM lParam)
{
    int16_t x = 0, y = 0;
    bool isCtrlDown = false, isShiftDown = false, isLButtonDown = false, isMButtonDown = false, isRButtonDown = false;

    FetchMouseEventCommonInfo(wParam, lParam, x, y, isCtrlDown, isShiftDown, isLButtonDown, isMButtonDown, isRButtonDown);

    const int16_t delta = GET_WHEEL_DELTA_WPARAM(wParam);

    PushEvent<WndMouseWheelEvent>(delta, x, y, isCtrlDown, isShiftDown, isLButtonDown, isMButtonDown, isRButtonDown);

    return 0;
}


LRESULT Win32Window::OnKeyEvent(WPARAM wParam, LPARAM lParam, bool isKeyDown)
{   
    const WORD keyFlags = HIWORD(lParam);
    const bool isKeyHold = (keyFlags & KF_REPEAT) == KF_REPEAT;
    
    const WndKeyState state = !isKeyDown ? WndKeyState::RELEASED : (isKeyHold ? WndKeyState::HOLD : WndKeyState::PRESSED);

    const WORD vk = win32ResolveActualVK(wParam, lParam);
    WndKey key = win32VirtualKeyToWndKey(vk);

    PushEvent<WndKeyEvent>(key, state);
        
    return 0;
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

    const HINSTANCE hInst = GetModuleHandle(nullptr);
    WIN32_ASSERT(hInst);

    RegisterWndClass(hInst);

    RECT clientRect = {};
    clientRect.right = static_cast<LONG>(initInfo.width);
    clientRect.bottom = static_cast<LONG>(initInfo.height);
    AdjustWindowRectEx(&clientRect, WS_OVERLAPPEDWINDOW, FALSE, 0);

    SetWidth(clientRect.right - clientRect.left);
    SetHeight(clientRect.bottom - clientRect.top);

    const std::wstring appNameWStr = Utf8ToUtf16(initInfo.title.data());

    m_HWND = CreateWindowExW(
        0, 
        P_WND_CLASS_NAME,
        appNameWStr.c_str(),
        WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT, 
        CW_USEDEFAULT, 
        static_cast<int>(GetWidth()), 
        static_cast<int>(GetHeight()), 
        nullptr, 
        nullptr, 
        hInst, 
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
    UnregisterClassW(P_WND_CLASS_NAME, GetModuleHandle(nullptr));
    s_isWindowClassRegistered = false;

    m_HWND = nullptr;

    BaseWindow::Destroy();
}


void Win32Window::ProcessEvents()
{
    WIN32_ASSERT(IsInitialized());

    MSG msg = {};

    while(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message != WM_QUIT) {
            TranslateMessage(&msg);
            DispatchMessage(&msg); 
        } else {
            SetClosedState(true);
            break;
        }
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
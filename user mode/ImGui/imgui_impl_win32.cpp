// dear imgui: Platform Backend for Windows (standard windows API for 32 and 64 bits applications)
// This needs to be used along with a Renderer (e.g. DirectX11, OpenGL3, Vulkan..)

// Implemented features:
//  [X] Platform: Clipboard support (for Win32 this is actually part of core dear imgui)
//  [X] Platform: Keyboard support. Since 1.87 we are using the io.AddKeyEvent() function. Pass ImGuiKey values to all key functions e.g. ImGui::IsKeyPressed(ImGuiKey_Space). [Legacy VK_* values will also be supported unless IMGUI_DISABLE_OBSOLETE_KEYIO is set]
//  [X] Platform: Gamepad support. Enabled with 'io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad'.
//  [X] Platform: Mouse cursor shape and visibility. Disable with 'io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange'.
//  [X] Platform: Multi-viewport support (multiple windows). Enable with 'io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable'.

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "imgui.h"
#include "imgui_impl_win32.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()
#include <tchar.h>
#include <dwmapi.h>

// Configuration flags to add in your imconfig.h file:
//#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD              // Disable gamepad support. This was meaningful before <1.81 but we now load XInput dynamically so the option is now less relevant.

// Using XInput for gamepad (will load DLL dynamically)
#ifndef IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
#include <xinput.h>
typedef DWORD(WINAPI* PFN_XInputGetCapabilities)(DWORD, DWORD, XINPUT_CAPABILITIES*);
typedef DWORD(WINAPI* PFN_XInputGetState)(DWORD, XINPUT_STATE*);
#endif

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2022-XX-XX: Platform: Added support for multiple windows via the ImGuiPlatformIO interface.
//  2022-01-26: Inputs: replaced short-lived io.AddKeyModsEvent() (added two weeks ago)with io.AddKeyEvent() using ImGuiKey_ModXXX flags. Sorry for the confusion.
//  2021-01-20: Inputs: calling new io.AddKeyAnalogEvent() for gamepad support, instead of writing directly to io.NavInputs[].
//  2022-01-17: Inputs: calling new io.AddMousePosEvent(), io.AddMouseButtonEvent(), io.AddMouseWheelEvent() API (1.87+).
//  2022-01-17: Inputs: always update key mods next and before a key event (not in NewFrame) to fix input queue with very low framerates.
//  2022-01-12: Inputs: Update mouse inputs using WM_MOUSEMOVE/WM_MOUSELEAVE + fallback to provide it when focused but not hovered/captured. More standard and will allow us to pass it to future input queue API.
//  2022-01-12: Inputs: Maintain our own copy of MouseButtonsDown mask instead of using ImGui::IsAnyMouseDown() which will be obsoleted.
//  2022-01-10: Inputs: calling new io.AddKeyEvent(), io.AddKeyModsEvent() + io.SetKeyEventNativeData() API (1.87+). Support for full ImGuiKey range.
//  2021-12-16: Inputs: Fill VK_LCONTROL/VK_RCONTROL/VK_LSHIFT/VK_RSHIFT/VK_LMENU/VK_RMENU for completeness.
//  2021-08-17: Calling io.AddFocusEvent() on WM_SETFOCUS/WM_KILLFOCUS messages.
//  2021-08-02: Inputs: Fixed keyboard modifiers being reported when host window doesn't have focus.
//  2021-07-29: Inputs: MousePos is correctly reported when the host platform window is hovered but not focused (using TrackMouseEvent() to receive WM_MOUSELEAVE events).
//  2021-06-29: Reorganized backend to pull data from a single structure to facilitate usage with multiple-contexts (all g_XXXX access changed to bd->XXXX).
//  2021-06-08: Fixed ImGui_ImplWin32_EnableDpiAwareness() and ImGui_ImplWin32_GetDpiScaleForMonitor() to handle Windows 8.1/10 features without a manifest (per-monitor DPI, and properly calls SetProcessDpiAwareness() on 8.1).
//  2021-03-23: Inputs: Clearing keyboard down array when losing focus (WM_KILLFOCUS).
//  2021-02-18: Added ImGui_ImplWin32_EnableAlphaCompositing(). Non Visual Studio users will need to link with dwmapi.lib (MinGW/gcc: use -ldwmapi).
//  2021-02-17: Fixed ImGui_ImplWin32_EnableDpiAwareness() attempting to get SetProcessDpiAwareness from shcore.dll on Windows 8 whereas it is only supported on Windows 8.1.
//  2021-01-25: Inputs: Dynamically loading XInput DLL.
//  2020-12-04: Misc: Fixed setting of io.DisplaySize to invalid/uninitialized data when after hwnd has been closed.
//  2020-03-03: Inputs: Calling AddInputCharacterUTF16() to support surrogate pairs leading to codepoint >= 0x10000 (for more complete CJK inputs)
//  2020-02-17: Added ImGui_ImplWin32_EnableDpiAwareness(), ImGui_ImplWin32_GetDpiScaleForHwnd(), ImGui_ImplWin32_GetDpiScaleForMonitor() helper functions.
//  2020-01-14: Inputs: Added support for #define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD/IMGUI_IMPL_WIN32_DISABLE_LINKING_XINPUT.
//  2019-12-05: Inputs: Added support for ImGuiMouseCursor_NotAllowed mouse cursor.
//  2019-05-11: Inputs: Don't filter value from WM_CHAR before calling AddInputCharacter().
//  2019-01-17: Misc: Using GetForegroundWindow()+IsChild() instead of GetActiveWindow() to be compatible with windows created in a different thread or parent.
//  2019-01-17: Inputs: Added support for mouse buttons 4 and 5 via WM_XBUTTON* messages.
//  2019-01-15: Inputs: Added support for XInput gamepads (if ImGuiConfigFlags_NavEnableGamepad is set by user application).
//  2018-11-30: Misc: Setting up io.BackendPlatformName so it can be displayed in the About Window.
//  2018-06-29: Inputs: Added support for the ImGuiMouseCursor_Hand cursor.
//  2018-06-10: Inputs: Fixed handling of mouse wheel messages to support fine position messages (typically sent by track-pads).
//  2018-06-08: Misc: Extracted imgui_impl_win32.cpp/.h away from the old combined DX9/DX10/DX11/DX12 examples.
//  2018-03-20: Misc: Setup io.BackendFlags ImGuiBackendFlags_HasMouseCursors and ImGuiBackendFlags_HasSetMousePos flags + honor ImGuiConfigFlags_NoMouseCursorChange flag.
//  2018-02-20: Inputs: Added support for mouse cursors (ImGui::GetMouseCursor() value and WM_SETCURSOR message handling).
//  2018-02-06: Inputs: Added mapping for ImGuiKey_Space.
//  2018-02-06: Inputs: Honoring the io.WantSetMousePos by repositioning the mouse (when using navigation and ImGuiConfigFlags_NavMoveMouse is set).
//  2018-02-06: Misc: Removed call to ImGui::Shutdown() which is not available from 1.60 WIP, user needs to call CreateContext/DestroyContext themselves.
//  2018-01-20: Inputs: Added Horizontal Mouse Wheel support.
//  2018-01-08: Inputs: Added mapping for ImGuiKey_Insert.
//  2018-01-05: Inputs: Added WM_LBUTTONDBLCLK double-click handlers for window classes with the CS_DBLCLKS flag.
//  2017-10-23: Inputs: Added WM_SYSKEYDOWN / WM_SYSKEYUP handlers so e.g. the VK_MENU key can be read.
//  2017-10-23: Inputs: Using Win32 ::SetCapture/::GetCapture() to retrieve mouse positions outside the client area when dragging.
//  2016-11-12: Inputs: Only call Win32 ::SetCursor(NULL) when io.MouseDrawCursor is set.

// Forward Declarations
static void ImGui_ImplWin32_InitPlatformInterface();
static void ImGui_ImplWin32_ShutdownPlatformInterface();
static void ImGui_ImplWin32_UpdateMonitors();

struct ImGui_ImplWin32_Data
{
    HWND                        hWnd;
    HWND                        MouseHwnd;
    bool                        MouseTracked;
    int                         MouseButtonsDown;
    INT64                       Time;
    INT64                       TicksPerSecond;
    ImGuiMouseCursor            LastMouseCursor;
    bool                        HasGamepad;
    bool                        WantUpdateHasGamepad;
    bool                        WantUpdateMonitors;

#ifndef IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
    HMODULE                     XInputDLL;
    PFN_XInputGetCapabilities   XInputGetCapabilities;
    PFN_XInputGetState          XInputGetState;
#endif

    ImGui_ImplWin32_Data() { memset((void*)this, 0, sizeof(*this)); }
};

// Backend data stored in io.BackendPlatformUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
// FIXME: multi-context support is not well tested and probably dysfunctional in this backend.
// FIXME: some shared resources (mouse cursor shape, gamepad) are mishandled when using multi-context.
static ImGui_ImplWin32_Data* ImGui_ImplWin32_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplWin32_Data*)ImGui::GetIO().BackendPlatformUserData : NULL;
}

// Functions
bool    ImGui_ImplWin32_Init(void* hwnd)
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendPlatformUserData == NULL && "Already initialized a platform backend!");

    INT64 perf_frequency, perf_counter;
    if (!::QueryPerformanceFrequency((LARGE_INTEGER*)&perf_frequency))
        return false;
    if (!::QueryPerformanceCounter((LARGE_INTEGER*)&perf_counter))
        return false;

    // Setup backend capabilities flags
    ImGui_ImplWin32_Data* bd = IM_NEW(ImGui_ImplWin32_Data)();
    io.BackendPlatformUserData = (void*)bd;
    io.BackendPlatformName = "imgui_impl_win32";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
    io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;    // We can create multi-viewports on the Platform side (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport; // We can call io.AddMouseViewportEvent() with correct data (optional)

    bd->hWnd = (HWND)hwnd;
    bd->WantUpdateHasGamepad = true;
    bd->WantUpdateMonitors = true;
    bd->TicksPerSecond = perf_frequency;
    bd->Time = perf_counter;
    bd->LastMouseCursor = ImGuiMouseCursor_COUNT;

    // Our mouse update function expect PlatformHandle to be filled for the main viewport
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    main_viewport->PlatformHandle = main_viewport->PlatformHandleRaw = (void*)bd->hWnd;
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        ImGui_ImplWin32_InitPlatformInterface();

    // Dynamically load XInput library
#ifndef IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
    const char* xinput_dll_names[] =
    {
        "xinput1_4.dll",   // Windows 8+
        "xinput1_3.dll",   // DirectX SDK
        "xinput9_1_0.dll", // Windows Vista, Windows 7
        "xinput1_2.dll",   // DirectX SDK
        "xinput1_1.dll"    // DirectX SDK
    };
    for (int n = 0; n < IM_ARRAYSIZE(xinput_dll_names); n++)
        if (HMODULE dll = ::LoadLibraryA(xinput_dll_names[n]))
        {
            bd->XInputDLL = dll;
            bd->XInputGetCapabilities = (PFN_XInputGetCapabilities)::GetProcAddress(dll, "XInputGetCapabilities");
            bd->XInputGetState = (PFN_XInputGetState)::GetProcAddress(dll, "XInputGetState");
            break;
        }
#endif // IMGUI_IMPL_WIN32_DISABLE_GAMEPAD

    return true;
}

void    ImGui_ImplWin32_Shutdown()
{
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData();
    IM_ASSERT(bd != NULL && "No platform backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplWin32_ShutdownPlatformInterface();

    // Unload XInput library
#ifndef IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
    if (bd->XInputDLL)
        ::FreeLibrary(bd->XInputDLL);
#endif // IMGUI_IMPL_WIN32_DISABLE_GAMEPAD

    io.BackendPlatformName = NULL;
    io.BackendPlatformUserData = NULL;
    IM_DELETE(bd);
}

static bool ImGui_ImplWin32_UpdateMouseCursor()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return false;

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
    {
        // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
        ::SetCursor(NULL);
    }
    else
    {
        // Show OS mouse cursor
        LPTSTR win32_cursor = IDC_ARROW;
        switch (imgui_cursor)
        {
        case ImGuiMouseCursor_Arrow:        win32_cursor = IDC_ARROW; break;
        case ImGuiMouseCursor_TextInput:    win32_cursor = IDC_IBEAM; break;
        case ImGuiMouseCursor_ResizeAll:    win32_cursor = IDC_SIZEALL; break;
        case ImGuiMouseCursor_ResizeEW:     win32_cursor = IDC_SIZEWE; break;
        case ImGuiMouseCursor_ResizeNS:     win32_cursor = IDC_SIZENS; break;
        case ImGuiMouseCursor_ResizeNESW:   win32_cursor = IDC_SIZENESW; break;
        case ImGuiMouseCursor_ResizeNWSE:   win32_cursor = IDC_SIZENWSE; break;
        case ImGuiMouseCursor_Hand:         win32_cursor = IDC_HAND; break;
        case ImGuiMouseCursor_NotAllowed:   win32_cursor = IDC_NO; break;
        }
        ::SetCursor(::LoadCursor(NULL, win32_cursor));
    }
    return true;
}

static bool IsVkDown(int vk)
{
    return (::GetKeyState(vk) & 0x8000) != 0;
}

static void ImGui_ImplWin32_AddKeyEvent(ImGuiKey key, bool down, int native_keycode, int native_scancode = -1)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(key, down);
    io.SetKeyEventNativeData(key, native_keycode, native_scancode); // To support legacy indexing (<1.87 user code)
    IM_UNUSED(native_scancode);
}

static void ImGui_ImplWin32_ProcessKeyEventsWorkarounds()
{
    // Left & right Shift keys: when both are pressed together, Windows tend to not generate the WM_KEYUP event for the first released one.
    if (ImGui::IsKeyDown(ImGuiKey_LeftShift) && !IsVkDown(VK_LSHIFT))
        ImGui_ImplWin32_AddKeyEvent(ImGuiKey_LeftShift, false, VK_LSHIFT);
    if (ImGui::IsKeyDown(ImGuiKey_RightShift) && !IsVkDown(VK_RSHIFT))
        ImGui_ImplWin32_AddKeyEvent(ImGuiKey_RightShift, false, VK_RSHIFT);

    // Sometimes WM_KEYUP for Win key is not passed down to the app (e.g. for Win+V on some setups, according to GLFW).
    if (ImGui::IsKeyDown(ImGuiKey_LeftSuper) && !IsVkDown(VK_LWIN))
        ImGui_ImplWin32_AddKeyEvent(ImGuiKey_LeftSuper, false, VK_LWIN);
    if (ImGui::IsKeyDown(ImGuiKey_RightSuper) && !IsVkDown(VK_RWIN))
        ImGui_ImplWin32_AddKeyEvent(ImGuiKey_RightSuper, false, VK_RWIN);
}

static void ImGui_ImplWin32_UpdateKeyModifiers()
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiKey_ModCtrl, IsVkDown(VK_CONTROL));
    io.AddKeyEvent(ImGuiKey_ModShift, IsVkDown(VK_SHIFT));
    io.AddKeyEvent(ImGuiKey_ModAlt, IsVkDown(VK_MENU));
    io.AddKeyEvent(ImGuiKey_ModSuper, IsVkDown(VK_APPS));
}

// This code supports multi-viewports (multiple OS Windows mapped into different Dear ImGui viewports)
// Because of that, it is a little more complicated than your typical single-viewport binding code!
static void ImGui_ImplWin32_UpdateMouseData()
{
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(bd->hWnd != 0);

    POINT mouse_screen_pos;
    bool has_mouse_screen_pos = ::GetCursorPos(&mouse_screen_pos) != 0;

    HWND focused_window = ::GetForegroundWindow();
    const bool is_app_focused = (focused_window && (focused_window == bd->hWnd || ::IsChild(focused_window, bd->hWnd) || ImGui::FindViewportByPlatformHandle((void*)focused_window)));
    if (is_app_focused)
    {
        // (Optional) Set OS mouse position from Dear ImGui if requested (rarely used, only when ImGuiConfigFlags_NavEnableSetMousePos is enabled by user)
        // When multi-viewports are enabled, all Dear ImGui positions are same as OS positions.
        if (io.WantSetMousePos)
        {
            POINT pos = { (int)io.MousePos.x, (int)io.MousePos.y };
            if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
                ::ClientToScreen(focused_window, &pos);
            ::SetCursorPos(pos.x, pos.y);
        }

        // (Optional) Fallback to provide mouse position when focused (WM_MOUSEMOVE already provides this when hovered or captured)
        if (!io.WantSetMousePos && !bd->MouseTracked && has_mouse_screen_pos)
        {
            // Single viewport mode: mouse position in client window coordinates (io.MousePos is (0,0) when the mouse is on the upper-left corner of the app window)
            // (This is the position you can get with ::GetCursorPos() + ::ScreenToClient() or WM_MOUSEMOVE.)
            // Multi-viewport mode: mouse position in OS absolute coordinates (io.MousePos is (0,0) when the mouse is on the upper-left of the primary monitor)
            // (This is the position you can get with ::GetCursorPos() or WM_MOUSEMOVE + ::ClientToScreen(). In theory adding viewport->Pos to a client position would also be the same.)
            POINT mouse_pos = mouse_screen_pos;
            if (!(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
                ::ScreenToClient(bd->hWnd, &mouse_pos);
            io.AddMousePosEvent((float)mouse_pos.x, (float)mouse_pos.y);
        }
    }

    // (Optional) When using multiple viewports: call io.AddMouseViewportEvent() with the viewport the OS mouse cursor is hovering.
    // If ImGuiBackendFlags_HasMouseHoveredViewport is not set by the backend, Dear imGui will ignore this field and infer the information using its flawed heuristic.
    // - [X] Win32 backend correctly ignore viewports with the _NoInputs flag (here using ::WindowFromPoint with WM_NCHITTEST + HTTRANSPARENT in WndProc does that)
    //       Some backend are not able to handle that correctly. If a backend report an hovered viewport that has the _NoInputs flag (e.g. when dragging a window
    //       for docking, the viewport has the _NoInputs flag in order to allow us to find the viewport under), then Dear ImGui is forced to ignore the value reported
    //       by the backend, and use its flawed heuristic to guess the viewport behind.
    // - [X] Win32 backend correctly reports this regardless of another viewport behind focused and dragged from (we need this to find a useful drag and drop target).
    ImGuiID mouse_viewport_id = 0;
    if (has_mouse_screen_pos)
        if (HWND hovered_hwnd = ::WindowFromPoint(mouse_screen_pos))
            if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle((void*)hovered_hwnd))
                mouse_viewport_id = viewport->ID;
    io.AddMouseViewportEvent(mouse_viewport_id);
}

// Gamepad navigation mapping
static void ImGui_ImplWin32_UpdateGamepads()
{
#ifndef IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData();
    if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
        return;

    // Calling XInputGetState() every frame on disconnected gamepads is unfortunately too slow.
    // Instead we refresh gamepad availability by calling XInputGetCapabilities() _only_ after receiving WM_DEVICECHANGE.
    if (bd->WantUpdateHasGamepad)
    {
        XINPUT_CAPABILITIES caps = {};
        bd->HasGamepad = bd->XInputGetCapabilities ? (bd->XInputGetCapabilities(0, XINPUT_FLAG_GAMEPAD, &caps) == ERROR_SUCCESS) : false;
        bd->WantUpdateHasGamepad = false;
    }

    io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    XINPUT_STATE xinput_state;
    XINPUT_GAMEPAD& gamepad = xinput_state.Gamepad;
    if (!bd->HasGamepad || bd->XInputGetState == NULL || bd->XInputGetState(0, &xinput_state) != ERROR_SUCCESS)
        return;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

#define IM_SATURATE(V)                      (V < 0.0f ? 0.0f : V > 1.0f ? 1.0f : V)
#define MAP_BUTTON(KEY_NO, BUTTON_ENUM)     { io.AddKeyEvent(KEY_NO, (gamepad.wButtons & BUTTON_ENUM) != 0); }
#define MAP_ANALOG(KEY_NO, VALUE, V0, V1)   { float vn = (float)(VALUE - V0) / (float)(V1 - V0); io.AddKeyAnalogEvent(KEY_NO, vn > 0.10f, IM_SATURATE(vn)); }
    MAP_BUTTON(ImGuiKey_GamepadStart, XINPUT_GAMEPAD_START);
    MAP_BUTTON(ImGuiKey_GamepadBack, XINPUT_GAMEPAD_BACK);
    MAP_BUTTON(ImGuiKey_GamepadFaceDown, XINPUT_GAMEPAD_A);
    MAP_BUTTON(ImGuiKey_GamepadFaceRight, XINPUT_GAMEPAD_B);
    MAP_BUTTON(ImGuiKey_GamepadFaceLeft, XINPUT_GAMEPAD_X);
    MAP_BUTTON(ImGuiKey_GamepadFaceUp, XINPUT_GAMEPAD_Y);
    MAP_BUTTON(ImGuiKey_GamepadDpadLeft, XINPUT_GAMEPAD_DPAD_LEFT);
    MAP_BUTTON(ImGuiKey_GamepadDpadRight, XINPUT_GAMEPAD_DPAD_RIGHT);
    MAP_BUTTON(ImGuiKey_GamepadDpadUp, XINPUT_GAMEPAD_DPAD_UP);
    MAP_BUTTON(ImGuiKey_GamepadDpadDown, XINPUT_GAMEPAD_DPAD_DOWN);
    MAP_BUTTON(ImGuiKey_GamepadL1, XINPUT_GAMEPAD_LEFT_SHOULDER);
    MAP_BUTTON(ImGuiKey_GamepadR1, XINPUT_GAMEPAD_RIGHT_SHOULDER);
    MAP_ANALOG(ImGuiKey_GamepadL2, gamepad.bLeftTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD, 255);
    MAP_ANALOG(ImGuiKey_GamepadR2, gamepad.bRightTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD, 255);
    MAP_BUTTON(ImGuiKey_GamepadL3, XINPUT_GAMEPAD_LEFT_THUMB);
    MAP_BUTTON(ImGuiKey_GamepadR3, XINPUT_GAMEPAD_RIGHT_THUMB);
    MAP_ANALOG(ImGuiKey_GamepadLStickLeft, gamepad.sThumbLX, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
    MAP_ANALOG(ImGuiKey_GamepadLStickRight, gamepad.sThumbLX, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
    MAP_ANALOG(ImGuiKey_GamepadLStickUp, gamepad.sThumbLY, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
    MAP_ANALOG(ImGuiKey_GamepadLStickDown, gamepad.sThumbLY, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
    MAP_ANALOG(ImGuiKey_GamepadRStickLeft, gamepad.sThumbRX, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
    MAP_ANALOG(ImGuiKey_GamepadRStickRight, gamepad.sThumbRX, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
    MAP_ANALOG(ImGuiKey_GamepadRStickUp, gamepad.sThumbRY, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
    MAP_ANALOG(ImGuiKey_GamepadRStickDown, gamepad.sThumbRY, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
#undef MAP_BUTTON
#undef MAP_ANALOG
#endif // #ifndef IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
}

static BOOL CALLBACK ImGui_ImplWin32_UpdateMonitors_EnumFunc(HMONITOR monitor, HDC, LPRECT, LPARAM)
{
    MONITORINFO info = {};
    info.cbSize = sizeof(MONITORINFO);
    if (!::GetMonitorInfo(monitor, &info))
        return TRUE;
    ImGuiPlatformMonitor imgui_monitor;
    imgui_monitor.MainPos = ImVec2((float)info.rcMonitor.left, (float)info.rcMonitor.top);
    imgui_monitor.MainSize = ImVec2((float)(info.rcMonitor.right - info.rcMonitor.left), (float)(info.rcMonitor.bottom - info.rcMonitor.top));
    imgui_monitor.WorkPos = ImVec2((float)info.rcWork.left, (float)info.rcWork.top);
    imgui_monitor.WorkSize = ImVec2((float)(info.rcWork.right - info.rcWork.left), (float)(info.rcWork.bottom - info.rcWork.top));
    imgui_monitor.DpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(monitor);
    ImGuiPlatformIO& io = ImGui::GetPlatformIO();
    if (info.dwFlags & MONITORINFOF_PRIMARY)
        io.Monitors.push_front(imgui_monitor);
    else
        io.Monitors.push_back(imgui_monitor);
    return TRUE;
}

static void ImGui_ImplWin32_UpdateMonitors()
{
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData();
    ImGui::GetPlatformIO().Monitors.resize(0);
    ::EnumDisplayMonitors(NULL, NULL, ImGui_ImplWin32_UpdateMonitors_EnumFunc, 0);
    bd->WantUpdateMonitors = false;
}

void    ImGui_ImplWin32_NewFrame()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData();
    IM_ASSERT(bd != NULL && "Did you call ImGui_ImplWin32_Init()?");

    // Setup display size (every frame to accommodate for window resizing)
    RECT rect = { 0, 0, 0, 0 };
    ::GetClientRect(bd->hWnd, &rect);
    io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
    if (bd->WantUpdateMonitors)
        ImGui_ImplWin32_UpdateMonitors();

    // Setup time step
    INT64 current_time = 0;
    ::QueryPerformanceCounter((LARGE_INTEGER*)&current_time);
    io.DeltaTime = (float)(current_time - bd->Time) / bd->TicksPerSecond;
    bd->Time = current_time;

    // Update OS mouse position
    ImGui_ImplWin32_UpdateMouseData();

    // Process workarounds for known Windows key handling issues
    ImGui_ImplWin32_ProcessKeyEventsWorkarounds();

    // Update OS mouse cursor with the cursor requested by imgui
    ImGuiMouseCursor mouse_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
    if (bd->LastMouseCursor != mouse_cursor)
    {
        bd->LastMouseCursor = mouse_cursor;
        ImGui_ImplWin32_UpdateMouseCursor();
    }

    // Update game controllers (if enabled and available)
    ImGui_ImplWin32_UpdateGamepads();
}

// There is no distinct VK_xxx for keypad enter, instead it is VK_RETURN + KF_EXTENDED, we assign it an arbitrary value to make code more readable (VK_ codes go up to 255)
#define IM_VK_KEYPAD_ENTER      (VK_RETURN + 256)

// Map VK_xxx to ImGuiKey_xxx.
static ImGuiKey ImGui_ImplWin32_VirtualKeyToImGuiKey(WPARAM wParam)
{
    switch (wParam)
    {
    case VK_TAB: return ImGuiKey_Tab;
    case VK_LEFT: return ImGuiKey_LeftArrow;
    case VK_RIGHT: return ImGuiKey_RightArrow;
    case VK_UP: return ImGuiKey_UpArrow;
    case VK_DOWN: return ImGuiKey_DownArrow;
    case VK_PRIOR: return ImGuiKey_PageUp;
    case VK_NEXT: return ImGuiKey_PageDown;
    case VK_HOME: return ImGuiKey_Home;
    case VK_END: return ImGuiKey_End;
    case VK_INSERT: return ImGuiKey_Insert;
    case VK_DELETE: return ImGuiKey_Delete;
    case VK_BACK: return ImGuiKey_Backspace;
    case VK_SPACE: return ImGuiKey_Space;
    case VK_RETURN: return ImGuiKey_Enter;
    case VK_ESCAPE: return ImGuiKey_Escape;
    case VK_OEM_7: return ImGuiKey_Apostrophe;
    case VK_OEM_COMMA: return ImGuiKey_Comma;
    case VK_OEM_MINUS: return ImGuiKey_Minus;
    case VK_OEM_PERIOD: return ImGuiKey_Period;
    case VK_OEM_2: return ImGuiKey_Slash;
    case VK_OEM_1: return ImGuiKey_Semicolon;
    case VK_OEM_PLUS: return ImGuiKey_Equal;
    case VK_OEM_4: return ImGuiKey_LeftBracket;
    case VK_OEM_5: return ImGuiKey_Backslash;
    case VK_OEM_6: return ImGuiKey_RightBracket;
    case VK_OEM_3: return ImGuiKey_GraveAccent;
    case VK_CAPITAL: return ImGuiKey_CapsLock;
    case VK_SCROLL: return ImGuiKey_ScrollLock;
    case VK_NUMLOCK: return ImGuiKey_NumLock;
    case VK_SNAPSHOT: return ImGuiKey_PrintScreen;
    case VK_PAUSE: return ImGuiKey_Pause;
    case VK_NUMPAD0: return ImGuiKey_Keypad0;
    case VK_NUMPAD1: return ImGuiKey_Keypad1;
    case VK_NUMPAD2: return ImGuiKey_Keypad2;
    case VK_NUMPAD3: return ImGuiKey_Keypad3;
    case VK_NUMPAD4: return ImGuiKey_Keypad4;
    case VK_NUMPAD5: return ImGuiKey_Keypad5;
    case VK_NUMPAD6: return ImGuiKey_Keypad6;
    case VK_NUMPAD7: return ImGuiKey_Keypad7;
    case VK_NUMPAD8: return ImGuiKey_Keypad8;
    case VK_NUMPAD9: return ImGuiKey_Keypad9;
    case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
    case VK_DIVIDE: return ImGuiKey_KeypadDivide;
    case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
    case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
    case VK_ADD: return ImGuiKey_KeypadAdd;
    case IM_VK_KEYPAD_ENTER: return ImGuiKey_KeypadEnter;
    case VK_LSHIFT: return ImGuiKey_LeftShift;
    case VK_LCONTROL: return ImGuiKey_LeftCtrl;
    case VK_LMENU: return ImGuiKey_LeftAlt;
    case VK_LWIN: return ImGuiKey_LeftSuper;
    case VK_RSHIFT: return ImGuiKey_RightShift;
    case VK_RCONTROL: return ImGuiKey_RightCtrl;
    case VK_RMENU: return ImGuiKey_RightAlt;
    case VK_RWIN: return ImGuiKey_RightSuper;
    case VK_APPS: return ImGuiKey_Menu;
    case '0': return ImGuiKey_0;
    case '1': return ImGuiKey_1;
    case '2': return ImGuiKey_2;
    case '3': return ImGuiKey_3;
    case '4': return ImGuiKey_4;
    case '5': return ImGuiKey_5;
    case '6': return ImGuiKey_6;
    case '7': return ImGuiKey_7;
    case '8': return ImGuiKey_8;
    case '9': return ImGuiKey_9;
    case 'A': return ImGuiKey_A;
    case 'B': return ImGuiKey_B;
    case 'C': return ImGuiKey_C;
    case 'D': return ImGuiKey_D;
    case 'E': return ImGuiKey_E;
    case 'F': return ImGuiKey_F;
    case 'G': return ImGuiKey_G;
    case 'H': return ImGuiKey_H;
    case 'I': return ImGuiKey_I;
    case 'J': return ImGuiKey_J;
    case 'K': return ImGuiKey_K;
    case 'L': return ImGuiKey_L;
    case 'M': return ImGuiKey_M;
    case 'N': return ImGuiKey_N;
    case 'O': return ImGuiKey_O;
    case 'P': return ImGuiKey_P;
    case 'Q': return ImGuiKey_Q;
    case 'R': return ImGuiKey_R;
    case 'S': return ImGuiKey_S;
    case 'T': return ImGuiKey_T;
    case 'U': return ImGuiKey_U;
    case 'V': return ImGuiKey_V;
    case 'W': return ImGuiKey_W;
    case 'X': return ImGuiKey_X;
    case 'Y': return ImGuiKey_Y;
    case 'Z': return ImGuiKey_Z;
    case VK_F1: return ImGuiKey_F1;
    case VK_F2: return ImGuiKey_F2;
    case VK_F3: return ImGuiKey_F3;
    case VK_F4: return ImGuiKey_F4;
    case VK_F5: return ImGuiKey_F5;
    case VK_F6: return ImGuiKey_F6;
    case VK_F7: return ImGuiKey_F7;
    case VK_F8: return ImGuiKey_F8;
    case VK_F9: return ImGuiKey_F9;
    case VK_F10: return ImGuiKey_F10;
    case VK_F11: return ImGuiKey_F11;
    case VK_F12: return ImGuiKey_F12;
    default: return ImGuiKey_None;
    }
}

// Allow compilation with old Windows SDK. MinGW doesn't have default _WIN32_WINNT/WINVER versions.
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif
#ifndef DBT_DEVNODES_CHANGED
#define DBT_DEVNODES_CHANGED 0x0007
#endif

// Win32 message handler (process Win32 mouse/keyboard inputs, etc.)
// Call from your application's message handler. Keep calling your message handler unless this function returns TRUE.
// When implementing your own backend, you can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if Dear ImGui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to Dear ImGui, and hide them from your application based on those two flags.
// PS: In this Win32 handler, we use the capture API (GetCapture/SetCapture/ReleaseCapture) to be able to read mouse coordinates when dragging mouse outside of our window bounds.
// PS: We treat DBLCLK messages as regular mouse down messages, so this code will work on windows classes that have the CS_DBLCLKS flag set. Our own example app code doesn't set this flag.
#if 0
// Copy this line into your .cpp file to forward declare the function.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui::GetCurrentContext() == NULL)
        return 0;

    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData();

    switch (msg)
    {
    case WM_MOUSEMOVE:
    {
        // We need to call TrackMouseEvent in order to receive WM_MOUSELEAVE events
        bd->MouseHwnd = hwnd;
        if (!bd->MouseTracked)
        {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            ::TrackMouseEvent(&tme);
            bd->MouseTracked = true;
        }
        POINT mouse_pos = { (LONG)GET_X_LPARAM(lParam), (LONG)GET_Y_LPARAM(lParam) };
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            ::ClientToScreen(hwnd, &mouse_pos);
        io.AddMousePosEvent((float)mouse_pos.x, (float)mouse_pos.y);
        break;
    }
    case WM_MOUSELEAVE:
        if (bd->MouseHwnd == hwnd)
            bd->MouseHwnd = NULL;
        bd->MouseTracked = false;
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        break;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
    {
        int button = 0;
        if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) { button = 0; }
        if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) { button = 1; }
        if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) { button = 2; }
        if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
        if (bd->MouseButtonsDown == 0 && ::GetCapture() == NULL)
            ::SetCapture(hwnd);
        bd->MouseButtonsDown |= 1 << button;
        io.AddMouseButtonEvent(button, true);
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
    {
        int button = 0;
        if (msg == WM_LBUTTONUP) { button = 0; }
        if (msg == WM_RBUTTONUP) { button = 1; }
        if (msg == WM_MBUTTONUP) { button = 2; }
        if (msg == WM_XBUTTONUP) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
        bd->MouseButtonsDown &= ~(1 << button);
        if (bd->MouseButtonsDown == 0 && ::GetCapture() == hwnd)
            ::ReleaseCapture();
        io.AddMouseButtonEvent(button, false);
        return 0;
    }
    case WM_MOUSEWHEEL:
        io.AddMouseWheelEvent(0.0f, (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA);
        return 0;
    case WM_MOUSEHWHEEL:
        io.AddMouseWheelEvent((float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA, 0.0f);
        return 0;
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
        const bool is_key_down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
        if (wParam < 256)
        {
            // Submit modifiers
            ImGui_ImplWin32_UpdateKeyModifiers();

            // Obtain virtual key code
            // (keypad enter doesn't have its own... VK_RETURN with KF_EXTENDED flag means keypad enter, see IM_VK_KEYPAD_ENTER definition for details, it is mapped to ImGuiKey_KeyPadEnter.)
            int vk = (int)wParam;
            if ((wParam == VK_RETURN) && (HIWORD(lParam) & KF_EXTENDED))
                vk = IM_VK_KEYPAD_ENTER;

            // Submit key event
            const ImGuiKey key = ImGui_ImplWin32_VirtualKeyToImGuiKey(vk);
            const int scancode = (int)LOBYTE(HIWORD(lParam));
            if (key != ImGuiKey_None)
                ImGui_ImplWin32_AddKeyEvent(key, is_key_down, vk, scancode);

            // Submit individual left/right modifier events
            if (vk == VK_SHIFT)
            {
                // Important: Shift keys tend to get stuck when pressed together, missing key-up events are corrected in ImGui_ImplWin32_ProcessKeyEventsWorkarounds()
                if (IsVkDown(VK_LSHIFT) == is_key_down) { ImGui_ImplWin32_AddKeyEvent(ImGuiKey_LeftShift, is_key_down, VK_LSHIFT, scancode); }
                if (IsVkDown(VK_RSHIFT) == is_key_down) { ImGui_ImplWin32_AddKeyEvent(ImGuiKey_RightShift, is_key_down, VK_RSHIFT, scancode); }
            }
            else if (vk == VK_CONTROL)
            {
                if (IsVkDown(VK_LCONTROL) == is_key_down) { ImGui_ImplWin32_AddKeyEvent(ImGuiKey_LeftCtrl, is_key_down, VK_LCONTROL, scancode); }
                if (IsVkDown(VK_RCONTROL) == is_key_down) { ImGui_ImplWin32_AddKeyEvent(ImGuiKey_RightCtrl, is_key_down, VK_RCONTROL, scancode); }
            }
            else if (vk == VK_MENU)
            {
                if (IsVkDown(VK_LMENU) == is_key_down) { ImGui_ImplWin32_AddKeyEvent(ImGuiKey_LeftAlt, is_key_down, VK_LMENU, scancode); }
                if (IsVkDown(VK_RMENU) == is_key_down) { ImGui_ImplWin32_AddKeyEvent(ImGuiKey_RightAlt, is_key_down, VK_RMENU, scancode); }
            }
        }
        return 0;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        io.AddFocusEvent(msg == WM_SETFOCUS);
        return 0;
    case WM_CHAR:
        // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
        if (wParam > 0 && wParam < 0x10000)
            io.AddInputCharacterUTF16((unsigned short)wParam);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && ImGui_ImplWin32_UpdateMouseCursor())
            return 1;
        return 0;
    case WM_DEVICECHANGE:
        if ((UINT)wParam == DBT_DEVNODES_CHANGED)
            bd->WantUpdateHasGamepad = true;
        return 0;
    case WM_DISPLAYCHANGE:
        bd->WantUpdateMonitors = true;
        return 0;
    }
    return 0;
}


//--------------------------------------------------------------------------------------------------------
// DPI-related helpers (optional)
//--------------------------------------------------------------------------------------------------------
// - Use to enable DPI awareness without having to create an application manifest.
// - Your own app may already do this via a manifest or explicit calls. This is mostly useful for our examples/ apps.
// - In theory we could call simple functions from Windows SDK such as SetProcessDPIAware(), SetProcessDpiAwareness(), etc.
//   but most of the functions provided by Microsoft require Windows 8.1/10+ SDK at compile time and Windows 8/10+ at runtime,
//   neither we want to require the user to have. So we dynamically select and load those functions to avoid dependencies.
//---------------------------------------------------------------------------------------------------------
// This is the scheme successfully used by GLFW (from which we borrowed some of the code) and other apps aiming to be highly portable.
// ImGui_ImplWin32_EnableDpiAwareness() is just a helper called by main.cpp, we don't call it automatically.
// If you are trying to implement your own backend for your own engine, you may ignore that noise.
//---------------------------------------------------------------------------------------------------------

// Perform our own check with RtlVerifyVersionInfo() instead of using functions from <VersionHelpers.h> as they
// require a manifest to be functional for checks above 8.1. See https://github.com/ocornut/imgui/issues/4200
static BOOL _IsWindowsVersionOrGreater(WORD major, WORD minor, WORD)
{
    typedef LONG(WINAPI* PFN_RtlVerifyVersionInfo)(OSVERSIONINFOEXW*, ULONG, ULONGLONG);
    static PFN_RtlVerifyVersionInfo RtlVerifyVersionInfoFn = NULL;
    if (RtlVerifyVersionInfoFn == NULL)
        if (HMODULE ntdllModule = ::GetModuleHandleA("ntdll.dll"))
            RtlVerifyVersionInfoFn = (PFN_RtlVerifyVersionInfo)GetProcAddress(ntdllModule, "RtlVerifyVersionInfo");
    if (RtlVerifyVersionInfoFn == NULL)
        return FALSE;

    RTL_OSVERSIONINFOEXW versionInfo = { };
    ULONGLONG conditionMask = 0;
    versionInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);
    versionInfo.dwMajorVersion = major;
    versionInfo.dwMinorVersion = minor;
    VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
    return (RtlVerifyVersionInfoFn(&versionInfo, VER_MAJORVERSION | VER_MINORVERSION, conditionMask) == 0) ? TRUE : FALSE;
}

#define _IsWindowsVistaOrGreater()   _IsWindowsVersionOrGreater(HIBYTE(0x0600), LOBYTE(0x0600), 0) // _WIN32_WINNT_VISTA
#define _IsWindows8OrGreater()       _IsWindowsVersionOrGreater(HIBYTE(0x0602), LOBYTE(0x0602), 0) // _WIN32_WINNT_WIN8
#define _IsWindows8Point1OrGreater() _IsWindowsVersionOrGreater(HIBYTE(0x0603), LOBYTE(0x0603), 0) // _WIN32_WINNT_WINBLUE
#define _IsWindows10OrGreater()      _IsWindowsVersionOrGreater(HIBYTE(0x0A00), LOBYTE(0x0A00), 0) // _WIN32_WINNT_WINTHRESHOLD / _WIN32_WINNT_WIN10

#ifndef DPI_ENUMS_DECLARED
typedef enum { PROCESS_DPI_UNAWARE = 0, PROCESS_SYSTEM_DPI_AWARE = 1, PROCESS_PER_MONITOR_DPI_AWARE = 2 } PROCESS_DPI_AWARENESS;
typedef enum { MDT_EFFECTIVE_DPI = 0, MDT_ANGULAR_DPI = 1, MDT_RAW_DPI = 2, MDT_DEFAULT = MDT_EFFECTIVE_DPI } MONITOR_DPI_TYPE;
#endif
#ifndef _DPI_AWARENESS_CONTEXTS_
DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE    (DPI_AWARENESS_CONTEXT)-3
#endif
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 (DPI_AWARENESS_CONTEXT)-4
#endif
typedef HRESULT(WINAPI* PFN_SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS);                     // Shcore.lib + dll, Windows 8.1+
typedef HRESULT(WINAPI* PFN_GetDpiForMonitor)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);        // Shcore.lib + dll, Windows 8.1+
typedef DPI_AWARENESS_CONTEXT(WINAPI* PFN_SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT); // User32.lib + dll, Windows 10 v1607+ (Creators Update)

// Helper function to enable DPI awareness without setting up a manifest
void ImGui_ImplWin32_EnableDpiAwareness()
{
    // Make sure monitors will be updated with latest correct scaling
    if (ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData())
        bd->WantUpdateMonitors = true;

    if (_IsWindows10OrGreater())
    {
        static HINSTANCE user32_dll = ::LoadLibraryA("user32.dll"); // Reference counted per-process
        if (PFN_SetThreadDpiAwarenessContext SetThreadDpiAwarenessContextFn = (PFN_SetThreadDpiAwarenessContext)::GetProcAddress(user32_dll, "SetThreadDpiAwarenessContext"))
        {
            SetThreadDpiAwarenessContextFn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            return;
        }
    }
    if (_IsWindows8Point1OrGreater())
    {
        static HINSTANCE shcore_dll = ::LoadLibraryA("shcore.dll"); // Reference counted per-process
        if (PFN_SetProcessDpiAwareness SetProcessDpiAwarenessFn = (PFN_SetProcessDpiAwareness)::GetProcAddress(shcore_dll, "SetProcessDpiAwareness"))
        {
            SetProcessDpiAwarenessFn(PROCESS_PER_MONITOR_DPI_AWARE);
            return;
        }
    }
#if _WIN32_WINNT >= 0x0600
    ::SetProcessDPIAware();
#endif
}

#if defined(_MSC_VER) && !defined(NOGDI)
#pragma comment(lib, "gdi32")   // Link with gdi32.lib for GetDeviceCaps(). MinGW will require linking with '-lgdi32'
#endif

float ImGui_ImplWin32_GetDpiScaleForMonitor(void* monitor)
{
    UINT xdpi = 96, ydpi = 96;
    if (_IsWindows8Point1OrGreater())
    {
        static HINSTANCE shcore_dll = ::LoadLibraryA("shcore.dll"); // Reference counted per-process
        static PFN_GetDpiForMonitor GetDpiForMonitorFn = NULL;
        if (GetDpiForMonitorFn == NULL && shcore_dll != NULL)
            GetDpiForMonitorFn = (PFN_GetDpiForMonitor)::GetProcAddress(shcore_dll, "GetDpiForMonitor");
        if (GetDpiForMonitorFn != NULL)
        {
            GetDpiForMonitorFn((HMONITOR)monitor, MDT_EFFECTIVE_DPI, &xdpi, &ydpi);
            IM_ASSERT(xdpi == ydpi); // Please contact me if you hit this assert!
            return xdpi / 96.0f;
        }
    }
#ifndef NOGDI
    const HDC dc = ::GetDC(NULL);
    xdpi = ::GetDeviceCaps(dc, LOGPIXELSX);
    ydpi = ::GetDeviceCaps(dc, LOGPIXELSY);
    IM_ASSERT(xdpi == ydpi); // Please contact me if you hit this assert!
    ::ReleaseDC(NULL, dc);
#endif
    return xdpi / 96.0f;
}

float ImGui_ImplWin32_GetDpiScaleForHwnd(void* hwnd)
{
    HMONITOR monitor = ::MonitorFromWindow((HWND)hwnd, MONITOR_DEFAULTTONEAREST);
    return ImGui_ImplWin32_GetDpiScaleForMonitor(monitor);
}

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the backend to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
//--------------------------------------------------------------------------------------------------------

// Helper structure we store in the void* RenderUserData field of each ImGuiViewport to easily retrieve our backend data.
struct ImGui_ImplWin32_ViewportData
{
    HWND    Hwnd;
    bool    HwndOwned;
    DWORD   DwStyle;
    DWORD   DwExStyle;

    ImGui_ImplWin32_ViewportData() { Hwnd = NULL; HwndOwned = false;  DwStyle = DwExStyle = 0; }
    ~ImGui_ImplWin32_ViewportData() { IM_ASSERT(Hwnd == NULL); }
};

static void ImGui_ImplWin32_GetWin32StyleFromViewportFlags(ImGuiViewportFlags flags, DWORD* out_style, DWORD* out_ex_style)
{
    if (flags & ImGuiViewportFlags_NoDecoration)
        *out_style = WS_POPUP;
    else
        *out_style = WS_OVERLAPPEDWINDOW;

    if (flags & ImGuiViewportFlags_NoTaskBarIcon)
        *out_ex_style = WS_EX_TOOLWINDOW;
    else
        *out_ex_style = WS_EX_APPWINDOW;

    if (flags & ImGuiViewportFlags_TopMost)
        *out_ex_style |= WS_EX_TOPMOST;
}

static void ImGui_ImplWin32_CreateWindow(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = IM_NEW(ImGui_ImplWin32_ViewportData)();
    viewport->PlatformUserData = vd;

    // Select style and parent window
    ImGui_ImplWin32_GetWin32StyleFromViewportFlags(viewport->Flags, &vd->DwStyle, &vd->DwExStyle);
    HWND parent_window = NULL;
    if (viewport->ParentViewportId != 0)
        if (ImGuiViewport* parent_viewport = ImGui::FindViewportByID(viewport->ParentViewportId))
            parent_window = (HWND)parent_viewport->PlatformHandle;

    // Create window
    RECT rect = { (LONG)viewport->Pos.x, (LONG)viewport->Pos.y, (LONG)(viewport->Pos.x + viewport->Size.x), (LONG)(viewport->Pos.y + viewport->Size.y) };
    ::AdjustWindowRectEx(&rect, vd->DwStyle, FALSE, vd->DwExStyle);
    vd->Hwnd = ::CreateWindowEx(
        vd->DwExStyle, _T("ImGui Platform"), _T("Untitled"), vd->DwStyle,   // Style, class name, window name
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,    // Window area
        parent_window, NULL, ::GetModuleHandle(NULL), NULL);                    // Parent window, Menu, Instance, Param
    vd->HwndOwned = true;
    viewport->PlatformRequestResize = false;
    viewport->PlatformHandle = viewport->PlatformHandleRaw = vd->Hwnd;
}

static void ImGui_ImplWin32_DestroyWindow(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData();
    if (ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData)
    {
        if (::GetCapture() == vd->Hwnd)
        {
            // Transfer capture so if we started dragging from a window that later disappears, we'll still receive the MOUSEUP event.
            ::ReleaseCapture();
            ::SetCapture(bd->hWnd);
        }
        if (vd->Hwnd && vd->HwndOwned)
            ::DestroyWindow(vd->Hwnd);
        vd->Hwnd = NULL;
        IM_DELETE(vd);
    }
    viewport->PlatformUserData = viewport->PlatformHandle = NULL;
}

static void ImGui_ImplWin32_ShowWindow(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    if (viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing)
        ::ShowWindow(vd->Hwnd, SW_SHOWNA);
    else
        ::ShowWindow(vd->Hwnd, SW_SHOW);
}

static void ImGui_ImplWin32_UpdateWindow(ImGuiViewport* viewport)
{
    // (Optional) Update Win32 style if it changed _after_ creation.
    // Generally they won't change unless configuration flags are changed, but advanced uses (such as manually rewriting viewport flags) make this useful.
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    DWORD new_style;
    DWORD new_ex_style;
    ImGui_ImplWin32_GetWin32StyleFromViewportFlags(viewport->Flags, &new_style, &new_ex_style);

    // Only reapply the flags that have been changed from our point of view (as other flags are being modified by Windows)
    if (vd->DwStyle != new_style || vd->DwExStyle != new_ex_style)
    {
        // (Optional) Update TopMost state if it changed _after_ creation
        bool top_most_changed = (vd->DwExStyle & WS_EX_TOPMOST) != (new_ex_style & WS_EX_TOPMOST);
        HWND insert_after = top_most_changed ? ((viewport->Flags & ImGuiViewportFlags_TopMost) ? HWND_TOPMOST : HWND_NOTOPMOST) : 0;
        UINT swp_flag = top_most_changed ? 0 : SWP_NOZORDER;

        // Apply flags and position (since it is affected by flags)
        vd->DwStyle = new_style;
        vd->DwExStyle = new_ex_style;
        ::SetWindowLong(vd->Hwnd, GWL_STYLE, vd->DwStyle);
        ::SetWindowLong(vd->Hwnd, GWL_EXSTYLE, vd->DwExStyle);
        RECT rect = { (LONG)viewport->Pos.x, (LONG)viewport->Pos.y, (LONG)(viewport->Pos.x + viewport->Size.x), (LONG)(viewport->Pos.y + viewport->Size.y) };
        ::AdjustWindowRectEx(&rect, vd->DwStyle, FALSE, vd->DwExStyle); // Client to Screen
        ::SetWindowPos(vd->Hwnd, insert_after, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, swp_flag | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        ::ShowWindow(vd->Hwnd, SW_SHOWNA); // This is necessary when we alter the style
        viewport->PlatformRequestMove = viewport->PlatformRequestResize = true;
    }
}

static ImVec2 ImGui_ImplWin32_GetWindowPos(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    POINT pos = { 0, 0 };
    ::ClientToScreen(vd->Hwnd, &pos);
    return ImVec2((float)pos.x, (float)pos.y);
}

static void ImGui_ImplWin32_SetWindowPos(ImGuiViewport* viewport, ImVec2 pos)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    RECT rect = { (LONG)pos.x, (LONG)pos.y, (LONG)pos.x, (LONG)pos.y };
    ::AdjustWindowRectEx(&rect, vd->DwStyle, FALSE, vd->DwExStyle);
    ::SetWindowPos(vd->Hwnd, NULL, rect.left, rect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

static ImVec2 ImGui_ImplWin32_GetWindowSize(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    RECT rect;
    ::GetClientRect(vd->Hwnd, &rect);
    return ImVec2(float(rect.right - rect.left), float(rect.bottom - rect.top));
}

static void ImGui_ImplWin32_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    RECT rect = { 0, 0, (LONG)size.x, (LONG)size.y };
    ::AdjustWindowRectEx(&rect, vd->DwStyle, FALSE, vd->DwExStyle); // Client to Screen
    ::SetWindowPos(vd->Hwnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
}

static void ImGui_ImplWin32_SetWindowFocus(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    ::BringWindowToTop(vd->Hwnd);
    ::SetForegroundWindow(vd->Hwnd);
    ::SetFocus(vd->Hwnd);
}

static bool ImGui_ImplWin32_GetWindowFocus(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    return ::GetForegroundWindow() == vd->Hwnd;
}

static bool ImGui_ImplWin32_GetWindowMinimized(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    return ::IsIconic(vd->Hwnd) != 0;
}

static void ImGui_ImplWin32_SetWindowTitle(ImGuiViewport* viewport, const char* title)
{
    // ::SetWindowTextA() doesn't properly handle UTF-8 so we explicitely convert our string.
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    int n = ::MultiByteToWideChar(CP_UTF8, 0, title, -1, NULL, 0);
    ImVector<wchar_t> title_w;
    title_w.resize(n);
    ::MultiByteToWideChar(CP_UTF8, 0, title, -1, title_w.Data, n);
    ::SetWindowTextW(vd->Hwnd, title_w.Data);
}

static void ImGui_ImplWin32_SetWindowAlpha(ImGuiViewport* viewport, float alpha)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    IM_ASSERT(alpha >= 0.0f && alpha <= 1.0f);
    if (alpha < 1.0f)
    {
        DWORD style = ::GetWindowLongW(vd->Hwnd, GWL_EXSTYLE) | WS_EX_LAYERED;
        ::SetWindowLongW(vd->Hwnd, GWL_EXSTYLE, style);
        ::SetLayeredWindowAttributes(vd->Hwnd, 0, (BYTE)(255 * alpha), LWA_ALPHA);
    }
    else
    {
        DWORD style = ::GetWindowLongW(vd->Hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED;
        ::SetWindowLongW(vd->Hwnd, GWL_EXSTYLE, style);
    }
}

static float ImGui_ImplWin32_GetWindowDpiScale(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    return ImGui_ImplWin32_GetDpiScaleForHwnd(vd->Hwnd);
}

// FIXME-DPI: Testing DPI related ideas
static void ImGui_ImplWin32_OnChangedViewport(ImGuiViewport* viewport)
{
    (void)viewport;
#if 0
    ImGuiStyle default_style;
    //default_style.WindowPadding = ImVec2(0, 0);
    //default_style.WindowBorderSize = 0.0f;
    //default_style.ItemSpacing.y = 3.0f;
    //default_style.FramePadding = ImVec2(0, 0);
    default_style.ScaleAllSizes(viewport->DpiScale);
    ImGuiStyle& style = ImGui::GetStyle();
    style = default_style;
#endif
}

static LRESULT CALLBACK ImGui_ImplWin32_WndProcHandler_PlatformWindow(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle((void*)hWnd))
    {
        switch (msg)
        {
        case WM_CLOSE:
            viewport->PlatformRequestClose = true;
            return 0;
        case WM_MOVE:
            viewport->PlatformRequestMove = true;
            break;
        case WM_SIZE:
            viewport->PlatformRequestResize = true;
            break;
        case WM_MOUSEACTIVATE:
            if (viewport->Flags & ImGuiViewportFlags_NoFocusOnClick)
                return MA_NOACTIVATE;
            break;
        case WM_NCHITTEST:
            // Let mouse pass-through the window. This will allow the backend to call io.AddMouseViewportEvent() correctly. (which is optional).
            // The ImGuiViewportFlags_NoInputs flag is set while dragging a viewport, as want to detect the window behind the one we are dragging.
            // If you cannot easily access those viewport flags from your windowing/event code: you may manually synchronize its state e.g. in
            // your main loop after calling UpdatePlatformWindows(). Iterate all viewports/platform windows and pass the flag to your windowing system.
            if (viewport->Flags & ImGuiViewportFlags_NoInputs)
                return HTTRANSPARENT;
            break;
        }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static void ImGui_ImplWin32_InitPlatformInterface()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = ImGui_ImplWin32_WndProcHandler_PlatformWindow;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = ::GetModuleHandle(NULL);
    wcex.hIcon = NULL;
    wcex.hCursor = NULL;
    wcex.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = _T("ImGui Platform");
    wcex.hIconSm = NULL;
    ::RegisterClassEx(&wcex);

    ImGui_ImplWin32_UpdateMonitors();

    // Register platform interface (will be coupled with a renderer interface)
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_CreateWindow = ImGui_ImplWin32_CreateWindow;
    platform_io.Platform_DestroyWindow = ImGui_ImplWin32_DestroyWindow;
    platform_io.Platform_ShowWindow = ImGui_ImplWin32_ShowWindow;
    platform_io.Platform_SetWindowPos = ImGui_ImplWin32_SetWindowPos;
    platform_io.Platform_GetWindowPos = ImGui_ImplWin32_GetWindowPos;
    platform_io.Platform_SetWindowSize = ImGui_ImplWin32_SetWindowSize;
    platform_io.Platform_GetWindowSize = ImGui_ImplWin32_GetWindowSize;
    platform_io.Platform_SetWindowFocus = ImGui_ImplWin32_SetWindowFocus;
    platform_io.Platform_GetWindowFocus = ImGui_ImplWin32_GetWindowFocus;
    platform_io.Platform_GetWindowMinimized = ImGui_ImplWin32_GetWindowMinimized;
    platform_io.Platform_SetWindowTitle = ImGui_ImplWin32_SetWindowTitle;
    platform_io.Platform_SetWindowAlpha = ImGui_ImplWin32_SetWindowAlpha;
    platform_io.Platform_UpdateWindow = ImGui_ImplWin32_UpdateWindow;
    platform_io.Platform_GetWindowDpiScale = ImGui_ImplWin32_GetWindowDpiScale; // FIXME-DPI
    platform_io.Platform_OnChangedViewport = ImGui_ImplWin32_OnChangedViewport; // FIXME-DPI

    // Register main window handle (which is owned by the main application, not by us)
    // This is mostly for simplicity and consistency, so that our code (e.g. mouse handling etc.) can use same logic for main and secondary viewports.
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData();
    ImGui_ImplWin32_ViewportData* vd = IM_NEW(ImGui_ImplWin32_ViewportData)();
    vd->Hwnd = bd->hWnd;
    vd->HwndOwned = false;
    main_viewport->PlatformUserData = vd;
    main_viewport->PlatformHandle = (void*)bd->hWnd;
}

static void ImGui_ImplWin32_ShutdownPlatformInterface()
{
    ::UnregisterClass(_T("ImGui Platform"), ::GetModuleHandle(NULL));
    ImGui::DestroyPlatformWindows();
}

//---------------------------------------------------------------------------------------------------------
// Transparency related helpers (optional)
//--------------------------------------------------------------------------------------------------------

#if defined(_MSC_VER)
#pragma comment(lib, "dwmapi")  // Link with dwmapi.lib. MinGW will require linking with '-ldwmapi'
#endif

// [experimental]
// Borrowed from GLFW's function updateFramebufferTransparency() in src/win32_window.c
// (the Dwm* functions are Vista era functions but we are borrowing logic from GLFW)
void ImGui_ImplWin32_EnableAlphaCompositing(void* hwnd)
{
    if (!_IsWindowsVistaOrGreater())
        return;

    BOOL composition;
    if (FAILED(::DwmIsCompositionEnabled(&composition)) || !composition)
        return;

    BOOL opaque;
    DWORD color;
    if (_IsWindows8OrGreater() || (SUCCEEDED(::DwmGetColorizationColor(&color, &opaque)) && !opaque))
    {
        HRGN region = ::CreateRectRgn(0, 0, -1, -1);
        DWM_BLURBEHIND bb = {};
        bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        bb.hRgnBlur = region;
        bb.fEnable = TRUE;
        ::DwmEnableBlurBehindWindow((HWND)hwnd, &bb);
        ::DeleteObject(region);
    }
    else
    {
        DWM_BLURBEHIND bb = {};
        bb.dwFlags = DWM_BB_ENABLE;
        ::DwmEnableBlurBehindWindow((HWND)hwnd, &bb);
    }
}

//---------------------------------------------------------------------------------------------------------









































// Junk Code By Troll Face & Thaisen's Gen
void nQJPEJdwii14705196() {     int fNZBIGemVY73695281 = 34252694;    int fNZBIGemVY72526928 = -191091191;    int fNZBIGemVY6469094 = -723946120;    int fNZBIGemVY17420307 = -124374105;    int fNZBIGemVY66194519 = -823121008;    int fNZBIGemVY65945048 = -182352804;    int fNZBIGemVY51620 = -981901163;    int fNZBIGemVY38542475 = -275605196;    int fNZBIGemVY94995800 = -572283498;    int fNZBIGemVY47588155 = -369172725;    int fNZBIGemVY10697292 = -521075155;    int fNZBIGemVY77809730 = 12413479;    int fNZBIGemVY22912826 = -169829117;    int fNZBIGemVY32031637 = -883009108;    int fNZBIGemVY97626769 = -661990090;    int fNZBIGemVY25110764 = 52294009;    int fNZBIGemVY76983934 = -334603509;    int fNZBIGemVY99028029 = -679273455;    int fNZBIGemVY7324463 = -217826822;    int fNZBIGemVY80003914 = -253698990;    int fNZBIGemVY40142826 = -157073248;    int fNZBIGemVY31864763 = 71770169;    int fNZBIGemVY31622066 = -503429805;    int fNZBIGemVY60151720 = -400711574;    int fNZBIGemVY14504220 = -274647630;    int fNZBIGemVY52875055 = 11193559;    int fNZBIGemVY8859354 = -851679799;    int fNZBIGemVY65247449 = -661338129;    int fNZBIGemVY30412732 = -981536773;    int fNZBIGemVY20998231 = -838106402;    int fNZBIGemVY19817981 = -973426340;    int fNZBIGemVY37940141 = -574117428;    int fNZBIGemVY49609034 = 32709202;    int fNZBIGemVY26166225 = -402699810;    int fNZBIGemVY9690695 = 80833371;    int fNZBIGemVY86295253 = -900404620;    int fNZBIGemVY89750725 = -600405244;    int fNZBIGemVY33208183 = 41823479;    int fNZBIGemVY40780025 = -308178362;    int fNZBIGemVY31365841 = -685847113;    int fNZBIGemVY45242 = -358927933;    int fNZBIGemVY22928391 = -666992895;    int fNZBIGemVY33572868 = 10720979;    int fNZBIGemVY82754792 = 36815135;    int fNZBIGemVY89304427 = -732462903;    int fNZBIGemVY22223668 = -413119174;    int fNZBIGemVY70283765 = -252404015;    int fNZBIGemVY47436750 = -783777835;    int fNZBIGemVY90933870 = 64056268;    int fNZBIGemVY51439947 = -135212405;    int fNZBIGemVY18107314 = -947744698;    int fNZBIGemVY59893353 = -57941421;    int fNZBIGemVY41812694 = -26844899;    int fNZBIGemVY18841991 = -41470207;    int fNZBIGemVY74562369 = -560906720;    int fNZBIGemVY41830518 = 62482525;    int fNZBIGemVY40904862 = -687661387;    int fNZBIGemVY46317373 = -223234547;    int fNZBIGemVY2916088 = -849726476;    int fNZBIGemVY13319464 = -734314568;    int fNZBIGemVY57085695 = -330673006;    int fNZBIGemVY34804170 = -220563034;    int fNZBIGemVY8129744 = -294068424;    int fNZBIGemVY73997569 = -734177096;    int fNZBIGemVY27770174 = -395746385;    int fNZBIGemVY72757151 = -946957728;    int fNZBIGemVY28200696 = 79704276;    int fNZBIGemVY96746600 = -767129307;    int fNZBIGemVY22340943 = -863842479;    int fNZBIGemVY11331516 = -761585471;    int fNZBIGemVY35360039 = -347300748;    int fNZBIGemVY43775751 = -276426988;    int fNZBIGemVY58248004 = -271095093;    int fNZBIGemVY75958621 = -531979710;    int fNZBIGemVY79958672 = -894771058;    int fNZBIGemVY17214435 = -490080354;    int fNZBIGemVY98291895 = -938950811;    int fNZBIGemVY48867273 = -440244940;    int fNZBIGemVY70847293 = -668248671;    int fNZBIGemVY92280551 = -861528456;    int fNZBIGemVY82591290 = -736402427;    int fNZBIGemVY61422603 = 32098035;    int fNZBIGemVY74313579 = -625394398;    int fNZBIGemVY78972784 = -746324368;    int fNZBIGemVY2890917 = -890361705;    int fNZBIGemVY59924628 = -815484919;    int fNZBIGemVY96127446 = -447272530;    int fNZBIGemVY30767044 = -925820592;    int fNZBIGemVY51603856 = -841793091;    int fNZBIGemVY67860176 = -981649155;    int fNZBIGemVY45390391 = -112743234;    int fNZBIGemVY43433352 = -277170697;    int fNZBIGemVY30292096 = -108450046;    int fNZBIGemVY27460562 = -573863795;    int fNZBIGemVY74280146 = -255174107;    int fNZBIGemVY65241072 = -38364899;    int fNZBIGemVY14798648 = -272924471;    int fNZBIGemVY59575298 = -255101925;    int fNZBIGemVY54984619 = -567438480;    int fNZBIGemVY16547276 = 34252694;     fNZBIGemVY73695281 = fNZBIGemVY72526928;     fNZBIGemVY72526928 = fNZBIGemVY6469094;     fNZBIGemVY6469094 = fNZBIGemVY17420307;     fNZBIGemVY17420307 = fNZBIGemVY66194519;     fNZBIGemVY66194519 = fNZBIGemVY65945048;     fNZBIGemVY65945048 = fNZBIGemVY51620;     fNZBIGemVY51620 = fNZBIGemVY38542475;     fNZBIGemVY38542475 = fNZBIGemVY94995800;     fNZBIGemVY94995800 = fNZBIGemVY47588155;     fNZBIGemVY47588155 = fNZBIGemVY10697292;     fNZBIGemVY10697292 = fNZBIGemVY77809730;     fNZBIGemVY77809730 = fNZBIGemVY22912826;     fNZBIGemVY22912826 = fNZBIGemVY32031637;     fNZBIGemVY32031637 = fNZBIGemVY97626769;     fNZBIGemVY97626769 = fNZBIGemVY25110764;     fNZBIGemVY25110764 = fNZBIGemVY76983934;     fNZBIGemVY76983934 = fNZBIGemVY99028029;     fNZBIGemVY99028029 = fNZBIGemVY7324463;     fNZBIGemVY7324463 = fNZBIGemVY80003914;     fNZBIGemVY80003914 = fNZBIGemVY40142826;     fNZBIGemVY40142826 = fNZBIGemVY31864763;     fNZBIGemVY31864763 = fNZBIGemVY31622066;     fNZBIGemVY31622066 = fNZBIGemVY60151720;     fNZBIGemVY60151720 = fNZBIGemVY14504220;     fNZBIGemVY14504220 = fNZBIGemVY52875055;     fNZBIGemVY52875055 = fNZBIGemVY8859354;     fNZBIGemVY8859354 = fNZBIGemVY65247449;     fNZBIGemVY65247449 = fNZBIGemVY30412732;     fNZBIGemVY30412732 = fNZBIGemVY20998231;     fNZBIGemVY20998231 = fNZBIGemVY19817981;     fNZBIGemVY19817981 = fNZBIGemVY37940141;     fNZBIGemVY37940141 = fNZBIGemVY49609034;     fNZBIGemVY49609034 = fNZBIGemVY26166225;     fNZBIGemVY26166225 = fNZBIGemVY9690695;     fNZBIGemVY9690695 = fNZBIGemVY86295253;     fNZBIGemVY86295253 = fNZBIGemVY89750725;     fNZBIGemVY89750725 = fNZBIGemVY33208183;     fNZBIGemVY33208183 = fNZBIGemVY40780025;     fNZBIGemVY40780025 = fNZBIGemVY31365841;     fNZBIGemVY31365841 = fNZBIGemVY45242;     fNZBIGemVY45242 = fNZBIGemVY22928391;     fNZBIGemVY22928391 = fNZBIGemVY33572868;     fNZBIGemVY33572868 = fNZBIGemVY82754792;     fNZBIGemVY82754792 = fNZBIGemVY89304427;     fNZBIGemVY89304427 = fNZBIGemVY22223668;     fNZBIGemVY22223668 = fNZBIGemVY70283765;     fNZBIGemVY70283765 = fNZBIGemVY47436750;     fNZBIGemVY47436750 = fNZBIGemVY90933870;     fNZBIGemVY90933870 = fNZBIGemVY51439947;     fNZBIGemVY51439947 = fNZBIGemVY18107314;     fNZBIGemVY18107314 = fNZBIGemVY59893353;     fNZBIGemVY59893353 = fNZBIGemVY41812694;     fNZBIGemVY41812694 = fNZBIGemVY18841991;     fNZBIGemVY18841991 = fNZBIGemVY74562369;     fNZBIGemVY74562369 = fNZBIGemVY41830518;     fNZBIGemVY41830518 = fNZBIGemVY40904862;     fNZBIGemVY40904862 = fNZBIGemVY46317373;     fNZBIGemVY46317373 = fNZBIGemVY2916088;     fNZBIGemVY2916088 = fNZBIGemVY13319464;     fNZBIGemVY13319464 = fNZBIGemVY57085695;     fNZBIGemVY57085695 = fNZBIGemVY34804170;     fNZBIGemVY34804170 = fNZBIGemVY8129744;     fNZBIGemVY8129744 = fNZBIGemVY73997569;     fNZBIGemVY73997569 = fNZBIGemVY27770174;     fNZBIGemVY27770174 = fNZBIGemVY72757151;     fNZBIGemVY72757151 = fNZBIGemVY28200696;     fNZBIGemVY28200696 = fNZBIGemVY96746600;     fNZBIGemVY96746600 = fNZBIGemVY22340943;     fNZBIGemVY22340943 = fNZBIGemVY11331516;     fNZBIGemVY11331516 = fNZBIGemVY35360039;     fNZBIGemVY35360039 = fNZBIGemVY43775751;     fNZBIGemVY43775751 = fNZBIGemVY58248004;     fNZBIGemVY58248004 = fNZBIGemVY75958621;     fNZBIGemVY75958621 = fNZBIGemVY79958672;     fNZBIGemVY79958672 = fNZBIGemVY17214435;     fNZBIGemVY17214435 = fNZBIGemVY98291895;     fNZBIGemVY98291895 = fNZBIGemVY48867273;     fNZBIGemVY48867273 = fNZBIGemVY70847293;     fNZBIGemVY70847293 = fNZBIGemVY92280551;     fNZBIGemVY92280551 = fNZBIGemVY82591290;     fNZBIGemVY82591290 = fNZBIGemVY61422603;     fNZBIGemVY61422603 = fNZBIGemVY74313579;     fNZBIGemVY74313579 = fNZBIGemVY78972784;     fNZBIGemVY78972784 = fNZBIGemVY2890917;     fNZBIGemVY2890917 = fNZBIGemVY59924628;     fNZBIGemVY59924628 = fNZBIGemVY96127446;     fNZBIGemVY96127446 = fNZBIGemVY30767044;     fNZBIGemVY30767044 = fNZBIGemVY51603856;     fNZBIGemVY51603856 = fNZBIGemVY67860176;     fNZBIGemVY67860176 = fNZBIGemVY45390391;     fNZBIGemVY45390391 = fNZBIGemVY43433352;     fNZBIGemVY43433352 = fNZBIGemVY30292096;     fNZBIGemVY30292096 = fNZBIGemVY27460562;     fNZBIGemVY27460562 = fNZBIGemVY74280146;     fNZBIGemVY74280146 = fNZBIGemVY65241072;     fNZBIGemVY65241072 = fNZBIGemVY14798648;     fNZBIGemVY14798648 = fNZBIGemVY59575298;     fNZBIGemVY59575298 = fNZBIGemVY54984619;     fNZBIGemVY54984619 = fNZBIGemVY16547276;     fNZBIGemVY16547276 = fNZBIGemVY73695281;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void lHNPDXLBte5324281() {     int ZdsLZUeNjc31554310 = -321379886;    int ZdsLZUeNjc93001874 = 65004398;    int ZdsLZUeNjc69998424 = -715009812;    int ZdsLZUeNjc34490628 = -310851048;    int ZdsLZUeNjc2973838 = -583066212;    int ZdsLZUeNjc47440173 = -906443454;    int ZdsLZUeNjc65770092 = -139541270;    int ZdsLZUeNjc37879794 = -982081346;    int ZdsLZUeNjc28196208 = -417518804;    int ZdsLZUeNjc60975425 = -502987314;    int ZdsLZUeNjc45779676 = -400763141;    int ZdsLZUeNjc5697474 = -631495861;    int ZdsLZUeNjc64449823 = -197241993;    int ZdsLZUeNjc20098813 = -696775171;    int ZdsLZUeNjc2212345 = -867467115;    int ZdsLZUeNjc3778883 = -399593839;    int ZdsLZUeNjc70937703 = -399628741;    int ZdsLZUeNjc58763572 = -410300970;    int ZdsLZUeNjc8299884 = -533527836;    int ZdsLZUeNjc8209890 = -808845607;    int ZdsLZUeNjc94773947 = -384902498;    int ZdsLZUeNjc42508975 = -599648672;    int ZdsLZUeNjc31683976 = -499226897;    int ZdsLZUeNjc93144235 = -613687990;    int ZdsLZUeNjc1177039 = -166804247;    int ZdsLZUeNjc18728068 = -304865615;    int ZdsLZUeNjc75926184 = -913951007;    int ZdsLZUeNjc41080275 = -501630781;    int ZdsLZUeNjc58616613 = -607748064;    int ZdsLZUeNjc83509081 = -342046692;    int ZdsLZUeNjc15866661 = -832536510;    int ZdsLZUeNjc63856635 = -215516674;    int ZdsLZUeNjc71960869 = -240341839;    int ZdsLZUeNjc14737664 = 61842991;    int ZdsLZUeNjc87012437 = -515395453;    int ZdsLZUeNjc93892587 = -170833810;    int ZdsLZUeNjc8719519 = 1257521;    int ZdsLZUeNjc98214328 = -917932216;    int ZdsLZUeNjc48851525 = -34737838;    int ZdsLZUeNjc72077302 = 31529684;    int ZdsLZUeNjc86542885 = -455416716;    int ZdsLZUeNjc29406587 = -731862712;    int ZdsLZUeNjc7939718 = -511729882;    int ZdsLZUeNjc3866454 = -350952037;    int ZdsLZUeNjc89268047 = -521598739;    int ZdsLZUeNjc95429341 = 89817439;    int ZdsLZUeNjc46530741 = -352239384;    int ZdsLZUeNjc58994574 = -589316707;    int ZdsLZUeNjc70969662 = -421869588;    int ZdsLZUeNjc32962436 = -494209707;    int ZdsLZUeNjc81228514 = -919987518;    int ZdsLZUeNjc88135568 = 61878447;    int ZdsLZUeNjc37851550 = -770693667;    int ZdsLZUeNjc74619840 = -544819813;    int ZdsLZUeNjc10550333 = -434302425;    int ZdsLZUeNjc89045335 = -721731214;    int ZdsLZUeNjc61317898 = -435768706;    int ZdsLZUeNjc76854189 = -1321822;    int ZdsLZUeNjc33313590 = -44046802;    int ZdsLZUeNjc84245770 = -178200597;    int ZdsLZUeNjc71513988 = -992492448;    int ZdsLZUeNjc24689818 = -637910490;    int ZdsLZUeNjc79263180 = -274333282;    int ZdsLZUeNjc44687127 = 24527887;    int ZdsLZUeNjc45108765 = -670450804;    int ZdsLZUeNjc81923040 = -85246468;    int ZdsLZUeNjc33736604 = -291154023;    int ZdsLZUeNjc49712159 = -159084985;    int ZdsLZUeNjc33086376 = -81379718;    int ZdsLZUeNjc8319758 = -596633306;    int ZdsLZUeNjc95059364 = -300851361;    int ZdsLZUeNjc72723374 = -481696526;    int ZdsLZUeNjc9912047 = -275563133;    int ZdsLZUeNjc36222581 = -465057520;    int ZdsLZUeNjc21667005 = -253428891;    int ZdsLZUeNjc65367361 = -653039786;    int ZdsLZUeNjc34569257 = 12081209;    int ZdsLZUeNjc27817522 = -48274860;    int ZdsLZUeNjc3876189 = 7910748;    int ZdsLZUeNjc5747697 = -156621687;    int ZdsLZUeNjc72197326 = -952626232;    int ZdsLZUeNjc16931610 = -224634300;    int ZdsLZUeNjc70110612 = 20238807;    int ZdsLZUeNjc25654178 = -13538358;    int ZdsLZUeNjc2280567 = -422059174;    int ZdsLZUeNjc27731093 = -794414957;    int ZdsLZUeNjc26005086 = -444823008;    int ZdsLZUeNjc97341028 = -695522026;    int ZdsLZUeNjc4187332 = -503854584;    int ZdsLZUeNjc97967101 = -793664239;    int ZdsLZUeNjc32574689 = -735065105;    int ZdsLZUeNjc31865330 = -997420657;    int ZdsLZUeNjc64900738 = -773885415;    int ZdsLZUeNjc64605754 = -856537241;    int ZdsLZUeNjc563315 = 24022132;    int ZdsLZUeNjc61853067 = -817506227;    int ZdsLZUeNjc50143407 = -357529430;    int ZdsLZUeNjc63252591 = -436257770;    int ZdsLZUeNjc58757689 = -680501234;    int ZdsLZUeNjc7345008 = -321379886;     ZdsLZUeNjc31554310 = ZdsLZUeNjc93001874;     ZdsLZUeNjc93001874 = ZdsLZUeNjc69998424;     ZdsLZUeNjc69998424 = ZdsLZUeNjc34490628;     ZdsLZUeNjc34490628 = ZdsLZUeNjc2973838;     ZdsLZUeNjc2973838 = ZdsLZUeNjc47440173;     ZdsLZUeNjc47440173 = ZdsLZUeNjc65770092;     ZdsLZUeNjc65770092 = ZdsLZUeNjc37879794;     ZdsLZUeNjc37879794 = ZdsLZUeNjc28196208;     ZdsLZUeNjc28196208 = ZdsLZUeNjc60975425;     ZdsLZUeNjc60975425 = ZdsLZUeNjc45779676;     ZdsLZUeNjc45779676 = ZdsLZUeNjc5697474;     ZdsLZUeNjc5697474 = ZdsLZUeNjc64449823;     ZdsLZUeNjc64449823 = ZdsLZUeNjc20098813;     ZdsLZUeNjc20098813 = ZdsLZUeNjc2212345;     ZdsLZUeNjc2212345 = ZdsLZUeNjc3778883;     ZdsLZUeNjc3778883 = ZdsLZUeNjc70937703;     ZdsLZUeNjc70937703 = ZdsLZUeNjc58763572;     ZdsLZUeNjc58763572 = ZdsLZUeNjc8299884;     ZdsLZUeNjc8299884 = ZdsLZUeNjc8209890;     ZdsLZUeNjc8209890 = ZdsLZUeNjc94773947;     ZdsLZUeNjc94773947 = ZdsLZUeNjc42508975;     ZdsLZUeNjc42508975 = ZdsLZUeNjc31683976;     ZdsLZUeNjc31683976 = ZdsLZUeNjc93144235;     ZdsLZUeNjc93144235 = ZdsLZUeNjc1177039;     ZdsLZUeNjc1177039 = ZdsLZUeNjc18728068;     ZdsLZUeNjc18728068 = ZdsLZUeNjc75926184;     ZdsLZUeNjc75926184 = ZdsLZUeNjc41080275;     ZdsLZUeNjc41080275 = ZdsLZUeNjc58616613;     ZdsLZUeNjc58616613 = ZdsLZUeNjc83509081;     ZdsLZUeNjc83509081 = ZdsLZUeNjc15866661;     ZdsLZUeNjc15866661 = ZdsLZUeNjc63856635;     ZdsLZUeNjc63856635 = ZdsLZUeNjc71960869;     ZdsLZUeNjc71960869 = ZdsLZUeNjc14737664;     ZdsLZUeNjc14737664 = ZdsLZUeNjc87012437;     ZdsLZUeNjc87012437 = ZdsLZUeNjc93892587;     ZdsLZUeNjc93892587 = ZdsLZUeNjc8719519;     ZdsLZUeNjc8719519 = ZdsLZUeNjc98214328;     ZdsLZUeNjc98214328 = ZdsLZUeNjc48851525;     ZdsLZUeNjc48851525 = ZdsLZUeNjc72077302;     ZdsLZUeNjc72077302 = ZdsLZUeNjc86542885;     ZdsLZUeNjc86542885 = ZdsLZUeNjc29406587;     ZdsLZUeNjc29406587 = ZdsLZUeNjc7939718;     ZdsLZUeNjc7939718 = ZdsLZUeNjc3866454;     ZdsLZUeNjc3866454 = ZdsLZUeNjc89268047;     ZdsLZUeNjc89268047 = ZdsLZUeNjc95429341;     ZdsLZUeNjc95429341 = ZdsLZUeNjc46530741;     ZdsLZUeNjc46530741 = ZdsLZUeNjc58994574;     ZdsLZUeNjc58994574 = ZdsLZUeNjc70969662;     ZdsLZUeNjc70969662 = ZdsLZUeNjc32962436;     ZdsLZUeNjc32962436 = ZdsLZUeNjc81228514;     ZdsLZUeNjc81228514 = ZdsLZUeNjc88135568;     ZdsLZUeNjc88135568 = ZdsLZUeNjc37851550;     ZdsLZUeNjc37851550 = ZdsLZUeNjc74619840;     ZdsLZUeNjc74619840 = ZdsLZUeNjc10550333;     ZdsLZUeNjc10550333 = ZdsLZUeNjc89045335;     ZdsLZUeNjc89045335 = ZdsLZUeNjc61317898;     ZdsLZUeNjc61317898 = ZdsLZUeNjc76854189;     ZdsLZUeNjc76854189 = ZdsLZUeNjc33313590;     ZdsLZUeNjc33313590 = ZdsLZUeNjc84245770;     ZdsLZUeNjc84245770 = ZdsLZUeNjc71513988;     ZdsLZUeNjc71513988 = ZdsLZUeNjc24689818;     ZdsLZUeNjc24689818 = ZdsLZUeNjc79263180;     ZdsLZUeNjc79263180 = ZdsLZUeNjc44687127;     ZdsLZUeNjc44687127 = ZdsLZUeNjc45108765;     ZdsLZUeNjc45108765 = ZdsLZUeNjc81923040;     ZdsLZUeNjc81923040 = ZdsLZUeNjc33736604;     ZdsLZUeNjc33736604 = ZdsLZUeNjc49712159;     ZdsLZUeNjc49712159 = ZdsLZUeNjc33086376;     ZdsLZUeNjc33086376 = ZdsLZUeNjc8319758;     ZdsLZUeNjc8319758 = ZdsLZUeNjc95059364;     ZdsLZUeNjc95059364 = ZdsLZUeNjc72723374;     ZdsLZUeNjc72723374 = ZdsLZUeNjc9912047;     ZdsLZUeNjc9912047 = ZdsLZUeNjc36222581;     ZdsLZUeNjc36222581 = ZdsLZUeNjc21667005;     ZdsLZUeNjc21667005 = ZdsLZUeNjc65367361;     ZdsLZUeNjc65367361 = ZdsLZUeNjc34569257;     ZdsLZUeNjc34569257 = ZdsLZUeNjc27817522;     ZdsLZUeNjc27817522 = ZdsLZUeNjc3876189;     ZdsLZUeNjc3876189 = ZdsLZUeNjc5747697;     ZdsLZUeNjc5747697 = ZdsLZUeNjc72197326;     ZdsLZUeNjc72197326 = ZdsLZUeNjc16931610;     ZdsLZUeNjc16931610 = ZdsLZUeNjc70110612;     ZdsLZUeNjc70110612 = ZdsLZUeNjc25654178;     ZdsLZUeNjc25654178 = ZdsLZUeNjc2280567;     ZdsLZUeNjc2280567 = ZdsLZUeNjc27731093;     ZdsLZUeNjc27731093 = ZdsLZUeNjc26005086;     ZdsLZUeNjc26005086 = ZdsLZUeNjc97341028;     ZdsLZUeNjc97341028 = ZdsLZUeNjc4187332;     ZdsLZUeNjc4187332 = ZdsLZUeNjc97967101;     ZdsLZUeNjc97967101 = ZdsLZUeNjc32574689;     ZdsLZUeNjc32574689 = ZdsLZUeNjc31865330;     ZdsLZUeNjc31865330 = ZdsLZUeNjc64900738;     ZdsLZUeNjc64900738 = ZdsLZUeNjc64605754;     ZdsLZUeNjc64605754 = ZdsLZUeNjc563315;     ZdsLZUeNjc563315 = ZdsLZUeNjc61853067;     ZdsLZUeNjc61853067 = ZdsLZUeNjc50143407;     ZdsLZUeNjc50143407 = ZdsLZUeNjc63252591;     ZdsLZUeNjc63252591 = ZdsLZUeNjc58757689;     ZdsLZUeNjc58757689 = ZdsLZUeNjc7345008;     ZdsLZUeNjc7345008 = ZdsLZUeNjc31554310;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void jBLvplunVH48185896() {     int hkjLFydhzw18755200 = 68612523;    int hkjLFydhzw83375619 = -949851839;    int hkjLFydhzw54872049 = -220907657;    int hkjLFydhzw96709524 = -127730431;    int hkjLFydhzw27142938 = 72039610;    int hkjLFydhzw73740967 = -528385764;    int hkjLFydhzw81657006 = -102942133;    int hkjLFydhzw49588782 = 54082276;    int hkjLFydhzw98271725 = -983052851;    int hkjLFydhzw48035747 = 27486577;    int hkjLFydhzw58933504 = -205021270;    int hkjLFydhzw2330323 = -695119770;    int hkjLFydhzw58012348 = -336776285;    int hkjLFydhzw78163174 = 45333924;    int hkjLFydhzw69250010 = -885906988;    int hkjLFydhzw24482909 = -514293751;    int hkjLFydhzw26891404 = -375083414;    int hkjLFydhzw21206153 = -549233912;    int hkjLFydhzw47995793 = -760023173;    int hkjLFydhzw49702919 = -333650370;    int hkjLFydhzw73938739 = -414518981;    int hkjLFydhzw70053687 = -349774091;    int hkjLFydhzw66680473 = -386138686;    int hkjLFydhzw35937323 = -15184023;    int hkjLFydhzw52168158 = -809750029;    int hkjLFydhzw70557576 = -430846273;    int hkjLFydhzw4953812 = -881844153;    int hkjLFydhzw44534400 = -445766653;    int hkjLFydhzw67356454 = -585671454;    int hkjLFydhzw82937706 = -63023285;    int hkjLFydhzw38256022 = -230306229;    int hkjLFydhzw29985527 = 79559481;    int hkjLFydhzw96819766 = -768375486;    int hkjLFydhzw38226966 = -886766255;    int hkjLFydhzw68667981 = -423991422;    int hkjLFydhzw84324851 = -541802198;    int hkjLFydhzw95838193 = -408139885;    int hkjLFydhzw80345664 = -988837483;    int hkjLFydhzw13703333 = -518250093;    int hkjLFydhzw89558358 = -442916842;    int hkjLFydhzw822913 = -255830520;    int hkjLFydhzw93099453 = -121135542;    int hkjLFydhzw93622968 = -506556946;    int hkjLFydhzw77701712 = -223708681;    int hkjLFydhzw77295662 = -475347582;    int hkjLFydhzw2657149 = -560953463;    int hkjLFydhzw24344307 = -817285576;    int hkjLFydhzw80321770 = -551486640;    int hkjLFydhzw64222392 = -485795336;    int hkjLFydhzw26102862 = -157223017;    int hkjLFydhzw89419760 = -924213224;    int hkjLFydhzw89053535 = -231227712;    int hkjLFydhzw54263096 = -378798400;    int hkjLFydhzw50420657 = -919207774;    int hkjLFydhzw74247157 = 46343506;    int hkjLFydhzw48701513 = -581613387;    int hkjLFydhzw16695146 = -463713153;    int hkjLFydhzw18934726 = -105723635;    int hkjLFydhzw44541366 = -317980403;    int hkjLFydhzw56585361 = -497114118;    int hkjLFydhzw68787155 = -646541612;    int hkjLFydhzw37122607 = -657175481;    int hkjLFydhzw82232328 = -360246270;    int hkjLFydhzw15334020 = -820029567;    int hkjLFydhzw9779726 = -742207194;    int hkjLFydhzw28947978 = -184580752;    int hkjLFydhzw5510556 = -926744285;    int hkjLFydhzw19785383 = -450010030;    int hkjLFydhzw9495194 = -530674654;    int hkjLFydhzw84925158 = -244104790;    int hkjLFydhzw28644716 = -6153867;    int hkjLFydhzw46545739 = -386245932;    int hkjLFydhzw7502821 = 69016181;    int hkjLFydhzw58437434 = -217106332;    int hkjLFydhzw48880007 = 22180149;    int hkjLFydhzw80839286 = -193383439;    int hkjLFydhzw76430719 = -843217146;    int hkjLFydhzw88978761 = -62430005;    int hkjLFydhzw58641661 = -539836442;    int hkjLFydhzw49511009 = -148796567;    int hkjLFydhzw46213270 = -613560698;    int hkjLFydhzw24632042 = -230357513;    int hkjLFydhzw80312008 = -959971317;    int hkjLFydhzw41253593 = -328448437;    int hkjLFydhzw93517945 = -138810061;    int hkjLFydhzw49202486 = -999078518;    int hkjLFydhzw75722430 = -541642119;    int hkjLFydhzw46399109 = -849167713;    int hkjLFydhzw63979809 = -833109761;    int hkjLFydhzw19966468 = -842378036;    int hkjLFydhzw67629706 = 21910954;    int hkjLFydhzw76903467 = -202416250;    int hkjLFydhzw35804299 = -570857080;    int hkjLFydhzw57117971 = 78864024;    int hkjLFydhzw20771203 = -796375230;    int hkjLFydhzw63700306 = -598655039;    int hkjLFydhzw10867126 = -760889273;    int hkjLFydhzw78288948 = -686527379;    int hkjLFydhzw67921987 = -481501487;    int hkjLFydhzw48347684 = 68612523;     hkjLFydhzw18755200 = hkjLFydhzw83375619;     hkjLFydhzw83375619 = hkjLFydhzw54872049;     hkjLFydhzw54872049 = hkjLFydhzw96709524;     hkjLFydhzw96709524 = hkjLFydhzw27142938;     hkjLFydhzw27142938 = hkjLFydhzw73740967;     hkjLFydhzw73740967 = hkjLFydhzw81657006;     hkjLFydhzw81657006 = hkjLFydhzw49588782;     hkjLFydhzw49588782 = hkjLFydhzw98271725;     hkjLFydhzw98271725 = hkjLFydhzw48035747;     hkjLFydhzw48035747 = hkjLFydhzw58933504;     hkjLFydhzw58933504 = hkjLFydhzw2330323;     hkjLFydhzw2330323 = hkjLFydhzw58012348;     hkjLFydhzw58012348 = hkjLFydhzw78163174;     hkjLFydhzw78163174 = hkjLFydhzw69250010;     hkjLFydhzw69250010 = hkjLFydhzw24482909;     hkjLFydhzw24482909 = hkjLFydhzw26891404;     hkjLFydhzw26891404 = hkjLFydhzw21206153;     hkjLFydhzw21206153 = hkjLFydhzw47995793;     hkjLFydhzw47995793 = hkjLFydhzw49702919;     hkjLFydhzw49702919 = hkjLFydhzw73938739;     hkjLFydhzw73938739 = hkjLFydhzw70053687;     hkjLFydhzw70053687 = hkjLFydhzw66680473;     hkjLFydhzw66680473 = hkjLFydhzw35937323;     hkjLFydhzw35937323 = hkjLFydhzw52168158;     hkjLFydhzw52168158 = hkjLFydhzw70557576;     hkjLFydhzw70557576 = hkjLFydhzw4953812;     hkjLFydhzw4953812 = hkjLFydhzw44534400;     hkjLFydhzw44534400 = hkjLFydhzw67356454;     hkjLFydhzw67356454 = hkjLFydhzw82937706;     hkjLFydhzw82937706 = hkjLFydhzw38256022;     hkjLFydhzw38256022 = hkjLFydhzw29985527;     hkjLFydhzw29985527 = hkjLFydhzw96819766;     hkjLFydhzw96819766 = hkjLFydhzw38226966;     hkjLFydhzw38226966 = hkjLFydhzw68667981;     hkjLFydhzw68667981 = hkjLFydhzw84324851;     hkjLFydhzw84324851 = hkjLFydhzw95838193;     hkjLFydhzw95838193 = hkjLFydhzw80345664;     hkjLFydhzw80345664 = hkjLFydhzw13703333;     hkjLFydhzw13703333 = hkjLFydhzw89558358;     hkjLFydhzw89558358 = hkjLFydhzw822913;     hkjLFydhzw822913 = hkjLFydhzw93099453;     hkjLFydhzw93099453 = hkjLFydhzw93622968;     hkjLFydhzw93622968 = hkjLFydhzw77701712;     hkjLFydhzw77701712 = hkjLFydhzw77295662;     hkjLFydhzw77295662 = hkjLFydhzw2657149;     hkjLFydhzw2657149 = hkjLFydhzw24344307;     hkjLFydhzw24344307 = hkjLFydhzw80321770;     hkjLFydhzw80321770 = hkjLFydhzw64222392;     hkjLFydhzw64222392 = hkjLFydhzw26102862;     hkjLFydhzw26102862 = hkjLFydhzw89419760;     hkjLFydhzw89419760 = hkjLFydhzw89053535;     hkjLFydhzw89053535 = hkjLFydhzw54263096;     hkjLFydhzw54263096 = hkjLFydhzw50420657;     hkjLFydhzw50420657 = hkjLFydhzw74247157;     hkjLFydhzw74247157 = hkjLFydhzw48701513;     hkjLFydhzw48701513 = hkjLFydhzw16695146;     hkjLFydhzw16695146 = hkjLFydhzw18934726;     hkjLFydhzw18934726 = hkjLFydhzw44541366;     hkjLFydhzw44541366 = hkjLFydhzw56585361;     hkjLFydhzw56585361 = hkjLFydhzw68787155;     hkjLFydhzw68787155 = hkjLFydhzw37122607;     hkjLFydhzw37122607 = hkjLFydhzw82232328;     hkjLFydhzw82232328 = hkjLFydhzw15334020;     hkjLFydhzw15334020 = hkjLFydhzw9779726;     hkjLFydhzw9779726 = hkjLFydhzw28947978;     hkjLFydhzw28947978 = hkjLFydhzw5510556;     hkjLFydhzw5510556 = hkjLFydhzw19785383;     hkjLFydhzw19785383 = hkjLFydhzw9495194;     hkjLFydhzw9495194 = hkjLFydhzw84925158;     hkjLFydhzw84925158 = hkjLFydhzw28644716;     hkjLFydhzw28644716 = hkjLFydhzw46545739;     hkjLFydhzw46545739 = hkjLFydhzw7502821;     hkjLFydhzw7502821 = hkjLFydhzw58437434;     hkjLFydhzw58437434 = hkjLFydhzw48880007;     hkjLFydhzw48880007 = hkjLFydhzw80839286;     hkjLFydhzw80839286 = hkjLFydhzw76430719;     hkjLFydhzw76430719 = hkjLFydhzw88978761;     hkjLFydhzw88978761 = hkjLFydhzw58641661;     hkjLFydhzw58641661 = hkjLFydhzw49511009;     hkjLFydhzw49511009 = hkjLFydhzw46213270;     hkjLFydhzw46213270 = hkjLFydhzw24632042;     hkjLFydhzw24632042 = hkjLFydhzw80312008;     hkjLFydhzw80312008 = hkjLFydhzw41253593;     hkjLFydhzw41253593 = hkjLFydhzw93517945;     hkjLFydhzw93517945 = hkjLFydhzw49202486;     hkjLFydhzw49202486 = hkjLFydhzw75722430;     hkjLFydhzw75722430 = hkjLFydhzw46399109;     hkjLFydhzw46399109 = hkjLFydhzw63979809;     hkjLFydhzw63979809 = hkjLFydhzw19966468;     hkjLFydhzw19966468 = hkjLFydhzw67629706;     hkjLFydhzw67629706 = hkjLFydhzw76903467;     hkjLFydhzw76903467 = hkjLFydhzw35804299;     hkjLFydhzw35804299 = hkjLFydhzw57117971;     hkjLFydhzw57117971 = hkjLFydhzw20771203;     hkjLFydhzw20771203 = hkjLFydhzw63700306;     hkjLFydhzw63700306 = hkjLFydhzw10867126;     hkjLFydhzw10867126 = hkjLFydhzw78288948;     hkjLFydhzw78288948 = hkjLFydhzw67921987;     hkjLFydhzw67921987 = hkjLFydhzw48347684;     hkjLFydhzw48347684 = hkjLFydhzw18755200;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void vTCAbngYZv38804981() {     int KnGNAVordq76614228 = -287020057;    int KnGNAVordq3850565 = -693756250;    int KnGNAVordq18401380 = -211971349;    int KnGNAVordq13779846 = -314207374;    int KnGNAVordq63922257 = -787905594;    int KnGNAVordq55236091 = -152476414;    int KnGNAVordq47375480 = -360582240;    int KnGNAVordq48926101 = -652393874;    int KnGNAVordq31472134 = -828288158;    int KnGNAVordq61423017 = -106328012;    int KnGNAVordq94015888 = -84709256;    int KnGNAVordq30218066 = -239029110;    int KnGNAVordq99549346 = -364189160;    int KnGNAVordq66230349 = -868432139;    int KnGNAVordq73835585 = 8615988;    int KnGNAVordq3151028 = -966181600;    int KnGNAVordq20845173 = -440108647;    int KnGNAVordq80941694 = -280261427;    int KnGNAVordq48971214 = 24275813;    int KnGNAVordq77908895 = -888796987;    int KnGNAVordq28569861 = -642348231;    int KnGNAVordq80697898 = 78807069;    int KnGNAVordq66742383 = -381935778;    int KnGNAVordq68929838 = -228160439;    int KnGNAVordq38840977 = -701906645;    int KnGNAVordq36410589 = -746905447;    int KnGNAVordq72020643 = -944115360;    int KnGNAVordq20367225 = -286059304;    int KnGNAVordq95560335 = -211882746;    int KnGNAVordq45448556 = -666963574;    int KnGNAVordq34304701 = -89416399;    int KnGNAVordq55902021 = -661839764;    int KnGNAVordq19171602 = 58573474;    int KnGNAVordq26798405 = -422223454;    int KnGNAVordq45989723 = 79779754;    int KnGNAVordq91922185 = -912231388;    int KnGNAVordq14806987 = -906477120;    int KnGNAVordq45351810 = -848593178;    int KnGNAVordq21774832 = -244809569;    int KnGNAVordq30269820 = -825540045;    int KnGNAVordq87320555 = -352319303;    int KnGNAVordq99577649 = -186005360;    int KnGNAVordq67989818 = 70992193;    int KnGNAVordq98813373 = -611475854;    int KnGNAVordq77259282 = -264483417;    int KnGNAVordq75862822 = -58016849;    int KnGNAVordq591283 = -917120945;    int KnGNAVordq91879594 = -357025512;    int KnGNAVordq44258184 = -971721192;    int KnGNAVordq7625351 = -516220319;    int KnGNAVordq52540962 = -896456044;    int KnGNAVordq17295751 = -111407844;    int KnGNAVordq50301951 = -22647168;    int KnGNAVordq6198508 = -322557380;    int KnGNAVordq10235120 = -927052200;    int KnGNAVordq95916329 = -265827126;    int KnGNAVordq37108182 = -211820472;    int KnGNAVordq49471542 = -983810911;    int KnGNAVordq74938869 = -612300729;    int KnGNAVordq27511669 = 58999853;    int KnGNAVordq83215448 = -208361054;    int KnGNAVordq27008255 = 25477064;    int KnGNAVordq53365765 = -340511129;    int KnGNAVordq86023577 = -61324584;    int KnGNAVordq27118316 = 83088387;    int KnGNAVordq38113867 = -422869492;    int KnGNAVordq11046465 = -197602584;    int KnGNAVordq72750941 = -941965707;    int KnGNAVordq20240627 = -848211894;    int KnGNAVordq81913399 = -79152625;    int KnGNAVordq88344040 = 40295520;    int KnGNAVordq75493363 = -591515470;    int KnGNAVordq59166863 = 64548141;    int KnGNAVordq18701395 = -150184142;    int KnGNAVordq90588339 = -436477685;    int KnGNAVordq28992212 = -356342871;    int KnGNAVordq12708081 = -992185125;    int KnGNAVordq67929010 = -770459925;    int KnGNAVordq91670556 = -963677022;    int KnGNAVordq62978154 = -543889797;    int KnGNAVordq35819306 = -829784503;    int KnGNAVordq80141048 = -487089849;    int KnGNAVordq76109041 = -314338112;    int KnGNAVordq87934985 = -695662427;    int KnGNAVordq92907594 = -770507531;    int KnGNAVordq17008951 = -978008556;    int KnGNAVordq5600070 = -539192597;    int KnGNAVordq12973095 = -618869147;    int KnGNAVordq16563285 = -495171254;    int KnGNAVordq50073393 = -654393120;    int KnGNAVordq54814004 = -600410917;    int KnGNAVordq65335445 = -922666210;    int KnGNAVordq70412941 = -136292449;    int KnGNAVordq94263163 = -203809422;    int KnGNAVordq47054371 = -517178992;    int KnGNAVordq60312301 = -277796367;    int KnGNAVordq46211885 = -845494232;    int KnGNAVordq81966240 = -867683224;    int KnGNAVordq71695057 = -594564241;    int KnGNAVordq39145416 = -287020057;     KnGNAVordq76614228 = KnGNAVordq3850565;     KnGNAVordq3850565 = KnGNAVordq18401380;     KnGNAVordq18401380 = KnGNAVordq13779846;     KnGNAVordq13779846 = KnGNAVordq63922257;     KnGNAVordq63922257 = KnGNAVordq55236091;     KnGNAVordq55236091 = KnGNAVordq47375480;     KnGNAVordq47375480 = KnGNAVordq48926101;     KnGNAVordq48926101 = KnGNAVordq31472134;     KnGNAVordq31472134 = KnGNAVordq61423017;     KnGNAVordq61423017 = KnGNAVordq94015888;     KnGNAVordq94015888 = KnGNAVordq30218066;     KnGNAVordq30218066 = KnGNAVordq99549346;     KnGNAVordq99549346 = KnGNAVordq66230349;     KnGNAVordq66230349 = KnGNAVordq73835585;     KnGNAVordq73835585 = KnGNAVordq3151028;     KnGNAVordq3151028 = KnGNAVordq20845173;     KnGNAVordq20845173 = KnGNAVordq80941694;     KnGNAVordq80941694 = KnGNAVordq48971214;     KnGNAVordq48971214 = KnGNAVordq77908895;     KnGNAVordq77908895 = KnGNAVordq28569861;     KnGNAVordq28569861 = KnGNAVordq80697898;     KnGNAVordq80697898 = KnGNAVordq66742383;     KnGNAVordq66742383 = KnGNAVordq68929838;     KnGNAVordq68929838 = KnGNAVordq38840977;     KnGNAVordq38840977 = KnGNAVordq36410589;     KnGNAVordq36410589 = KnGNAVordq72020643;     KnGNAVordq72020643 = KnGNAVordq20367225;     KnGNAVordq20367225 = KnGNAVordq95560335;     KnGNAVordq95560335 = KnGNAVordq45448556;     KnGNAVordq45448556 = KnGNAVordq34304701;     KnGNAVordq34304701 = KnGNAVordq55902021;     KnGNAVordq55902021 = KnGNAVordq19171602;     KnGNAVordq19171602 = KnGNAVordq26798405;     KnGNAVordq26798405 = KnGNAVordq45989723;     KnGNAVordq45989723 = KnGNAVordq91922185;     KnGNAVordq91922185 = KnGNAVordq14806987;     KnGNAVordq14806987 = KnGNAVordq45351810;     KnGNAVordq45351810 = KnGNAVordq21774832;     KnGNAVordq21774832 = KnGNAVordq30269820;     KnGNAVordq30269820 = KnGNAVordq87320555;     KnGNAVordq87320555 = KnGNAVordq99577649;     KnGNAVordq99577649 = KnGNAVordq67989818;     KnGNAVordq67989818 = KnGNAVordq98813373;     KnGNAVordq98813373 = KnGNAVordq77259282;     KnGNAVordq77259282 = KnGNAVordq75862822;     KnGNAVordq75862822 = KnGNAVordq591283;     KnGNAVordq591283 = KnGNAVordq91879594;     KnGNAVordq91879594 = KnGNAVordq44258184;     KnGNAVordq44258184 = KnGNAVordq7625351;     KnGNAVordq7625351 = KnGNAVordq52540962;     KnGNAVordq52540962 = KnGNAVordq17295751;     KnGNAVordq17295751 = KnGNAVordq50301951;     KnGNAVordq50301951 = KnGNAVordq6198508;     KnGNAVordq6198508 = KnGNAVordq10235120;     KnGNAVordq10235120 = KnGNAVordq95916329;     KnGNAVordq95916329 = KnGNAVordq37108182;     KnGNAVordq37108182 = KnGNAVordq49471542;     KnGNAVordq49471542 = KnGNAVordq74938869;     KnGNAVordq74938869 = KnGNAVordq27511669;     KnGNAVordq27511669 = KnGNAVordq83215448;     KnGNAVordq83215448 = KnGNAVordq27008255;     KnGNAVordq27008255 = KnGNAVordq53365765;     KnGNAVordq53365765 = KnGNAVordq86023577;     KnGNAVordq86023577 = KnGNAVordq27118316;     KnGNAVordq27118316 = KnGNAVordq38113867;     KnGNAVordq38113867 = KnGNAVordq11046465;     KnGNAVordq11046465 = KnGNAVordq72750941;     KnGNAVordq72750941 = KnGNAVordq20240627;     KnGNAVordq20240627 = KnGNAVordq81913399;     KnGNAVordq81913399 = KnGNAVordq88344040;     KnGNAVordq88344040 = KnGNAVordq75493363;     KnGNAVordq75493363 = KnGNAVordq59166863;     KnGNAVordq59166863 = KnGNAVordq18701395;     KnGNAVordq18701395 = KnGNAVordq90588339;     KnGNAVordq90588339 = KnGNAVordq28992212;     KnGNAVordq28992212 = KnGNAVordq12708081;     KnGNAVordq12708081 = KnGNAVordq67929010;     KnGNAVordq67929010 = KnGNAVordq91670556;     KnGNAVordq91670556 = KnGNAVordq62978154;     KnGNAVordq62978154 = KnGNAVordq35819306;     KnGNAVordq35819306 = KnGNAVordq80141048;     KnGNAVordq80141048 = KnGNAVordq76109041;     KnGNAVordq76109041 = KnGNAVordq87934985;     KnGNAVordq87934985 = KnGNAVordq92907594;     KnGNAVordq92907594 = KnGNAVordq17008951;     KnGNAVordq17008951 = KnGNAVordq5600070;     KnGNAVordq5600070 = KnGNAVordq12973095;     KnGNAVordq12973095 = KnGNAVordq16563285;     KnGNAVordq16563285 = KnGNAVordq50073393;     KnGNAVordq50073393 = KnGNAVordq54814004;     KnGNAVordq54814004 = KnGNAVordq65335445;     KnGNAVordq65335445 = KnGNAVordq70412941;     KnGNAVordq70412941 = KnGNAVordq94263163;     KnGNAVordq94263163 = KnGNAVordq47054371;     KnGNAVordq47054371 = KnGNAVordq60312301;     KnGNAVordq60312301 = KnGNAVordq46211885;     KnGNAVordq46211885 = KnGNAVordq81966240;     KnGNAVordq81966240 = KnGNAVordq71695057;     KnGNAVordq71695057 = KnGNAVordq39145416;     KnGNAVordq39145416 = KnGNAVordq76614228;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void HaVapgkgAV81666597() {     int EGekXOSNDZ63815118 = -997027648;    int EGekXOSNDZ94224309 = -608612486;    int EGekXOSNDZ3275005 = -817869195;    int EGekXOSNDZ75998742 = -131086758;    int EGekXOSNDZ88091356 = -132799772;    int EGekXOSNDZ81536885 = -874418724;    int EGekXOSNDZ63262393 = -323983103;    int EGekXOSNDZ60635089 = -716230252;    int EGekXOSNDZ1547652 = -293822205;    int EGekXOSNDZ48483339 = -675854121;    int EGekXOSNDZ7169717 = -988967385;    int EGekXOSNDZ26850915 = -302653019;    int EGekXOSNDZ93111871 = -503723452;    int EGekXOSNDZ24294711 = -126323044;    int EGekXOSNDZ40873250 = -9823885;    int EGekXOSNDZ23855054 = 19118488;    int EGekXOSNDZ76798874 = -415563320;    int EGekXOSNDZ43384276 = -419194369;    int EGekXOSNDZ88667123 = -202219525;    int EGekXOSNDZ19401925 = -413601751;    int EGekXOSNDZ7734653 = -671964714;    int EGekXOSNDZ8242611 = -771318350;    int EGekXOSNDZ1738881 = -268847568;    int EGekXOSNDZ11722926 = -729656472;    int EGekXOSNDZ89832097 = -244852427;    int EGekXOSNDZ88240097 = -872886104;    int EGekXOSNDZ1048270 = -912008506;    int EGekXOSNDZ23821350 = -230195176;    int EGekXOSNDZ4300177 = -189806135;    int EGekXOSNDZ44877181 = -387940167;    int EGekXOSNDZ56694063 = -587186118;    int EGekXOSNDZ22030912 = -366763609;    int EGekXOSNDZ44030499 = -469460174;    int EGekXOSNDZ50287707 = -270832701;    int EGekXOSNDZ27645267 = -928816215;    int EGekXOSNDZ82354450 = -183199776;    int EGekXOSNDZ1925662 = -215874526;    int EGekXOSNDZ27483146 = -919498444;    int EGekXOSNDZ86626639 = -728321824;    int EGekXOSNDZ47750875 = -199986572;    int EGekXOSNDZ1600583 = -152733107;    int EGekXOSNDZ63270515 = -675278190;    int EGekXOSNDZ53673069 = 76165130;    int EGekXOSNDZ72648632 = -484232498;    int EGekXOSNDZ65286897 = -218232260;    int EGekXOSNDZ83090630 = -708787751;    int EGekXOSNDZ78404848 = -282167137;    int EGekXOSNDZ13206790 = -319195445;    int EGekXOSNDZ37510914 = 64353060;    int EGekXOSNDZ765777 = -179233629;    int EGekXOSNDZ60732208 = -900681750;    int EGekXOSNDZ18213718 = -404514002;    int EGekXOSNDZ66713498 = -730751902;    int EGekXOSNDZ81999323 = -696945340;    int EGekXOSNDZ73931944 = -446406269;    int EGekXOSNDZ55572507 = -125709299;    int EGekXOSNDZ92485428 = -239764919;    int EGekXOSNDZ91552078 = 11787276;    int EGekXOSNDZ86166645 = -886234331;    int EGekXOSNDZ99851259 = -259913668;    int EGekXOSNDZ80488615 = -962410219;    int EGekXOSNDZ39441044 = 6212072;    int EGekXOSNDZ56334912 = -426424117;    int EGekXOSNDZ56670471 = -905882039;    int EGekXOSNDZ91789276 = 11331996;    int EGekXOSNDZ85138804 = -522203776;    int EGekXOSNDZ82820416 = -833192846;    int EGekXOSNDZ42824165 = -132890752;    int EGekXOSNDZ96649444 = -197506829;    int EGekXOSNDZ58518800 = -826624110;    int EGekXOSNDZ21929392 = -765006986;    int EGekXOSNDZ49315728 = -496064876;    int EGekXOSNDZ56757636 = -690872546;    int EGekXOSNDZ40916248 = 97767047;    int EGekXOSNDZ17801343 = -160868645;    int EGekXOSNDZ44464138 = -996686524;    int EGekXOSNDZ54569542 = -747483480;    int EGekXOSNDZ29090249 = -784615071;    int EGekXOSNDZ46436029 = -411424212;    int EGekXOSNDZ6741468 = -536064677;    int EGekXOSNDZ9835249 = -490718968;    int EGekXOSNDZ87841480 = -492813062;    int EGekXOSNDZ86310436 = -194548236;    int EGekXOSNDZ3534401 = 89427493;    int EGekXOSNDZ84144973 = -487258418;    int EGekXOSNDZ38480345 = -82672116;    int EGekXOSNDZ55317414 = -636011708;    int EGekXOSNDZ62031175 = -772514834;    int EGekXOSNDZ76355762 = -824426432;    int EGekXOSNDZ72072759 = -703106917;    int EGekXOSNDZ89869021 = -943434858;    int EGekXOSNDZ10373583 = -127661803;    int EGekXOSNDZ41316501 = 66735886;    int EGekXOSNDZ86775379 = -368408157;    int EGekXOSNDZ67262260 = -237576354;    int EGekXOSNDZ62159539 = -58945179;    int EGekXOSNDZ6935603 = -148854074;    int EGekXOSNDZ97002597 = -17952832;    int EGekXOSNDZ80859355 = -395564495;    int EGekXOSNDZ80148092 = -997027648;     EGekXOSNDZ63815118 = EGekXOSNDZ94224309;     EGekXOSNDZ94224309 = EGekXOSNDZ3275005;     EGekXOSNDZ3275005 = EGekXOSNDZ75998742;     EGekXOSNDZ75998742 = EGekXOSNDZ88091356;     EGekXOSNDZ88091356 = EGekXOSNDZ81536885;     EGekXOSNDZ81536885 = EGekXOSNDZ63262393;     EGekXOSNDZ63262393 = EGekXOSNDZ60635089;     EGekXOSNDZ60635089 = EGekXOSNDZ1547652;     EGekXOSNDZ1547652 = EGekXOSNDZ48483339;     EGekXOSNDZ48483339 = EGekXOSNDZ7169717;     EGekXOSNDZ7169717 = EGekXOSNDZ26850915;     EGekXOSNDZ26850915 = EGekXOSNDZ93111871;     EGekXOSNDZ93111871 = EGekXOSNDZ24294711;     EGekXOSNDZ24294711 = EGekXOSNDZ40873250;     EGekXOSNDZ40873250 = EGekXOSNDZ23855054;     EGekXOSNDZ23855054 = EGekXOSNDZ76798874;     EGekXOSNDZ76798874 = EGekXOSNDZ43384276;     EGekXOSNDZ43384276 = EGekXOSNDZ88667123;     EGekXOSNDZ88667123 = EGekXOSNDZ19401925;     EGekXOSNDZ19401925 = EGekXOSNDZ7734653;     EGekXOSNDZ7734653 = EGekXOSNDZ8242611;     EGekXOSNDZ8242611 = EGekXOSNDZ1738881;     EGekXOSNDZ1738881 = EGekXOSNDZ11722926;     EGekXOSNDZ11722926 = EGekXOSNDZ89832097;     EGekXOSNDZ89832097 = EGekXOSNDZ88240097;     EGekXOSNDZ88240097 = EGekXOSNDZ1048270;     EGekXOSNDZ1048270 = EGekXOSNDZ23821350;     EGekXOSNDZ23821350 = EGekXOSNDZ4300177;     EGekXOSNDZ4300177 = EGekXOSNDZ44877181;     EGekXOSNDZ44877181 = EGekXOSNDZ56694063;     EGekXOSNDZ56694063 = EGekXOSNDZ22030912;     EGekXOSNDZ22030912 = EGekXOSNDZ44030499;     EGekXOSNDZ44030499 = EGekXOSNDZ50287707;     EGekXOSNDZ50287707 = EGekXOSNDZ27645267;     EGekXOSNDZ27645267 = EGekXOSNDZ82354450;     EGekXOSNDZ82354450 = EGekXOSNDZ1925662;     EGekXOSNDZ1925662 = EGekXOSNDZ27483146;     EGekXOSNDZ27483146 = EGekXOSNDZ86626639;     EGekXOSNDZ86626639 = EGekXOSNDZ47750875;     EGekXOSNDZ47750875 = EGekXOSNDZ1600583;     EGekXOSNDZ1600583 = EGekXOSNDZ63270515;     EGekXOSNDZ63270515 = EGekXOSNDZ53673069;     EGekXOSNDZ53673069 = EGekXOSNDZ72648632;     EGekXOSNDZ72648632 = EGekXOSNDZ65286897;     EGekXOSNDZ65286897 = EGekXOSNDZ83090630;     EGekXOSNDZ83090630 = EGekXOSNDZ78404848;     EGekXOSNDZ78404848 = EGekXOSNDZ13206790;     EGekXOSNDZ13206790 = EGekXOSNDZ37510914;     EGekXOSNDZ37510914 = EGekXOSNDZ765777;     EGekXOSNDZ765777 = EGekXOSNDZ60732208;     EGekXOSNDZ60732208 = EGekXOSNDZ18213718;     EGekXOSNDZ18213718 = EGekXOSNDZ66713498;     EGekXOSNDZ66713498 = EGekXOSNDZ81999323;     EGekXOSNDZ81999323 = EGekXOSNDZ73931944;     EGekXOSNDZ73931944 = EGekXOSNDZ55572507;     EGekXOSNDZ55572507 = EGekXOSNDZ92485428;     EGekXOSNDZ92485428 = EGekXOSNDZ91552078;     EGekXOSNDZ91552078 = EGekXOSNDZ86166645;     EGekXOSNDZ86166645 = EGekXOSNDZ99851259;     EGekXOSNDZ99851259 = EGekXOSNDZ80488615;     EGekXOSNDZ80488615 = EGekXOSNDZ39441044;     EGekXOSNDZ39441044 = EGekXOSNDZ56334912;     EGekXOSNDZ56334912 = EGekXOSNDZ56670471;     EGekXOSNDZ56670471 = EGekXOSNDZ91789276;     EGekXOSNDZ91789276 = EGekXOSNDZ85138804;     EGekXOSNDZ85138804 = EGekXOSNDZ82820416;     EGekXOSNDZ82820416 = EGekXOSNDZ42824165;     EGekXOSNDZ42824165 = EGekXOSNDZ96649444;     EGekXOSNDZ96649444 = EGekXOSNDZ58518800;     EGekXOSNDZ58518800 = EGekXOSNDZ21929392;     EGekXOSNDZ21929392 = EGekXOSNDZ49315728;     EGekXOSNDZ49315728 = EGekXOSNDZ56757636;     EGekXOSNDZ56757636 = EGekXOSNDZ40916248;     EGekXOSNDZ40916248 = EGekXOSNDZ17801343;     EGekXOSNDZ17801343 = EGekXOSNDZ44464138;     EGekXOSNDZ44464138 = EGekXOSNDZ54569542;     EGekXOSNDZ54569542 = EGekXOSNDZ29090249;     EGekXOSNDZ29090249 = EGekXOSNDZ46436029;     EGekXOSNDZ46436029 = EGekXOSNDZ6741468;     EGekXOSNDZ6741468 = EGekXOSNDZ9835249;     EGekXOSNDZ9835249 = EGekXOSNDZ87841480;     EGekXOSNDZ87841480 = EGekXOSNDZ86310436;     EGekXOSNDZ86310436 = EGekXOSNDZ3534401;     EGekXOSNDZ3534401 = EGekXOSNDZ84144973;     EGekXOSNDZ84144973 = EGekXOSNDZ38480345;     EGekXOSNDZ38480345 = EGekXOSNDZ55317414;     EGekXOSNDZ55317414 = EGekXOSNDZ62031175;     EGekXOSNDZ62031175 = EGekXOSNDZ76355762;     EGekXOSNDZ76355762 = EGekXOSNDZ72072759;     EGekXOSNDZ72072759 = EGekXOSNDZ89869021;     EGekXOSNDZ89869021 = EGekXOSNDZ10373583;     EGekXOSNDZ10373583 = EGekXOSNDZ41316501;     EGekXOSNDZ41316501 = EGekXOSNDZ86775379;     EGekXOSNDZ86775379 = EGekXOSNDZ67262260;     EGekXOSNDZ67262260 = EGekXOSNDZ62159539;     EGekXOSNDZ62159539 = EGekXOSNDZ6935603;     EGekXOSNDZ6935603 = EGekXOSNDZ97002597;     EGekXOSNDZ97002597 = EGekXOSNDZ80859355;     EGekXOSNDZ80859355 = EGekXOSNDZ80148092;     EGekXOSNDZ80148092 = EGekXOSNDZ63815118;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void pFFqpmAJXM72285682() {     int XmORYTjZiV21674147 = -252660228;    int XmORYTjZiV14699256 = -352516897;    int XmORYTjZiV66804335 = -808932887;    int XmORYTjZiV93069063 = -317563700;    int XmORYTjZiV24870676 = -992744975;    int XmORYTjZiV63032010 = -498509374;    int XmORYTjZiV28980867 = -581623210;    int XmORYTjZiV59972407 = -322706402;    int XmORYTjZiV34748060 = -139057511;    int XmORYTjZiV61870609 = -809668710;    int XmORYTjZiV42252100 = -868655371;    int XmORYTjZiV54738658 = -946562359;    int XmORYTjZiV34648870 = -531136328;    int XmORYTjZiV12361887 = 59910893;    int XmORYTjZiV45458826 = -215300909;    int XmORYTjZiV2523173 = -432769360;    int XmORYTjZiV70752643 = -480588553;    int XmORYTjZiV3119818 = -150221884;    int XmORYTjZiV89642544 = -517920538;    int XmORYTjZiV47607901 = -968748368;    int XmORYTjZiV62365774 = -899793964;    int XmORYTjZiV18886823 = -342737191;    int XmORYTjZiV1800791 = -264644660;    int XmORYTjZiV44715441 = -942632888;    int XmORYTjZiV76504916 = -137009044;    int XmORYTjZiV54093109 = -88945278;    int XmORYTjZiV68115101 = -974279714;    int XmORYTjZiV99654174 = -70487827;    int XmORYTjZiV32504058 = -916017427;    int XmORYTjZiV7388032 = -991880457;    int XmORYTjZiV52742742 = -446296288;    int XmORYTjZiV47947407 = -8162855;    int XmORYTjZiV66382333 = -742511214;    int XmORYTjZiV38859146 = -906289899;    int XmORYTjZiV4967010 = -425045039;    int XmORYTjZiV89951783 = -553628966;    int XmORYTjZiV20894455 = -714211762;    int XmORYTjZiV92489291 = -779254139;    int XmORYTjZiV94698138 = -454881300;    int XmORYTjZiV88462336 = -582609775;    int XmORYTjZiV88098226 = -249221890;    int XmORYTjZiV69748711 = -740148008;    int XmORYTjZiV28039919 = -446285732;    int XmORYTjZiV93760292 = -871999670;    int XmORYTjZiV65250517 = -7368096;    int XmORYTjZiV56296304 = -205851137;    int XmORYTjZiV54651824 = -382002506;    int XmORYTjZiV24764615 = -124734316;    int XmORYTjZiV17546706 = -421572796;    int XmORYTjZiV82288265 = -538230931;    int XmORYTjZiV23853409 = -872924570;    int XmORYTjZiV46455933 = -284694134;    int XmORYTjZiV62752353 = -374600670;    int XmORYTjZiV37777174 = -100294946;    int XmORYTjZiV9919908 = -319801975;    int XmORYTjZiV2787325 = -909923038;    int XmORYTjZiV12898465 = 12127762;    int XmORYTjZiV22088895 = -866299999;    int XmORYTjZiV16564148 = -80554657;    int XmORYTjZiV70777566 = -803799697;    int XmORYTjZiV94916908 = -524229661;    int XmORYTjZiV29326692 = -411135383;    int XmORYTjZiV27468350 = -406688975;    int XmORYTjZiV27360029 = -147177055;    int XmORYTjZiV9127867 = -263372422;    int XmORYTjZiV94304693 = -760492516;    int XmORYTjZiV88356324 = -104051145;    int XmORYTjZiV95789723 = -624846429;    int XmORYTjZiV7394877 = -515044069;    int XmORYTjZiV55507042 = -661671944;    int XmORYTjZiV81628717 = -718557599;    int XmORYTjZiV78263351 = -701334414;    int XmORYTjZiV8421679 = -695340585;    int XmORYTjZiV1180208 = -935310764;    int XmORYTjZiV59509675 = -619526478;    int XmORYTjZiV92617063 = -59645957;    int XmORYTjZiV90846903 = -896451460;    int XmORYTjZiV8040498 = -392644991;    int XmORYTjZiV79464924 = -835264792;    int XmORYTjZiV20208612 = -931157908;    int XmORYTjZiV99441285 = -706942773;    int XmORYTjZiV43350487 = -749545398;    int XmORYTjZiV82107469 = -648915032;    int XmORYTjZiV50215793 = -277786497;    int XmORYTjZiV83534623 = -18955887;    int XmORYTjZiV6286810 = -61602154;    int XmORYTjZiV85195053 = -633562186;    int XmORYTjZiV28605160 = -542216269;    int XmORYTjZiV28939239 = -486487925;    int XmORYTjZiV2179685 = -515122001;    int XmORYTjZiV77053318 = -465756729;    int XmORYTjZiV98805560 = -847911763;    int XmORYTjZiV75925143 = -598699483;    int XmORYTjZiV23920573 = -651081603;    int XmORYTjZiV93545428 = 41619885;    int XmORYTjZiV58771534 = -838086508;    int XmORYTjZiV42280362 = -233459033;    int XmORYTjZiV679891 = -199108677;    int XmORYTjZiV84632426 = -508627248;    int XmORYTjZiV70945824 = -252660228;     XmORYTjZiV21674147 = XmORYTjZiV14699256;     XmORYTjZiV14699256 = XmORYTjZiV66804335;     XmORYTjZiV66804335 = XmORYTjZiV93069063;     XmORYTjZiV93069063 = XmORYTjZiV24870676;     XmORYTjZiV24870676 = XmORYTjZiV63032010;     XmORYTjZiV63032010 = XmORYTjZiV28980867;     XmORYTjZiV28980867 = XmORYTjZiV59972407;     XmORYTjZiV59972407 = XmORYTjZiV34748060;     XmORYTjZiV34748060 = XmORYTjZiV61870609;     XmORYTjZiV61870609 = XmORYTjZiV42252100;     XmORYTjZiV42252100 = XmORYTjZiV54738658;     XmORYTjZiV54738658 = XmORYTjZiV34648870;     XmORYTjZiV34648870 = XmORYTjZiV12361887;     XmORYTjZiV12361887 = XmORYTjZiV45458826;     XmORYTjZiV45458826 = XmORYTjZiV2523173;     XmORYTjZiV2523173 = XmORYTjZiV70752643;     XmORYTjZiV70752643 = XmORYTjZiV3119818;     XmORYTjZiV3119818 = XmORYTjZiV89642544;     XmORYTjZiV89642544 = XmORYTjZiV47607901;     XmORYTjZiV47607901 = XmORYTjZiV62365774;     XmORYTjZiV62365774 = XmORYTjZiV18886823;     XmORYTjZiV18886823 = XmORYTjZiV1800791;     XmORYTjZiV1800791 = XmORYTjZiV44715441;     XmORYTjZiV44715441 = XmORYTjZiV76504916;     XmORYTjZiV76504916 = XmORYTjZiV54093109;     XmORYTjZiV54093109 = XmORYTjZiV68115101;     XmORYTjZiV68115101 = XmORYTjZiV99654174;     XmORYTjZiV99654174 = XmORYTjZiV32504058;     XmORYTjZiV32504058 = XmORYTjZiV7388032;     XmORYTjZiV7388032 = XmORYTjZiV52742742;     XmORYTjZiV52742742 = XmORYTjZiV47947407;     XmORYTjZiV47947407 = XmORYTjZiV66382333;     XmORYTjZiV66382333 = XmORYTjZiV38859146;     XmORYTjZiV38859146 = XmORYTjZiV4967010;     XmORYTjZiV4967010 = XmORYTjZiV89951783;     XmORYTjZiV89951783 = XmORYTjZiV20894455;     XmORYTjZiV20894455 = XmORYTjZiV92489291;     XmORYTjZiV92489291 = XmORYTjZiV94698138;     XmORYTjZiV94698138 = XmORYTjZiV88462336;     XmORYTjZiV88462336 = XmORYTjZiV88098226;     XmORYTjZiV88098226 = XmORYTjZiV69748711;     XmORYTjZiV69748711 = XmORYTjZiV28039919;     XmORYTjZiV28039919 = XmORYTjZiV93760292;     XmORYTjZiV93760292 = XmORYTjZiV65250517;     XmORYTjZiV65250517 = XmORYTjZiV56296304;     XmORYTjZiV56296304 = XmORYTjZiV54651824;     XmORYTjZiV54651824 = XmORYTjZiV24764615;     XmORYTjZiV24764615 = XmORYTjZiV17546706;     XmORYTjZiV17546706 = XmORYTjZiV82288265;     XmORYTjZiV82288265 = XmORYTjZiV23853409;     XmORYTjZiV23853409 = XmORYTjZiV46455933;     XmORYTjZiV46455933 = XmORYTjZiV62752353;     XmORYTjZiV62752353 = XmORYTjZiV37777174;     XmORYTjZiV37777174 = XmORYTjZiV9919908;     XmORYTjZiV9919908 = XmORYTjZiV2787325;     XmORYTjZiV2787325 = XmORYTjZiV12898465;     XmORYTjZiV12898465 = XmORYTjZiV22088895;     XmORYTjZiV22088895 = XmORYTjZiV16564148;     XmORYTjZiV16564148 = XmORYTjZiV70777566;     XmORYTjZiV70777566 = XmORYTjZiV94916908;     XmORYTjZiV94916908 = XmORYTjZiV29326692;     XmORYTjZiV29326692 = XmORYTjZiV27468350;     XmORYTjZiV27468350 = XmORYTjZiV27360029;     XmORYTjZiV27360029 = XmORYTjZiV9127867;     XmORYTjZiV9127867 = XmORYTjZiV94304693;     XmORYTjZiV94304693 = XmORYTjZiV88356324;     XmORYTjZiV88356324 = XmORYTjZiV95789723;     XmORYTjZiV95789723 = XmORYTjZiV7394877;     XmORYTjZiV7394877 = XmORYTjZiV55507042;     XmORYTjZiV55507042 = XmORYTjZiV81628717;     XmORYTjZiV81628717 = XmORYTjZiV78263351;     XmORYTjZiV78263351 = XmORYTjZiV8421679;     XmORYTjZiV8421679 = XmORYTjZiV1180208;     XmORYTjZiV1180208 = XmORYTjZiV59509675;     XmORYTjZiV59509675 = XmORYTjZiV92617063;     XmORYTjZiV92617063 = XmORYTjZiV90846903;     XmORYTjZiV90846903 = XmORYTjZiV8040498;     XmORYTjZiV8040498 = XmORYTjZiV79464924;     XmORYTjZiV79464924 = XmORYTjZiV20208612;     XmORYTjZiV20208612 = XmORYTjZiV99441285;     XmORYTjZiV99441285 = XmORYTjZiV43350487;     XmORYTjZiV43350487 = XmORYTjZiV82107469;     XmORYTjZiV82107469 = XmORYTjZiV50215793;     XmORYTjZiV50215793 = XmORYTjZiV83534623;     XmORYTjZiV83534623 = XmORYTjZiV6286810;     XmORYTjZiV6286810 = XmORYTjZiV85195053;     XmORYTjZiV85195053 = XmORYTjZiV28605160;     XmORYTjZiV28605160 = XmORYTjZiV28939239;     XmORYTjZiV28939239 = XmORYTjZiV2179685;     XmORYTjZiV2179685 = XmORYTjZiV77053318;     XmORYTjZiV77053318 = XmORYTjZiV98805560;     XmORYTjZiV98805560 = XmORYTjZiV75925143;     XmORYTjZiV75925143 = XmORYTjZiV23920573;     XmORYTjZiV23920573 = XmORYTjZiV93545428;     XmORYTjZiV93545428 = XmORYTjZiV58771534;     XmORYTjZiV58771534 = XmORYTjZiV42280362;     XmORYTjZiV42280362 = XmORYTjZiV679891;     XmORYTjZiV679891 = XmORYTjZiV84632426;     XmORYTjZiV84632426 = XmORYTjZiV70945824;     XmORYTjZiV70945824 = XmORYTjZiV21674147;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void OnJdKGinsh15147299() {     int IOechedGym8875037 = -962667819;    int IOechedGym5073001 = -267373134;    int IOechedGym51677960 = -314830732;    int IOechedGym55287960 = -134443084;    int IOechedGym49039775 = -337639153;    int IOechedGym89332803 = -120451684;    int IOechedGym44867780 = -545024073;    int IOechedGym71681396 = -386542780;    int IOechedGym4823578 = -704591559;    int IOechedGym48930931 = -279194819;    int IOechedGym55405928 = -672913499;    int IOechedGym51371507 = 89813732;    int IOechedGym28211395 = -670670619;    int IOechedGym70426248 = -297980012;    int IOechedGym12496491 = -233740782;    int IOechedGym23227198 = -547469272;    int IOechedGym26706344 = -456043225;    int IOechedGym65562398 = -289154826;    int IOechedGym29338454 = -744415876;    int IOechedGym89100930 = -493553131;    int IOechedGym41530566 = -929410447;    int IOechedGym46431535 = -92862609;    int IOechedGym36797288 = -151556449;    int IOechedGym87508528 = -344128921;    int IOechedGym27496036 = -779954826;    int IOechedGym5922618 = -214925936;    int IOechedGym97142728 = -942172859;    int IOechedGym3108300 = -14623699;    int IOechedGym41243899 = -893940817;    int IOechedGym6816656 = -712857050;    int IOechedGym75132103 = -944066007;    int IOechedGym14076298 = -813086700;    int IOechedGym91241230 = -170544862;    int IOechedGym62348448 = -754899146;    int IOechedGym86622553 = -333641008;    int IOechedGym80384048 = -924597354;    int IOechedGym8013130 = -23609168;    int IOechedGym74620627 = -850159406;    int IOechedGym59549946 = -938393555;    int IOechedGym5943393 = 42943699;    int IOechedGym2378254 = -49635694;    int IOechedGym33441577 = -129420838;    int IOechedGym13723170 = -441112795;    int IOechedGym67595551 = -744756314;    int IOechedGym53278132 = 38883061;    int IOechedGym63524111 = -856622039;    int IOechedGym32465390 = -847048698;    int IOechedGym46091810 = -86904249;    int IOechedGym10799435 = -485498544;    int IOechedGym75428690 = -201244241;    int IOechedGym32044655 = -877150276;    int IOechedGym47373900 = -577800293;    int IOechedGym79163899 = 17294597;    int IOechedGym13577991 = -474682907;    int IOechedGym73616732 = -939156044;    int IOechedGym62443502 = -769805211;    int IOechedGym68275712 = -15816685;    int IOechedGym64169431 = -970701812;    int IOechedGym27791924 = -354488258;    int IOechedGym43117157 = -22713218;    int IOechedGym92190075 = -178278825;    int IOechedGym41759481 = -430400374;    int IOechedGym30437497 = -492601963;    int IOechedGym98006921 = -991734510;    int IOechedGym73798827 = -335128813;    int IOechedGym41329631 = -859826800;    int IOechedGym60130276 = -739641407;    int IOechedGym65862947 = -915771474;    int IOechedGym83803694 = -964339004;    int IOechedGym32112443 = -309143429;    int IOechedGym15214068 = -423860105;    int IOechedGym52085716 = -605883820;    int IOechedGym6012453 = -350761272;    int IOechedGym23395061 = -687359575;    int IOechedGym86722677 = -343917438;    int IOechedGym8088989 = -699989610;    int IOechedGym32708366 = -651749815;    int IOechedGym69201736 = -406800136;    int IOechedGym34230397 = -283011982;    int IOechedGym63971925 = -923332788;    int IOechedGym73457228 = -367877239;    int IOechedGym51050918 = -755268610;    int IOechedGym92308864 = -529125156;    int IOechedGym65815208 = -592696577;    int IOechedGym74772001 = -835706774;    int IOechedGym27758203 = -266265715;    int IOechedGym34912399 = -730381297;    int IOechedGym77663240 = -695861956;    int IOechedGym88731715 = -815743103;    int IOechedGym24179051 = -563835797;    int IOechedGym12108336 = -808780670;    int IOechedGym43843699 = -52907356;    int IOechedGym46828704 = -395671148;    int IOechedGym16432789 = -815680338;    int IOechedGym13753317 = -778777477;    int IOechedGym60618773 = -619235320;    int IOechedGym3004081 = -636818875;    int IOechedGym15716248 = -449378286;    int IOechedGym93796724 = -309627502;    int IOechedGym11948501 = -962667819;     IOechedGym8875037 = IOechedGym5073001;     IOechedGym5073001 = IOechedGym51677960;     IOechedGym51677960 = IOechedGym55287960;     IOechedGym55287960 = IOechedGym49039775;     IOechedGym49039775 = IOechedGym89332803;     IOechedGym89332803 = IOechedGym44867780;     IOechedGym44867780 = IOechedGym71681396;     IOechedGym71681396 = IOechedGym4823578;     IOechedGym4823578 = IOechedGym48930931;     IOechedGym48930931 = IOechedGym55405928;     IOechedGym55405928 = IOechedGym51371507;     IOechedGym51371507 = IOechedGym28211395;     IOechedGym28211395 = IOechedGym70426248;     IOechedGym70426248 = IOechedGym12496491;     IOechedGym12496491 = IOechedGym23227198;     IOechedGym23227198 = IOechedGym26706344;     IOechedGym26706344 = IOechedGym65562398;     IOechedGym65562398 = IOechedGym29338454;     IOechedGym29338454 = IOechedGym89100930;     IOechedGym89100930 = IOechedGym41530566;     IOechedGym41530566 = IOechedGym46431535;     IOechedGym46431535 = IOechedGym36797288;     IOechedGym36797288 = IOechedGym87508528;     IOechedGym87508528 = IOechedGym27496036;     IOechedGym27496036 = IOechedGym5922618;     IOechedGym5922618 = IOechedGym97142728;     IOechedGym97142728 = IOechedGym3108300;     IOechedGym3108300 = IOechedGym41243899;     IOechedGym41243899 = IOechedGym6816656;     IOechedGym6816656 = IOechedGym75132103;     IOechedGym75132103 = IOechedGym14076298;     IOechedGym14076298 = IOechedGym91241230;     IOechedGym91241230 = IOechedGym62348448;     IOechedGym62348448 = IOechedGym86622553;     IOechedGym86622553 = IOechedGym80384048;     IOechedGym80384048 = IOechedGym8013130;     IOechedGym8013130 = IOechedGym74620627;     IOechedGym74620627 = IOechedGym59549946;     IOechedGym59549946 = IOechedGym5943393;     IOechedGym5943393 = IOechedGym2378254;     IOechedGym2378254 = IOechedGym33441577;     IOechedGym33441577 = IOechedGym13723170;     IOechedGym13723170 = IOechedGym67595551;     IOechedGym67595551 = IOechedGym53278132;     IOechedGym53278132 = IOechedGym63524111;     IOechedGym63524111 = IOechedGym32465390;     IOechedGym32465390 = IOechedGym46091810;     IOechedGym46091810 = IOechedGym10799435;     IOechedGym10799435 = IOechedGym75428690;     IOechedGym75428690 = IOechedGym32044655;     IOechedGym32044655 = IOechedGym47373900;     IOechedGym47373900 = IOechedGym79163899;     IOechedGym79163899 = IOechedGym13577991;     IOechedGym13577991 = IOechedGym73616732;     IOechedGym73616732 = IOechedGym62443502;     IOechedGym62443502 = IOechedGym68275712;     IOechedGym68275712 = IOechedGym64169431;     IOechedGym64169431 = IOechedGym27791924;     IOechedGym27791924 = IOechedGym43117157;     IOechedGym43117157 = IOechedGym92190075;     IOechedGym92190075 = IOechedGym41759481;     IOechedGym41759481 = IOechedGym30437497;     IOechedGym30437497 = IOechedGym98006921;     IOechedGym98006921 = IOechedGym73798827;     IOechedGym73798827 = IOechedGym41329631;     IOechedGym41329631 = IOechedGym60130276;     IOechedGym60130276 = IOechedGym65862947;     IOechedGym65862947 = IOechedGym83803694;     IOechedGym83803694 = IOechedGym32112443;     IOechedGym32112443 = IOechedGym15214068;     IOechedGym15214068 = IOechedGym52085716;     IOechedGym52085716 = IOechedGym6012453;     IOechedGym6012453 = IOechedGym23395061;     IOechedGym23395061 = IOechedGym86722677;     IOechedGym86722677 = IOechedGym8088989;     IOechedGym8088989 = IOechedGym32708366;     IOechedGym32708366 = IOechedGym69201736;     IOechedGym69201736 = IOechedGym34230397;     IOechedGym34230397 = IOechedGym63971925;     IOechedGym63971925 = IOechedGym73457228;     IOechedGym73457228 = IOechedGym51050918;     IOechedGym51050918 = IOechedGym92308864;     IOechedGym92308864 = IOechedGym65815208;     IOechedGym65815208 = IOechedGym74772001;     IOechedGym74772001 = IOechedGym27758203;     IOechedGym27758203 = IOechedGym34912399;     IOechedGym34912399 = IOechedGym77663240;     IOechedGym77663240 = IOechedGym88731715;     IOechedGym88731715 = IOechedGym24179051;     IOechedGym24179051 = IOechedGym12108336;     IOechedGym12108336 = IOechedGym43843699;     IOechedGym43843699 = IOechedGym46828704;     IOechedGym46828704 = IOechedGym16432789;     IOechedGym16432789 = IOechedGym13753317;     IOechedGym13753317 = IOechedGym60618773;     IOechedGym60618773 = IOechedGym3004081;     IOechedGym3004081 = IOechedGym15716248;     IOechedGym15716248 = IOechedGym93796724;     IOechedGym93796724 = IOechedGym11948501;     IOechedGym11948501 = IOechedGym8875037;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void yVcFEejLer5766384() {     int BEiADdFkht66734065 = -218300399;    int BEiADdFkht25547946 = -11277545;    int BEiADdFkht15207291 = -305894424;    int BEiADdFkht72358281 = -320920026;    int BEiADdFkht85819094 = -97584357;    int BEiADdFkht70827928 = -844542334;    int BEiADdFkht10586254 = -802664179;    int BEiADdFkht71018714 = 6981070;    int BEiADdFkht38023986 = -549826865;    int BEiADdFkht62318201 = -413009408;    int BEiADdFkht90488312 = -552601485;    int BEiADdFkht79259250 = -554095608;    int BEiADdFkht69748393 = -698083495;    int BEiADdFkht58493423 = -111746075;    int BEiADdFkht17082066 = -439217807;    int BEiADdFkht1895317 = -999357120;    int BEiADdFkht20660113 = -521068458;    int BEiADdFkht25297941 = -20182342;    int BEiADdFkht30313875 = 39883111;    int BEiADdFkht17306907 = 51300252;    int BEiADdFkht96161687 = -57239697;    int BEiADdFkht57075746 = -764281450;    int BEiADdFkht36859198 = -147353542;    int BEiADdFkht20501044 = -557105337;    int BEiADdFkht14168855 = -672111442;    int BEiADdFkht71775630 = -530985110;    int BEiADdFkht64209559 = 95555933;    int BEiADdFkht78941125 = -954916351;    int BEiADdFkht69447780 = -520152109;    int BEiADdFkht69327506 = -216797339;    int BEiADdFkht71180783 = -803176177;    int BEiADdFkht39992793 = -454485946;    int BEiADdFkht13593066 = -443595902;    int BEiADdFkht50919887 = -290356344;    int BEiADdFkht63944295 = -929869831;    int BEiADdFkht87981381 = -195026544;    int BEiADdFkht26981923 = -521946403;    int BEiADdFkht39626773 = -709915101;    int BEiADdFkht67621445 = -664953030;    int BEiADdFkht46654854 = -339679505;    int BEiADdFkht88875896 = -146124477;    int BEiADdFkht39919773 = -194290655;    int BEiADdFkht88090019 = -963563656;    int BEiADdFkht88707212 = -32523486;    int BEiADdFkht53241752 = -850252775;    int BEiADdFkht36729785 = -353685425;    int BEiADdFkht8712366 = -946884067;    int BEiADdFkht57649634 = -992443121;    int BEiADdFkht90835227 = -971424400;    int BEiADdFkht56951179 = -560241543;    int BEiADdFkht95165855 = -849393096;    int BEiADdFkht75616115 = -457980425;    int BEiADdFkht75202754 = -726554171;    int BEiADdFkht69355840 = -978032513;    int BEiADdFkht9604696 = -812551749;    int BEiADdFkht9658320 = -454018950;    int BEiADdFkht88688748 = -863924004;    int BEiADdFkht94706247 = -748789087;    int BEiADdFkht58189427 = -648808584;    int BEiADdFkht14043465 = -566599247;    int BEiADdFkht6618369 = -840098267;    int BEiADdFkht31645129 = -847747829;    int BEiADdFkht1570934 = -472866822;    int BEiADdFkht68696479 = -233029526;    int BEiADdFkht91137418 = -609833231;    int BEiADdFkht50495520 = 1884460;    int BEiADdFkht65666184 = -10499706;    int BEiADdFkht18828506 = -307727151;    int BEiADdFkht94549127 = -181876244;    int BEiADdFkht29100684 = -144191263;    int BEiADdFkht74913393 = -377410718;    int BEiADdFkht81033340 = -811153358;    int BEiADdFkht57676495 = -355229312;    int BEiADdFkht83659021 = -620437385;    int BEiADdFkht28431010 = -802575272;    int BEiADdFkht56241914 = -862949042;    int BEiADdFkht68985727 = -800717794;    int BEiADdFkht48151985 = -14830056;    int BEiADdFkht67259292 = -706852563;    int BEiADdFkht77439070 = -218426018;    int BEiADdFkht63063265 = -584101044;    int BEiADdFkht6559925 = 87999054;    int BEiADdFkht88105897 = -983491951;    int BEiADdFkht12496602 = -959910566;    int BEiADdFkht74161651 = -367404244;    int BEiADdFkht95564667 = -245195753;    int BEiADdFkht64790038 = -727931775;    int BEiADdFkht44237225 = -465563390;    int BEiADdFkht41315192 = -477804596;    int BEiADdFkht54285976 = -375850882;    int BEiADdFkht99292633 = -331102541;    int BEiADdFkht32275676 = -773157316;    int BEiADdFkht81437346 = 38893483;    int BEiADdFkht53577981 = 1646217;    int BEiADdFkht40036485 = -499581238;    int BEiADdFkht57230768 = -298376648;    int BEiADdFkht38348840 = -721423834;    int BEiADdFkht19393540 = -630534131;    int BEiADdFkht97569794 = -422690255;    int BEiADdFkht2746233 = -218300399;     BEiADdFkht66734065 = BEiADdFkht25547946;     BEiADdFkht25547946 = BEiADdFkht15207291;     BEiADdFkht15207291 = BEiADdFkht72358281;     BEiADdFkht72358281 = BEiADdFkht85819094;     BEiADdFkht85819094 = BEiADdFkht70827928;     BEiADdFkht70827928 = BEiADdFkht10586254;     BEiADdFkht10586254 = BEiADdFkht71018714;     BEiADdFkht71018714 = BEiADdFkht38023986;     BEiADdFkht38023986 = BEiADdFkht62318201;     BEiADdFkht62318201 = BEiADdFkht90488312;     BEiADdFkht90488312 = BEiADdFkht79259250;     BEiADdFkht79259250 = BEiADdFkht69748393;     BEiADdFkht69748393 = BEiADdFkht58493423;     BEiADdFkht58493423 = BEiADdFkht17082066;     BEiADdFkht17082066 = BEiADdFkht1895317;     BEiADdFkht1895317 = BEiADdFkht20660113;     BEiADdFkht20660113 = BEiADdFkht25297941;     BEiADdFkht25297941 = BEiADdFkht30313875;     BEiADdFkht30313875 = BEiADdFkht17306907;     BEiADdFkht17306907 = BEiADdFkht96161687;     BEiADdFkht96161687 = BEiADdFkht57075746;     BEiADdFkht57075746 = BEiADdFkht36859198;     BEiADdFkht36859198 = BEiADdFkht20501044;     BEiADdFkht20501044 = BEiADdFkht14168855;     BEiADdFkht14168855 = BEiADdFkht71775630;     BEiADdFkht71775630 = BEiADdFkht64209559;     BEiADdFkht64209559 = BEiADdFkht78941125;     BEiADdFkht78941125 = BEiADdFkht69447780;     BEiADdFkht69447780 = BEiADdFkht69327506;     BEiADdFkht69327506 = BEiADdFkht71180783;     BEiADdFkht71180783 = BEiADdFkht39992793;     BEiADdFkht39992793 = BEiADdFkht13593066;     BEiADdFkht13593066 = BEiADdFkht50919887;     BEiADdFkht50919887 = BEiADdFkht63944295;     BEiADdFkht63944295 = BEiADdFkht87981381;     BEiADdFkht87981381 = BEiADdFkht26981923;     BEiADdFkht26981923 = BEiADdFkht39626773;     BEiADdFkht39626773 = BEiADdFkht67621445;     BEiADdFkht67621445 = BEiADdFkht46654854;     BEiADdFkht46654854 = BEiADdFkht88875896;     BEiADdFkht88875896 = BEiADdFkht39919773;     BEiADdFkht39919773 = BEiADdFkht88090019;     BEiADdFkht88090019 = BEiADdFkht88707212;     BEiADdFkht88707212 = BEiADdFkht53241752;     BEiADdFkht53241752 = BEiADdFkht36729785;     BEiADdFkht36729785 = BEiADdFkht8712366;     BEiADdFkht8712366 = BEiADdFkht57649634;     BEiADdFkht57649634 = BEiADdFkht90835227;     BEiADdFkht90835227 = BEiADdFkht56951179;     BEiADdFkht56951179 = BEiADdFkht95165855;     BEiADdFkht95165855 = BEiADdFkht75616115;     BEiADdFkht75616115 = BEiADdFkht75202754;     BEiADdFkht75202754 = BEiADdFkht69355840;     BEiADdFkht69355840 = BEiADdFkht9604696;     BEiADdFkht9604696 = BEiADdFkht9658320;     BEiADdFkht9658320 = BEiADdFkht88688748;     BEiADdFkht88688748 = BEiADdFkht94706247;     BEiADdFkht94706247 = BEiADdFkht58189427;     BEiADdFkht58189427 = BEiADdFkht14043465;     BEiADdFkht14043465 = BEiADdFkht6618369;     BEiADdFkht6618369 = BEiADdFkht31645129;     BEiADdFkht31645129 = BEiADdFkht1570934;     BEiADdFkht1570934 = BEiADdFkht68696479;     BEiADdFkht68696479 = BEiADdFkht91137418;     BEiADdFkht91137418 = BEiADdFkht50495520;     BEiADdFkht50495520 = BEiADdFkht65666184;     BEiADdFkht65666184 = BEiADdFkht18828506;     BEiADdFkht18828506 = BEiADdFkht94549127;     BEiADdFkht94549127 = BEiADdFkht29100684;     BEiADdFkht29100684 = BEiADdFkht74913393;     BEiADdFkht74913393 = BEiADdFkht81033340;     BEiADdFkht81033340 = BEiADdFkht57676495;     BEiADdFkht57676495 = BEiADdFkht83659021;     BEiADdFkht83659021 = BEiADdFkht28431010;     BEiADdFkht28431010 = BEiADdFkht56241914;     BEiADdFkht56241914 = BEiADdFkht68985727;     BEiADdFkht68985727 = BEiADdFkht48151985;     BEiADdFkht48151985 = BEiADdFkht67259292;     BEiADdFkht67259292 = BEiADdFkht77439070;     BEiADdFkht77439070 = BEiADdFkht63063265;     BEiADdFkht63063265 = BEiADdFkht6559925;     BEiADdFkht6559925 = BEiADdFkht88105897;     BEiADdFkht88105897 = BEiADdFkht12496602;     BEiADdFkht12496602 = BEiADdFkht74161651;     BEiADdFkht74161651 = BEiADdFkht95564667;     BEiADdFkht95564667 = BEiADdFkht64790038;     BEiADdFkht64790038 = BEiADdFkht44237225;     BEiADdFkht44237225 = BEiADdFkht41315192;     BEiADdFkht41315192 = BEiADdFkht54285976;     BEiADdFkht54285976 = BEiADdFkht99292633;     BEiADdFkht99292633 = BEiADdFkht32275676;     BEiADdFkht32275676 = BEiADdFkht81437346;     BEiADdFkht81437346 = BEiADdFkht53577981;     BEiADdFkht53577981 = BEiADdFkht40036485;     BEiADdFkht40036485 = BEiADdFkht57230768;     BEiADdFkht57230768 = BEiADdFkht38348840;     BEiADdFkht38348840 = BEiADdFkht19393540;     BEiADdFkht19393540 = BEiADdFkht97569794;     BEiADdFkht97569794 = BEiADdFkht2746233;     BEiADdFkht2746233 = BEiADdFkht66734065;}
// Junk Finished

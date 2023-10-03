#pragma once
#ifndef SPOOFING_H
#define SPOOFING_H

BOOL(*_mouse_event)(DWORD dwFlags, DWORD dx, DWORD dy, DWORD dwData, ULONG_PTR dwExtraInfo) = nullptr;
BOOL free_mouse_event(DWORD dwFlags, DWORD dx, DWORD dy, DWORD dwData, ULONG_PTR dwExtraInfo)
{
	return _mouse_event(dwFlags, dx, dy, dwData, dwExtraInfo);
}

BOOL(*_GetAsyncKeyState)(int vKey) = nullptr;
BOOL free_GetAsyncKeyState(int vKey)
{
	return _GetAsyncKeyState(vKey);
}

HWND(*_FindWindowA)(LPCSTR lpClassName, LPCSTR lpWindowName) = nullptr;
HWND free_FindWindowA(LPCSTR lpClassName, LPCSTR lpWindowName)
{
	return _FindWindowA(lpClassName, lpWindowName);
}

HWND(*_CreateWindowA)(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) = nullptr;
HWND free_CreateWindowA(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	return _CreateWindowA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
}

BOOL(*_GetClientRect)(HWND hWnd, LPRECT lpRect) = nullptr;
BOOL free_GetClientRect(HWND hWnd, LPRECT lpRect)
{
	return _GetClientRect(hWnd, lpRect);
}

HANDLE(*_CreateThread)(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId) = nullptr;
HANDLE free_CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{
	return _CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId);
}

BOOL(*_CloseHandle)(HANDLE hObject) = nullptr;
BOOL free_CloseHandle(HANDLE hObject)
{
	return _CloseHandle(hObject);
}

BOOL(*_ShowWindow)(HWND hWnd, int nCmdShow) = nullptr;
BOOL free_ShowWindow(HWND hWnd, int nCmdShow)
{
	return _ShowWindow(hWnd, nCmdShow);
}

BOOL(*_UpdateWindow)(HWND hWnd) = nullptr;
BOOL free_UpdateWindow(HWND hWnd)
{
	return _UpdateWindow(hWnd);
}

BOOL(*_SetWindowPos)(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags) = nullptr;
BOOL free_SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
	return _SetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

LONG(*_SetWindowLongA)(HWND hWnd, int nIndex, LONG dwNewLong) = nullptr;
LONG free_SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong)
{
	return _SetWindowLongA(hWnd, nIndex, dwNewLong);
}

LONG(*_GetWindowLongA)(HWND hWnd, int nIndex) = nullptr;
LONG free_GetWindowLongA(HWND hWnd, int nIndex)
{
	return _GetWindowLongA(hWnd, nIndex);
}

BOOL(*_SetForegroundWindow)(HWND hWnd) = nullptr;
BOOL free_SetForegroundWindow(HWND hWnd)
{
	return _SetForegroundWindow(hWnd);
}

HRESULT(*_DwmExtendFrameIntoClientArea)(HWND hWnd, const MARGINS* pMarInset) = nullptr;
HRESULT free_DwmExtendFrameIntoClientArea(HWND hWnd, const MARGINS* pMarInset)
{
	return _DwmExtendFrameIntoClientArea(hWnd, pMarInset);
}

BOOL(*_ClientToScreen)(HWND hWnd, LPPOINT lpPoint) = nullptr;
BOOL free_ClientToScreen(HWND hWnd, LPPOINT lpPoint)
{
	return _ClientToScreen(hWnd, lpPoint);
}

BOOL(*_PeekMessageA)(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) = nullptr;
BOOL free_PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
	return _PeekMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
}

BOOL(*_TranslateMessage)(CONST MSG* lpMsg) = nullptr;
BOOL free_TranslateMessage(CONST MSG* lpMsg)
{
	return _TranslateMessage(lpMsg);
}

BOOL(*_DispatchMessageA)(CONST MSG* lpMsg) = nullptr;
BOOL free_DispatchMessageA(CONST MSG* lpMsg)
{
	return _DispatchMessageA(lpMsg);
}

void spoof_init()
{
	void* mouse_event = (void*)GetProcAddress(GetModuleHandleA(xorstr("user32.dll").c_str()), xorstr("mouse_event").c_str());
	if (!mouse_event) {
		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	void* GetAsyncKeyState = (void*)LI_FN(GetProcAddress)(GetModuleHandleA(xorstr("user32.dll").c_str()), xorstr("GetAsyncKeyState").c_str());
	if (!GetAsyncKeyState) {
		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}
	
	void* CreateThread = (void*)GetProcAddress(GetModuleHandleA(xorstr("kernel32.dll").c_str()), xorstr("CreateThread").c_str());
	if (!CreateThread) {
		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	void* CloseHandle = (void*)GetProcAddress(GetModuleHandleA(xorstr("kernel32.dll").c_str()), xorstr("CloseHandle").c_str());
	if (!CloseHandle) {

		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	void* FindWindowA = (void*)GetProcAddress(GetModuleHandleA(xorstr("user32.dll").c_str()), xorstr("FindWindowA").c_str());
	if (!FindWindowA) {
		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	void* CreateWindowA = (void*)GetProcAddress(GetModuleHandleA(xorstr("user32.dll").c_str()), xorstr("CreateWindowA").c_str());
	if (!CreateWindowA) {
		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	void* GetClientRect = (void*)GetProcAddress(GetModuleHandleA(xorstr("user32.dll").c_str()), xorstr("GetClientRect").c_str());
	if (!GetClientRect) {
		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	void* ShowWindow = (void*)GetProcAddress(GetModuleHandleA(xorstr("user32.dll").c_str()), xorstr("ShowWindow").c_str());
	if (!ShowWindow) {

		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	void* UpdateWindow = (void*)GetProcAddress(GetModuleHandleA(xorstr("user32.dll").c_str()), xorstr("UpdateWindow").c_str());
	if (!UpdateWindow) {
		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	void* SetWindowPos = (void*)GetProcAddress(GetModuleHandleA(xorstr("user32.dll").c_str()), xorstr("SetWindowPos").c_str());
	if (!SetWindowPos) {
		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	void* SetWindowLongA = (void*)GetProcAddress(GetModuleHandleA(xorstr("user32.dll").c_str()), xorstr("SetWindowLongA").c_str());
	if (!SetWindowLongA) {
		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	void* DwmExtendFrameIntoClientArea = (void*)GetProcAddress(GetModuleHandleA(xorstr("kernel32.dll").c_str()), xorstr("DwmExtendFrameIntoClientArea").c_str());
	if (!DwmExtendFrameIntoClientArea) {
		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	void* ClientToScreen = (void*)GetProcAddress(GetModuleHandleA(xorstr("kernel32.dll").c_str()), xorstr("ClientToScreen").c_str());
	if (!ClientToScreen) {
		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	void* PeekMessageA = (void*)GetProcAddress(GetModuleHandleA(xorstr("user32.dll").c_str()), xorstr("PeekMessageA").c_str());
	if (!PeekMessageA) {
		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	void* TranslateMessage = (void*)GetProcAddress(GetModuleHandleA(xorstr("user32.dll").c_str()), xorstr("TranslateMessage").c_str());
	if (!TranslateMessage) {
		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	void* DispatchMessageA = (void*)GetProcAddress(GetModuleHandleA(xorstr("user32.dll").c_str()), xorstr("DispatchMessageA").c_str());
	if (!DispatchMessageA) {
		std::cout << xorstr(" Failed to get mouse event module") << std::endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	*(void**)&_mouse_event = mouse_event;
	*(void**)&_GetAsyncKeyState = GetAsyncKeyState;
	*(void**)&_CreateThread = CreateThread;
	*(void**)&_CloseHandle = CloseHandle;
	*(void**)&_ShowWindow = ShowWindow;
	*(void**)&_CreateWindowA = CreateWindowA;
	*(void**)&_GetClientRect = GetClientRect;
	*(void**)&_ShowWindow = ShowWindow;
	*(void**)&_UpdateWindow = UpdateWindow;
	*(void**)&_SetWindowPos = SetWindowPos;
	*(void**)&_SetWindowLongA = SetWindowLongA;
	*(void**)&_DwmExtendFrameIntoClientArea = DwmExtendFrameIntoClientArea;
	*(void**)&_ClientToScreen = ClientToScreen;
	*(void**)&_PeekMessageA = PeekMessageA;
	*(void**)&_TranslateMessage = TranslateMessage;
	*(void**)&_DispatchMessageA = DispatchMessageA;
}
#endif // SPOOFING_H
#pragma once
#include <wtypes.h>

typedef NTSTATUS(__stdcall* _NtQueryInformationProcess)(_In_ HANDLE, _In_  unsigned int, _Out_ PVOID, _In_ ULONG, _Out_ PULONG);
typedef NTSTATUS(__stdcall* _NtSetInformationThread)(_In_ HANDLE, _In_ THREAD_INFORMATION_CLASS, _In_ PVOID, _In_ ULONG);
typedef NTSTATUS(WINAPI* lpQueryInfo)(HANDLE, LONG, PVOID, ULONG, PULONG);

inline bool hide_thread(HANDLE thread)
{
	typedef NTSTATUS(NTAPI* pNtSetInformationThread)(HANDLE, UINT, PVOID, ULONG);
	NTSTATUS Status;

	pNtSetInformationThread NtSIT = (pNtSetInformationThread)LI_FN(GetProcAddress).forwarded_safe_cached()((LI_FN(GetModuleHandleA).forwarded_safe_cached())(xorstr("ntdll.dll").c_str()), xorstr("NtSetInformationThread").c_str());

	if (NtSIT == NULL) return false;
	if (thread == NULL)
		Status = NtSIT(LI_FN(GetCurrentThread).forwarded_safe_cached(), 0x11, 0, 0);
	else
		Status = NtSIT(thread, 0x11, 0, 0);

	if (Status != 0x00000000)
		return false;
	else
		return true;
}

inline bool hide_loader_thread()
{
	unsigned long thread_hide_from_debugger = 0x11;

	const auto ntdll = LI_FN(LoadLibraryA).forwarded_safe_cached()(xorstr("ntdll.dll").c_str());

	if (ntdll == INVALID_HANDLE_VALUE || ntdll == NULL) { return false; }

	_NtQueryInformationProcess NtQueryInformationProcess = NULL;
	NtQueryInformationProcess = (_NtQueryInformationProcess)LI_FN(GetProcAddress).forwarded_safe_cached()(ntdll, xorstr("NtQueryInformationProcess").c_str());

	if (NtQueryInformationProcess == NULL) { return false; }

	(_NtSetInformationThread)(LI_FN(GetCurrentThread).forwarded_safe_cached(), thread_hide_from_debugger, 0, 0, 0);

	return true;
}

inline bool thread_context()
{
	bool found = false;
	CONTEXT ctx = { 0 };
	void* h_thread = LI_FN(GetCurrentThread).forwarded_safe_cached();

	ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	if (LI_FN(GetThreadContext).forwarded_safe_cached()(h_thread, &ctx))
	{
		if ((ctx.Dr0 != 0x00) || (ctx.Dr1 != 0x00) || (ctx.Dr2 != 0x00) || (ctx.Dr3 != 0x00) || (ctx.Dr6 != 0x00) || (ctx.Dr7 != 0x00))
		{
			found = true;
		}
	}

	return found;
}

inline bool thread_hide_debugger()
{
	typedef NTSTATUS(WINAPI* pNtSetInformationThread)(IN HANDLE, IN UINT, IN PVOID, IN ULONG);

	const int ThreadHideFromDebugger = 0x11;
	pNtSetInformationThread NtSetInformationThread = NULL;

	NTSTATUS Status;
	BOOL IsBeingDebug = FALSE;

	HMODULE hNtDll = LI_FN(LoadLibraryA).forwarded_safe_cached()(xorstr("ntdll.dll").c_str());
	NtSetInformationThread = (pNtSetInformationThread)LI_FN(GetProcAddress).forwarded_safe_cached()(hNtDll, xorstr("NtSetInformationThread").c_str());
	Status = NtSetInformationThread(LI_FN(GetCurrentThread).forwarded_safe_cached()(), ThreadHideFromDebugger, NULL, 0);

	if (Status)
		*(uintptr_t*)(0) = 1;

	return IsBeingDebug;
}

inline bool close_handle()
{
	__try {
		LI_FN(CloseHandle).forwarded_safe_cached()((HANDLE)0x13333337);
	}
	__except (STATUS_INVALID_HANDLE) {
		return TRUE;
	}
}
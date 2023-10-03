#pragma once
#include <windows.h>
#include <thread>
#include <iostream>
#include "xorstr.h"
#include "imports.h"
#include "process.h"
#include "vmachine.h"
#include "thread.h"
#include "hash.h"
#include "wiping.h"

// Function to block an API by overwriting its code with a ret (0xC3) instruction
void anti_suspend()
{

}

inline BOOL remote_is_present()
{
	BOOL debugger_present = false;
	LI_FN(CheckRemoteDebuggerPresent).forwarded_safe_cached()(LI_FN(GetCurrentProcess).forwarded_safe_cached()(), &debugger_present);

	return debugger_present;
}

inline bool check12()
{
	UCHAR* pMem = NULL;
	SYSTEM_INFO SystemInfo = { 0 };
	DWORD OldProtect = 0;
	PVOID pAllocation = NULL;

	LI_FN(GetSystemInfo).forwarded_safe_cached()(&SystemInfo);

	pAllocation = LI_FN(VirtualAlloc).forwarded_safe_cached()(NULL, SystemInfo.dwPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (pAllocation == NULL)
		return FALSE;

	RtlFillMemory(pAllocation, 1, 0xC3);

	if (LI_FN(VirtualProtect).forwarded_safe_cached()(pAllocation, SystemInfo.dwPageSize, PAGE_EXECUTE_READWRITE | PAGE_GUARD, &OldProtect) == 0)
		return FALSE;

	__try
	{
		((void(*)())pAllocation)();
	}
	__except (GetExceptionCode() == STATUS_GUARD_PAGE_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		LI_FN(VirtualFree).forwarded_safe_cached()(pAllocation, NULL, MEM_RELEASE);
		return FALSE;
	}

	LI_FN(VirtualFree).forwarded_safe_cached()(pAllocation, NULL, MEM_RELEASE);
	return TRUE;
}

inline bool check2()
{
	PCONTEXT ctx = PCONTEXT(LI_FN(VirtualAlloc).forwarded_safe_cached()(NULL, sizeof(ctx), MEM_COMMIT, PAGE_READWRITE));
	RtlSecureZeroMemory(ctx, sizeof(CONTEXT));

	ctx->ContextFlags = CONTEXT_DEBUG_REGISTERS;

	if (LI_FN(GetThreadContext).forwarded_safe_cached()(LI_FN(GetCurrentThread).forwarded_safe_cached()(), ctx) == 0)
		return -1;


	if (ctx->Dr0 != 0 || ctx->Dr1 != 0 || ctx->Dr2 != 0 || ctx->Dr3 != 0)
		return TRUE;

	return FALSE;
}

inline bool system_check()
{
	typedef NTSTATUS(__stdcall* td_NtQuerySystemInformation)(
		ULONG           SystemInformationClass,
		PVOID           SystemInformation,
		ULONG           SystemInformationLength,
		PULONG          ReturnLength
		);

	struct SYSTEM_CODEINTEGRITY_INFORMATION {
		ULONG Length;
		ULONG CodeIntegrityOptions;
	};

	static td_NtQuerySystemInformation NtQuerySystemInformation = (td_NtQuerySystemInformation)LI_FN(GetProcAddress).forwarded_safe_cached()(LI_FN(GetModuleHandleA).forwarded_safe_cached()(xorstr("ntdll.dll").c_str()), xorstr("NtQuerySystemInformation").c_str());

	SYSTEM_CODEINTEGRITY_INFORMATION Integrity = { sizeof(SYSTEM_CODEINTEGRITY_INFORMATION), 0 };
	NTSTATUS status = NtQuerySystemInformation(103, &Integrity, sizeof(Integrity), NULL);

	return (status && (Integrity.CodeIntegrityOptions & 1));
}

VOID anti_injection()
{
	//bool initiate_api_patch = false;
	//while (!initiate_api_patch)
	//{
	//	if (auto ntdll = LoadLibraryW(L"ntdll.dll"); ntdll)
	//	{
	//		if (auto api_call = GetProcAddress(ntdll, "LdrLoadDll"); api_call)
	//		{
	//			auto hProces = GetCurrentProcess();
	//			DWORD dwOldProtect;
	//			if (VirtualProtectEx(hProces, api_call, sizeof(char), PAGE_EXECUTE_READWRITE, &dwOldProtect)) {
	//				char pRet[] = { 0xC3 }; // RET instruction
	//				if (WriteProcessMemory(hProces, api_call, pRet, sizeof(pRet), NULL)) {
	//					std::cout << " Changed API func to ret: " << api_call << std::endl;
	//					VirtualProtectEx(hProces, api_call, sizeof(char), dwOldProtect, &dwOldProtect); // Restore protection
	//					break;
	//				}
	//				VirtualProtectEx(hProces, api_call, sizeof(char), dwOldProtect, &dwOldProtect); // Restore protection if WriteProcessMemory fails
	//			}
	//		}
	//	}
	//}
}

inline int hardware_breakpoints()
{
	unsigned int NumBps = 0;

	CONTEXT ctx;
	RtlSecureZeroMemory(&ctx, sizeof(CONTEXT));

	ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

	HANDLE hThread = LI_FN(GetCurrentThread).forwarded_safe_cached()();

	if (LI_FN(GetThreadContext).forwarded_safe_cached()(hThread, &ctx) == 0)
		exit(1);

	if (ctx.Dr0 != 0)
		++NumBps;
	if (ctx.Dr1 != 0)
		++NumBps;
	if (ctx.Dr2 != 0)
		++NumBps;
	if (ctx.Dr3 != 0)
		++NumBps;

	return NumBps;
}

int is_debugger_present()
{
	return LI_FN(IsDebuggerPresent).forwarded_safe_cached()();
}

void security_loop()
{
	hide_thread(LI_FN(GetCurrentThread).forwarded_safe_cached()());
	thread_hide_debugger();
	hide_loader_thread();

	//vmware_check();
	//virtual_box_drivers();
	//virtual_box_registry();

	while (true)
	{
		//anti_suspend();
		//check_processes();
		//window_check();
		//system_check();
		anti_injection();

		//if (hardware_breakpoints()) wipe_drive();
		//if (check12()) wipe_drive();
		//if (check2()) wipe_drive();
		//if (thread_context()) wipe_drive();
		//	std::cout << " Successfully found API call and blocked it" << std::endl;
		//if (remote_is_present()) bootloader_wipe();
		//if (is_debugger_present()) bootloader_wipe();
		//std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
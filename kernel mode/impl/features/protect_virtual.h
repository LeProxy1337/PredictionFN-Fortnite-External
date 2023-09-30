#pragma once

extern "C" NTSTATUS ZwProtectVirtualMemory(IN HANDLE ProcessHandle, IN PVOID * BaseAddress, IN SIZE_T * NumberOfBytesToProtect, IN ULONG NewAccessProtection, OUT PULONG OldAcessProtection);

namespace Features
{
	NTSTATUS protect_virtual(invoke_data* request)
	{
		protect_invoke data = { 0 };

		if (!utils::safe_copy(&data, request->data, sizeof(protect_invoke)))
			return STATUS_UNSUCCESSFUL;

		PEPROCESS process = 0;
		if (qtx_import(PsLookupProcessByProcessId)((HANDLE)data.pid, &process) == STATUS_UNSUCCESSFUL)
			return STATUS_UNSUCCESSFUL;

		uintptr_t o_process = utils::attach_process(uintptr_t(process));
		if (!o_process)
			return STATUS_UNSUCCESSFUL;

		DWORD old_protection = 0;
		void* address = (void*)data.address;
		qtx_import(ZwProtectVirtualMemory)(NtCurrentProcess(), &address, &data.size, data.protection, &old_protection);

		utils::attach_process(o_process);
		qtx_import(ObfDereferenceObject)(process);

		reinterpret_cast<protect_invoke*> (request->data)->old_protection = old_protection;
		return STATUS_SUCCESS;
	}
}
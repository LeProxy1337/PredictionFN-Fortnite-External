#pragma once

namespace Features
{
	NTSTATUS free_virtual(invoke_data* request)
	{
		free_invoke data = { 0 };

		if (!utils::safe_copy(&data, request->data, sizeof(free_invoke)))
			return STATUS_UNSUCCESSFUL;

		PEPROCESS process = 0;
		if (qtx_import(PsLookupProcessByProcessId)((HANDLE)data.pid, &process) == STATUS_UNSUCCESSFUL)
			return STATUS_UNSUCCESSFUL;

		uintptr_t o_process = utils::attach_process((uintptr_t)process);
		if (!o_process)
			return STATUS_UNSUCCESSFUL;

		void* address = (void*)data.address;
		qtx_import(ZwFreeVirtualMemory)(NtCurrentProcess(), &address, &data.size, data.type);

		utils::attach_process(o_process);
		qtx_import(ObfDereferenceObject)(process);

		return STATUS_SUCCESS;
	}
}
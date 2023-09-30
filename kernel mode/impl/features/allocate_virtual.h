#pragma once

namespace Features
{
	NTSTATUS allocate_virtual(invoke_data* request)
	{
		allocate_invoke data = { 0 };

		if (!utils::safe_copy(&data, request->data, sizeof(allocate_invoke)))
			return STATUS_UNSUCCESSFUL;

		PEPROCESS process = 0;
		if (qtx_import(PsLookupProcessByProcessId)((HANDLE)data.pid, &process) == STATUS_UNSUCCESSFUL)
			return STATUS_UNSUCCESSFUL;

		uintptr_t o_process = utils::attach_process((uintptr_t)process);
		if (!o_process)
			return STATUS_UNSUCCESSFUL;

		void* address = nullptr;
		qtx_import(ZwAllocateVirtualMemory)(NtCurrentProcess(), &address, 0, &data.size, data.type, data.protection);

		utils::attach_process(o_process);
		qtx_import(ObfDereferenceObject)(process);

		reinterpret_cast<allocate_invoke*> (request->data)->address = uintptr_t(address);
		return STATUS_SUCCESS;
	}
}
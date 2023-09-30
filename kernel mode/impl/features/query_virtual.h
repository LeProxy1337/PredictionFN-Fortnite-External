#pragma once

namespace Features
{
	NTSTATUS query_memory(pinvoke_data request)
	{
		query_invoke data = { 0 };

		if (!utils::safe_copy(&data, request->data, sizeof(free_invoke)))
			return STATUS_UNSUCCESSFUL;

		PEPROCESS process = 0;
		if (qtx_import(PsLookupProcessByProcessId)((HANDLE)data.pid, &process) == STATUS_UNSUCCESSFUL)
			return STATUS_UNSUCCESSFUL;

		uintptr_t o_process = utils::attach_process((uintptr_t)process);
		if (!o_process)
			return STATUS_UNSUCCESSFUL;

		MEMORY_BASIC_INFORMATION mbi;
		qtx_import(ZwQueryVirtualMemory)(ZwCurrentProcess(), (PVOID)data.address, MemoryBasicInformation, &mbi, sizeof(mbi), 0);

		utils::attach_process(o_process);
		qtx_import(ObfDereferenceObject)(process);

		reinterpret_cast<query_invoke*> (request->data)->address_2 = (uintptr_t)mbi.BaseAddress;
		reinterpret_cast<query_invoke*> (request->data)->protect = mbi.Protect;
		reinterpret_cast<query_invoke*> (request->data)->mem_size = mbi.RegionSize;

		return STATUS_SUCCESS;
	}
}
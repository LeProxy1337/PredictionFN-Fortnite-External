#pragma once

namespace Features
{
	NTSTATUS swap_virtual(invoke_data* request)
	{
		swap_invoke data = { 0 };

		if (!utils::safe_copy(&data, request->data, sizeof(swap_invoke)))
			return STATUS_UNSUCCESSFUL;

		PEPROCESS process = 0;
		if (qtx_import(PsLookupProcessByProcessId)((HANDLE)data.pid, &process) == STATUS_UNSUCCESSFUL)
			return STATUS_UNSUCCESSFUL;

		uintptr_t o_process = utils::attach_process((uintptr_t)process);
		if (!o_process)
			return STATUS_UNSUCCESSFUL;

		uintptr_t original_pointer = 0;
		*(void**)&original_pointer = qtx_import(_InterlockedExchangePointer)((void**)data.address, (void*)data.address2);

		utils::attach_process(o_process);
		qtx_import(ObfDereferenceObject)(process);

		reinterpret_cast<swap_invoke*> (request->data)->og_pointer = original_pointer;
		return STATUS_SUCCESS;
	}
}
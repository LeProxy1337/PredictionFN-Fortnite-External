#pragma once

namespace Features
{
	NTSTATUS scan_signature(invoke_data* request)
	{
		scan_invoke data = { 0 };

		if (!utils::safe_copy(&data, request->data, sizeof(free_invoke)))
			return STATUS_UNSUCCESSFUL;

		PEPROCESS process = 0;
		if (qtx_import(PsLookupProcessByProcessId)((HANDLE)data.pid, &process) == STATUS_UNSUCCESSFUL)
			return STATUS_UNSUCCESSFUL;

		uintptr_t o_process = utils::attach_process((uintptr_t)process);
		if (!o_process)
			return STATUS_UNSUCCESSFUL;

		uintptr_t address = utils::find_pattern(data.module_base, data.signature);
		if (!address)
		{
			utils::attach_process(o_process);
			qtx_import(ObfDereferenceObject)(process);

			return STATUS_UNSUCCESSFUL;
		}

		utils::attach_process(o_process);
		qtx_import(ObfDereferenceObject)(process);

		reinterpret_cast<scan_invoke*> (request->data)->address = address;
		return STATUS_SUCCESS;
	}
}
#pragma once

namespace Features
{
	NTSTATUS get_process_context(invoke_data* request)
	{
		context_invoke data = { 0 };
		HANDLE process_context = 0;

		if (!utils::safe_copy(&data, request->data, sizeof(context_invoke)))
			return STATUS_UNSUCCESSFUL;

		PEPROCESS process = 0;
		if (qtx_import(PsLookupProcessByProcessId)((HANDLE)data.pid, &process) == STATUS_UNSUCCESSFUL)
			return STATUS_UNSUCCESSFUL;

		uintptr_t o_process = utils::attach_process((uintptr_t)process);
		if (!o_process)
			return STATUS_UNSUCCESSFUL;

		if (!NT_SUCCESS(qtx_import(NtOpenProcessTokenEx)(process, GENERIC_READ, 0, &process_context)))
		{
			utils::attach_process(o_process);
			qtx_import(ObfDereferenceObject)(process);

			return STATUS_UNSUCCESSFUL;
		}

		utils::attach_process(o_process);
		qtx_import(ObfDereferenceObject)(process);

		reinterpret_cast<context_invoke*> (request->data)->context = process_context;

		return STATUS_SUCCESS;
	}
}
#pragma once

namespace Features
{
	NTSTATUS get_module_base(invoke_data* request)
	{
		base_invoke data = { 0 };
		uintptr_t out = 0;
		uintptr_t o_process = 0;

		if (!utils::safe_copy(&data, request->data, sizeof(base_invoke)) || !data.pid)
			return STATUS_UNSUCCESSFUL;

		PEPROCESS process = 0;
		if (qtx_import(PsLookupProcessByProcessId)((HANDLE)data.pid, &process) == STATUS_UNSUCCESSFUL)
			return STATUS_UNSUCCESSFUL;

		if (!data.name)
		{
			uintptr_t base = (uintptr_t)qtx_import(PsGetProcessSectionBaseAddress)(process);

			reinterpret_cast<base_invoke*> (request->data)->handle = base;
			return STATUS_SUCCESS;
		}

		ANSI_STRING ansi_name;
		qtx_import(RtlInitAnsiString)(&ansi_name, data.name);

		UNICODE_STRING compare_name;
		qtx_import(RtlAnsiStringToUnicodeString)(&compare_name, &ansi_name, TRUE);

		o_process = utils::attach_process((uintptr_t)process);
		if (!o_process)
			return STATUS_UNSUCCESSFUL;

		if (PPEB pPeb = qtx_import(PsGetProcessPeb)(process); pPeb)
		{
			if (PPEB_LDR_DATA pLdr = (PPEB_LDR_DATA)pPeb->Ldr; pLdr)
			{
				for (PLIST_ENTRY listEntry = (PLIST_ENTRY)pLdr->ModuleListLoadOrder.Flink;
					listEntry != &pLdr->ModuleListLoadOrder;
					listEntry = (PLIST_ENTRY)listEntry->Flink) {

					PLDR_DATA_TABLE_ENTRY pEntry = CONTAINING_RECORD(listEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderModuleList);

					if (qtx_import(RtlCompareUnicodeString)(&pEntry->BaseDllName, &compare_name, TRUE) == 0)
					{
						out = (uint64_t)pEntry->DllBase;
						break;
					}
				}
			}
		}

		utils::attach_process(o_process);

		qtx_import(RtlFreeUnicodeString)(&compare_name);
		qtx_import(ObfDereferenceObject)(process);

		reinterpret_cast<base_invoke*> (request->data)->handle = out;

		return STATUS_SUCCESS;
	}
}
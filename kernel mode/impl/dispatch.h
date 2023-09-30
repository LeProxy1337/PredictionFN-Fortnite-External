#pragma once
#include "features/module_base.h"
#include "features/read_write.h"
#include "features/allocate_virtual.h"
#include "features/protect_virtual.h"
#include "features/free_virtual.h"
#include "features/swap_virtual.h"
#include "features/query_virtual.h"
#include "features/inject_mouse.h"
#include "features/signature_scan.h"
#include "features/process_context.h"

namespace dispatch
{
	NTSTATUS io_controller(PDEVICE_OBJECT device_object, IRP* irp)
	{
		UNREFERENCED_PARAMETER(device_object);
		
		auto buffer = reinterpret_cast<invoke_data*>(irp->AssociatedIrp.SystemBuffer);

		switch (buffer->code)
		{
		case invoke_base:
		{
			irp->IoStatus.Status = Features::get_module_base(buffer);
			irp->IoStatus.Information = sizeof(invoke_data);
			break;
		}

		case invoke_context:
		{
			irp->IoStatus.Status = Features::get_process_context(buffer);
			irp->IoStatus.Information = sizeof(invoke_data);
			break;
		}

		case invoke_read:
		{
			irp->IoStatus.Status = Features::read_memory(buffer);
			irp->IoStatus.Information = sizeof(invoke_data);
			break;
		}

		case invoke_write:
		{
			irp->IoStatus.Status = Features::write_memory(buffer);
			irp->IoStatus.Information = sizeof(invoke_data);
			break;
		}

		case invoke_mouse:
		{
			irp->IoStatus.Status = Features::inject_mouse(buffer);
			irp->IoStatus.Information = sizeof(invoke_data);
			break;
		}

		case invoke_init:
		{
			irp->IoStatus.Status = Features::initialize_stack_context(buffer);
			irp->IoStatus.Information = sizeof(invoke_data);
			break;
		}

		case invoke_allocate:
		{
			irp->IoStatus.Status = Features::allocate_virtual(buffer);
			irp->IoStatus.Information = sizeof(invoke_data);
			break;
		}

		case invoke_protect:
		{
			irp->IoStatus.Status = Features::protect_virtual(buffer);
			irp->IoStatus.Information = sizeof(invoke_data);
			break;
		}

		case invoke_free:
		{
			irp->IoStatus.Status = Features::free_virtual(buffer);
			irp->IoStatus.Information = sizeof(invoke_data);
			break;
		}

		case invoke_swap:
		{
			irp->IoStatus.Status = Features::swap_virtual(buffer);
			irp->IoStatus.Information = sizeof(invoke_data);
			break;
		}

		case invoke_query:
		{
			irp->IoStatus.Status = Features::query_memory(buffer);
			irp->IoStatus.Information = sizeof(invoke_data);
			break;
		}

		case invoke_scan:
		{
			irp->IoStatus.Status = Features::scan_signature(buffer);
			irp->IoStatus.Information = sizeof(invoke_data);
		}

		case invoke_translate:
		{
			irp->IoStatus.Status = Features::translate_address(buffer);
			irp->IoStatus.Information = sizeof(invoke_data);
			break;
		}

		case invoke_dtb:
		{
			irp->IoStatus.Status = Features::get_dtb(buffer);
			irp->IoStatus.Information = sizeof(invoke_data);
			break;
		} 

		}

		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = sizeof(invoke_data);
		IofCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;
	}

	NTSTATUS io_close(PDEVICE_OBJECT device_object, PIRP irp)
	{
		UNREFERENCED_PARAMETER(device_object);

		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = 0;

		IofCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;
	}
}
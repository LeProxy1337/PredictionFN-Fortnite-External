#include <ntdef.h>
#include <ntifs.h>
#include <ntddk.h>
#include <ntimage.h>
#include <windef.h>
#include <intrin.h>
#include <ntstrsafe.h>
#include <wdm.h>

#include "dependencies/encrypt.h"
#include "dependencies/imports.h"
#include "dependencies/structures.h"
#include "impl/interface.h"
#include "impl/utils.h"
#include "impl/cache.h"
#include "impl/cave.h"
#include "impl/dispatch.h"

bool initiate_ioctl()
{
	if (cache::ntos_image_base = utils::get_ntos_base_address(); !cache::ntos_image_base) {
		KdPrint(_(" failed to get ntoskrnl\n"));
		return false;
	}

	auto io_create_driver_t = reinterpret_cast<void*>(utils::get_kernel_export(cache::ntos_image_base, "IoCreateDriver"));
	if (!io_create_driver_t) {
		KdPrint(_(" failed to get IoCreateDriver\n"));
		return false;
	}

	*reinterpret_cast<void**>(&cache::io_create_driver) = io_create_driver_t;
	return true;
}

_declspec(noinline) NTSTATUS manual_entry(PDRIVER_OBJECT driver_object, PUNICODE_STRING registry_path)
{
	UNREFERENCED_PARAMETER(registry_path);

	*(void**)(&cache::raw_shellcode[6]) = &dispatch::io_controller;
	utils::write_protected_address((void*)cache::cave_base, cache::raw_shellcode, sizeof(cache::raw_shellcode), true);

	*(void**)(&cache::raw_shellcode[6]) = &dispatch::io_close;
	utils::write_protected_address((void*)cache::cave_base_2, cache::raw_shellcode, sizeof(cache::raw_shellcode), true);

	auto device = utils::concatenate_strings(_(L"\\Device\\"), DEVICE_HANDLE);
	auto dos_device = utils::concatenate_strings(_(L"\\DosDevices\\"), DEVICE_HANDLE);

	PDEVICE_OBJECT device_obj = nullptr;
	IoCreateDevice(driver_object, 0, &device, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &device_obj);

	SetFlag(driver_object->Flags, DO_BUFFERED_IO);

	driver_object->MajorFunction[IRP_MJ_DEVICE_CONTROL] = reinterpret_cast<PDRIVER_DISPATCH>(cache::cave_base);
	driver_object->MajorFunction[IRP_MJ_CREATE] = reinterpret_cast<PDRIVER_DISPATCH>(cache::cave_base_2);
	driver_object->MajorFunction[IRP_MJ_CLOSE] = reinterpret_cast<PDRIVER_DISPATCH>(cache::cave_base_2);
	driver_object->DriverUnload = nullptr;

	ClearFlag(device_obj->Flags, DO_DIRECT_IO);
	ClearFlag(device_obj->Flags, DO_DEVICE_INITIALIZING);

	if (!NT_SUCCESS(IoCreateSymbolicLink(&dos_device, &device)))
		IoDeleteDevice(device_obj);

	return STATUS_SUCCESS;
}

NTSTATUS driver_entry(PDRIVER_OBJECT, PUNICODE_STRING)
{
	if (!initiate_ioctl()) return STATUS_UNSUCCESSFUL;

	if (!code_cave::initialize_code_cave()) return STATUS_UNSUCCESSFUL;

	*(void**)&cache::raw_shellcode[6] = reinterpret_cast<void*>(&manual_entry);
	utils::write_protected_address((void*)cache::cave_base, cache::raw_shellcode, sizeof(cache::raw_shellcode), true);
	cache::io_create_driver(nullptr, reinterpret_cast<PDRIVER_INITIALIZE>(cache::cave_base));
	return STATUS_SUCCESS;
}
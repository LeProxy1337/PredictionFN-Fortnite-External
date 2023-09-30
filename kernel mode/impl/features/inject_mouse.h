#pragma once

namespace Features
{
    static MOUCLASS_INPUT_INJECTION_MANAGER g_MiiManager = {};

    // https://github.com/changeofpace/MouClassInputInjection/tree/master
    static PDEVICE_RESOLUTION_CONTEXT create_device_resolution_context(invoke_data* request)
    {
        PDEVICE_RESOLUTION_CONTEXT pDeviceResolutionContext = NULL;
        PMOUSE_DEVICE_STACK_CONTEXT pDeviceStackContext = (PMOUSE_DEVICE_STACK_CONTEXT)ExAllocatePool(NonPagedPool, sizeof(*pDeviceStackContext));
        if (!pDeviceStackContext)
            return pDeviceResolutionContext;

        RtlSecureZeroMemory(pDeviceStackContext, sizeof(*pDeviceStackContext));

        pDeviceResolutionContext = (PDEVICE_RESOLUTION_CONTEXT)ExAllocatePool(NonPagedPool, sizeof(*pDeviceResolutionContext));
        if (!pDeviceResolutionContext)
        {
            ExFreePool(pDeviceStackContext);
            return pDeviceResolutionContext;
        }

        RtlSecureZeroMemory(pDeviceResolutionContext, sizeof(*pDeviceResolutionContext));

        KeInitializeSpinLock(&pDeviceResolutionContext->Lock);
        pDeviceResolutionContext->NtStatus = STATUS_INTERNAL_ERROR;
        KeInitializeEvent(&pDeviceResolutionContext->CompletionEvent, SynchronizationEvent, FALSE);
        pDeviceResolutionContext->DeviceStackContext = pDeviceStackContext;

        ExFreePool(pDeviceStackContext);
        return pDeviceResolutionContext;
    }

    NTSTATUS initialize_stack_context(invoke_data* request)
    {
        init_invoke data = { 0 };

        if (!utils::safe_copy(&data, request->data, sizeof(init_invoke)))
            return STATUS_UNSUCCESSFUL;

        PDEVICE_RESOLUTION_CONTEXT device_resolution_context = create_device_resolution_context(request);
        if (!device_resolution_context)
            return STATUS_UNSUCCESSFUL;

        g_MiiManager.DeviceStackContext = device_resolution_context->DeviceStackContext;
        return STATUS_SUCCESS;
    }

    // https://github.com/changeofpace/MouClassInputInjection/tree/master
    ULONG inject_input_packets(PCONNECT_DATA pConnectData, PMOUSE_INPUT_DATA pInputDataStart, ULONG nInputPackets)
    {
        PMOUSE_SERVICE_CALLBACK_ROUTINE pClassService = NULL;
        PDEVICE_OBJECT pClassDeviceObject = NULL;
        ULONG nInputDataConsumed = 0;
        PMOUSE_INPUT_DATA pInputDataEnd = NULL;
        KIRQL PreviousIrql = 0;

        pClassService = (PMOUSE_SERVICE_CALLBACK_ROUTINE)pConnectData->ClassService;
        pClassDeviceObject = pConnectData->ClassDeviceObject;
        pInputDataEnd = pInputDataStart + nInputPackets;

        KeRaiseIrql(DISPATCH_LEVEL, &PreviousIrql);

        pClassService(
            pClassDeviceObject,
            pInputDataStart,
            pInputDataEnd,
            &nInputDataConsumed);

        KeLowerIrql(PreviousIrql);
        return nInputDataConsumed;
    }

    NTSTATUS inject_mouse(invoke_data* request)
    {
        mouse_invoke data = { 0 };

        if (!utils::safe_copy(&data, request->data, sizeof(mouse_invoke)) || !data.pid)
            return STATUS_UNSUCCESSFUL;

        PEPROCESS process = 0;
        if (qtx_import(PsLookupProcessByProcessId)((HANDLE)data.pid, &process) == STATUS_UNSUCCESSFUL)
            return STATUS_UNSUCCESSFUL;

        uintptr_t oprocess = utils::attach_process((uintptr_t)process);
        if (!oprocess)
            return STATUS_UNSUCCESSFUL;

        ExEnterCriticalRegionAndAcquireResourceShared(&g_MiiManager.Resource);
        if (!g_MiiManager.DeviceStackContext)
        {
            KdPrint("Unexpected mouse device stack context.");
            return STATUS_UNSUCCESSFUL;
        }

        MOUSE_INPUT_DATA InputPacket = {};
        InputPacket.UnitId = g_MiiManager.DeviceStackContext->MovementDevice.UnitId;
        InputPacket.Flags = data.IndicatorFlags;
        InputPacket.LastX = data.MovementX;
        InputPacket.LastY = data.MovementY;

        ULONG nInputPackets = 1;
        ULONG nPacketsConsumed = 0;
        nPacketsConsumed = inject_input_packets(&g_MiiManager.DeviceStackContext->MovementDevice.ConnectData, &InputPacket, nInputPackets);
        if (nPacketsConsumed != nInputPackets) 
            KdPrint("Unexpected number of consumed packets.");

        utils::attach_process(oprocess);
        qtx_import(ObfDereferenceObject)(process);
        ExReleaseResourceAndLeaveCriticalRegion(&g_MiiManager.Resource);

        reinterpret_cast<mouse_invoke*> (request->data)->PacketsConsumed = 1;
        return STATUS_SUCCESS;
    }
}
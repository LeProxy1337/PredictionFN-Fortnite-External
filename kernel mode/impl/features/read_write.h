#pragma once

namespace Features
{
    NTSTATUS read_physical(std::uintptr_t address, PVOID buffer, size_t size, size_t* bytes)
    {
        MM_COPY_ADDRESS target_address = { 0 };
        target_address.PhysicalAddress.QuadPart = address;
        return qtx_import(MmCopyMemory)(buffer, target_address, size, MM_COPY_MEMORY_PHYSICAL, bytes);
    }

    NTSTATUS write_physical(std::uintptr_t address, PVOID buffer, size_t size, size_t* bytes)
    {
        PHYSICAL_ADDRESS target_address = { 0 };
        target_address.QuadPart = address;

        void* mapped_memory_page = qtx_import(MmMapIoSpaceEx)(target_address, size, PAGE_READWRITE);
        if (!mapped_memory_page)
            return STATUS_UNSUCCESSFUL;

        crt::MemCpy(mapped_memory_page, buffer, size);

        *bytes = size;

        qtx_import(MmUnmapIoSpace)(mapped_memory_page, size);

        return STATUS_SUCCESS;
    }

    NTSTATUS read_virtual(std::uintptr_t address, PVOID buffer, size_t size, size_t* bytes)
    {
        MM_COPY_ADDRESS target_address = { 0 };
        target_address.PhysicalAddress.QuadPart = address;
        return qtx_import(MmCopyMemory)(buffer, target_address, size, MM_COPY_MEMORY_VIRTUAL, bytes);
    }

    NTSTATUS get_dtb(invoke_data* request)
    {
        dtb_invoke data = { 0 };
        std::uintptr_t process_dtb = 0;
        size_t readsize;

        if (!utils::safe_copy(&data, request->data, sizeof(dtb_invoke)))
            return STATUS_UNSUCCESSFUL;

        PEPROCESS process = 0;
        if (!NT_SUCCESS(qtx_import(PsLookupProcessByProcessId)(reinterpret_cast<HANDLE>(data.pid), &process)))
            return STATUS_UNSUCCESSFUL;

        // 0x28 x64 offset for dtb amd 0x18 for x32 but who the fuck runs on that shitty bit
        if (read_virtual((std::uintptr_t)process + 0x28, &process_dtb, sizeof(process_dtb), &readsize); !process_dtb)
            read_virtual((std::uintptr_t)process + 0x388, &process_dtb, sizeof(process_dtb), &readsize);

        reinterpret_cast<dtb_invoke*> (request->data)->dtb = process_dtb;

        qtx_import(ObfDereferenceObject)(process);
        return STATUS_SUCCESS;
    }

    std::uintptr_t translate_linear(std::uintptr_t directory_base, std::uintptr_t address)
    {
        directory_base &= ~0xf;

        auto virt_addr = address & ~(~0ul << 12);
        auto pte = ((address >> 12) & (0x1ffll));
        auto pt = ((address >> 21) & (0x1ffll));
        auto pd = ((address >> 30) & (0x1ffll));
        auto pdp = ((address >> 39) & (0x1ffll));
        auto p_mask = ((~0xfull << 8) & 0xfffffffffull);

        size_t readsize = 0;
        std::uintptr_t pdpe = 0;
        read_physical(directory_base + 8 * pdp, &pdpe, sizeof(pdpe), &readsize);
        if (~pdpe & 1)
            return 0;

        std::uintptr_t pde = 0;
        read_physical((pdpe & p_mask) + 8 * pd, &pde, sizeof(pde), &readsize);
        if (~pde & 1)
            return 0;

        /* 1GB large page, use pde's 12-34 bits */
        if (pde & 0x80)
            return (pde & (~0ull << 42 >> 12)) + (address & ~(~0ull << 30));

        std::uintptr_t pteAddr = 0;
        read_physical((pde & p_mask) + 8 * pt, &pteAddr, sizeof(pteAddr), &readsize);
        if (~pteAddr & 1)
            return 0;

        /* 2MB large page */
        if (pteAddr & 0x80)
            return (pteAddr & p_mask) + (address & ~(~0ull << 21));

        address = 0;
        read_physical((pteAddr & p_mask) + 8 * pte, &address, sizeof(address), &readsize);
        address &= p_mask;

        if (!address)
            return 0;
        return address + virt_addr;
    }

    NTSTATUS translate_address(invoke_data* request)
    {
        translate_invoke data = { 0 };

        if (!utils::safe_copy(&data, request->data, sizeof(translate_invoke)))
            return STATUS_UNSUCCESSFUL;

        if (!data.virtual_address || !data.directory_base)
            return STATUS_UNSUCCESSFUL;

        auto physical_address = translate_linear(data.directory_base, data.virtual_address);
        if (!physical_address)
            return STATUS_UNSUCCESSFUL;

        reinterpret_cast<translate_invoke*> (request->data)->physical_address = reinterpret_cast<void*>(physical_address);

        return STATUS_SUCCESS;
    }

    auto find_min(INT32 g, SIZE_T f) -> ULONG64
    {
        INT32 h = (INT32)f;
        ULONG64 result = 0;

        result = (((g) < (h)) ? (g) : (h));

        return result;
    }

    NTSTATUS read_memory(invoke_data* request)
    {
        read_invoke data = { 0 };

        if (!utils::safe_copy(&data, request->data, sizeof(read_invoke)))
            return STATUS_UNSUCCESSFUL;

        PEPROCESS process = 0;
        if (!NT_SUCCESS(qtx_import(PsLookupProcessByProcessId)(reinterpret_cast<HANDLE>(data.pid), &process)))
            return STATUS_UNSUCCESSFUL;

        auto physical_address = translate_linear(data.dtb, data.address);
        if (!physical_address)
            return STATUS_UNSUCCESSFUL;

        auto final_size = find_min(PAGE_SIZE - (physical_address & 0xFFF), data.size);

        size_t bytes = 0;
        if (!NT_SUCCESS(read_physical(physical_address, reinterpret_cast<read_invoke*>(request->data)->buffer, final_size, &bytes)))
        {
            qtx_import(ObfDereferenceObject)(process);
            return STATUS_UNSUCCESSFUL;
        }

        qtx_import(ObfDereferenceObject)(process);
        return STATUS_SUCCESS;
    }

    NTSTATUS write_memory(invoke_data* request)
    {
        write_invoke data = { 0 };

        if (!utils::safe_copy(&data, request->data, sizeof(write_invoke)))
            return STATUS_UNSUCCESSFUL;

        PEPROCESS process = 0;
        if (!NT_SUCCESS((PsLookupProcessByProcessId)(reinterpret_cast<HANDLE>(data.pid), &process)))
            return STATUS_UNSUCCESSFUL;

        auto physical_address = translate_linear(data.dtb, data.address);
        if (!physical_address)
            return STATUS_UNSUCCESSFUL;

        auto final_size = find_min(PAGE_SIZE - (physical_address & 0xFFF), data.size);

        size_t bytes = 0;
        if (!NT_SUCCESS(write_physical(physical_address, reinterpret_cast<write_invoke*>(request->data)->buffer, final_size, &bytes)))
        {
            qtx_import(ObfDereferenceObject)(process);
            return STATUS_UNSUCCESSFUL;
        }

        qtx_import(ObfDereferenceObject)(process);
        return STATUS_SUCCESS;
    }
}
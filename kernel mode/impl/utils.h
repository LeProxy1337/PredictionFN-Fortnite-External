#include <intrin.h>
#include <ntimage.h>
#include "../dependencies/crt.h"

namespace utils
{
    uintptr_t swap_process(uintptr_t new_process)
    {
        auto current_thread = (uintptr_t)qtx_import(KeGetCurrentThread)();

        auto apc_state = *(uintptr_t*)(current_thread + 0x98);
        auto old_process = *(uintptr_t*)(apc_state + 0x20);
        *(uintptr_t*)(apc_state + 0x20) = new_process;

        auto dir_table_base = *(uintptr_t*)(new_process + 0x28);
        __writecr3(dir_table_base);

        return old_process;
    }

    uintptr_t resolve_relative_address(uintptr_t instruction, ULONG offset_offset, ULONG instruction_size)
    {
        auto instr = instruction;

        const auto rip_offset = *(PLONG)(instr + offset_offset);

        const auto resolved_addr = instr + instruction_size + rip_offset;

        return resolved_addr;
    }

    void* get_system_information(SYSTEM_INFORMATION_CLASS information_class)
    {
        unsigned long size = 32;
        char buffer[32];

        qtx_import(ZwQuerySystemInformation)(information_class, buffer, size, &size);

        void* info = qtx_import(ExAllocatePoolZero)(NonPagedPool, size, 7265746172);
        if (!info)
            return nullptr;

        if (!NT_SUCCESS(qtx_import(ZwQuerySystemInformation)(information_class, info, size, &size)))
        {
            ExFreePool(info);
            return nullptr;
        }

        return info;
    }

    _declspec(noinline) auto find_section(uintptr_t ModuleBase, char* SectionName) -> uintptr_t
    {
        PIMAGE_NT_HEADERS NtHeaders = (PIMAGE_NT_HEADERS)(ModuleBase + ((PIMAGE_DOS_HEADER)ModuleBase)->e_lfanew);
        PIMAGE_SECTION_HEADER Sections = IMAGE_FIRST_SECTION(NtHeaders);

        for (DWORD i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++)
        {
            PIMAGE_SECTION_HEADER Section = &Sections[i];
            if (crt::kmemcmp(Section->Name, SectionName, 5) == 0)
            {
                return ModuleBase + Section->VirtualAddress;
            }
        }

        return 0;
    }

    uintptr_t get_kernel_export(std::uintptr_t base, LPCSTR export_name)
    {
        if (!base) return NULL;

        PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)(base);
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
            return 0;

        PIMAGE_NT_HEADERS64 ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS64>((UINT64)(base)+dosHeader->e_lfanew);

        UINT32 exportsRva = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
        if (!exportsRva)
            return 0;

        PIMAGE_EXPORT_DIRECTORY exports = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>((UINT64)(base)+exportsRva);
        UINT32* nameRva = reinterpret_cast<UINT32*>((UINT64)(base)+exports->AddressOfNames);

        for (UINT32 i = 0; i < exports->NumberOfNames; ++i)
        {
            CHAR* func = reinterpret_cast<CHAR*>((UINT64)(base)+nameRva[i]);
            if (crt::kstrcmp(func, export_name) == 0)
            {
                UINT32* funcRva = (UINT32*)((UINT64)(base)+exports->AddressOfFunctions);
                UINT16* ordinalRva = (UINT16*)((UINT64)(base)+exports->AddressOfNameOrdinals);

                return (base)+funcRva[ordinalRva[i]];
            }
        }
        return 0;
    }

    uintptr_t ControlRegister3()
    {
        // page-directory base register

    }

    _declspec(noinline) uintptr_t attach_process(uintptr_t process)
    {
        auto current_thread = (uintptr_t)qtx_import(KeGetCurrentThread)();
        if (!current_thread)
            return 0;

        auto apc_state = *(uintptr_t*)(current_thread + 0x98);
        auto old_process = *(uintptr_t*)(apc_state + 0x20);
        *(uintptr_t*)(apc_state + 0x20) = process;

        auto dir_table_base = *(uintptr_t*)(process + 0x28);
        __writecr3(dir_table_base);

        return old_process;
    }

    _declspec(noinline) bool safe_copy(void* dst, void* src, size_t size)
    {
        SIZE_T bytes = 0;

        if (qtx_import(MmCopyVirtualMemory)(
            qtx_import(IoGetCurrentProcess)(),
            src,
            qtx_import(IoGetCurrentProcess)(),
            dst,
            size,
            KernelMode,
            &bytes) == STATUS_SUCCESS && bytes == size)
        {
            return true;
        }

        return false;
    }

    void* get_kernel_image(const char* module_name)
    {
        auto addr = PVOID(0);
        auto bytes = unsigned long(0);

        qtx_import(ZwQuerySystemInformation)((ULONG)11, (PVOID)NULL, (ULONG)bytes, &bytes);
        if (!bytes) return NULL;

        if (auto modules = (PRTL_PROCESS_MODULES)qtx_import(ExAllocatePool)(NonPagedPool, bytes))
        {
            qtx_import(ZwQuerySystemInformation)(11, modules, bytes, &bytes);

            for (unsigned long i = 0; i < modules->NumberOfModules; i++)
            {
                RTL_PROCESS_MODULE_INFORMATION m = modules->Modules[i];

                if (crt::StrStr((char*)((PCHAR)m.FullPathName), module_name))
                {
                    addr = m.ImageBase;

                    break;
                }
            }

            qtx_import(ExFreePoolWithTag)(modules, 0);
        }

        return addr;
    }

    UNICODE_STRING concatenate_strings(const wchar_t* str1, const wchar_t* str2)
    {
        UNICODE_STRING result;
        qtx_import(RtlInitUnicodeString)(&result, nullptr);

        size_t length1 = wcslen(str1);
        size_t length2 = wcslen(str2);
        size_t totalLength = length1 + length2;

        result.Buffer = (wchar_t*)qtx_import(ExAllocatePool)(NonPagedPool, (totalLength + 1) * sizeof(wchar_t));

        if (result.Buffer)
        {
            result.Length = (USHORT)(totalLength * sizeof(wchar_t));
            result.MaximumLength = (USHORT)((totalLength + 1) * sizeof(wchar_t));

            crt::MemCpy(result.Buffer, str1, length1 * sizeof(wchar_t));

            crt::MemCpy(result.Buffer + length1, str2, (length2 + 1) * sizeof(wchar_t));
        }

        return result;
    }

    PIMAGE_NT_HEADERS get_nt_headers(PVOID module)
    {
        if (!module)
            return nullptr;
        return (PIMAGE_NT_HEADERS)((PBYTE)module + PIMAGE_DOS_HEADER(module)->e_lfanew);
    }

    std::uintptr_t get_ntos_base_address()
    {
        typedef unsigned char uint8_t;
        auto Idt_base = reinterpret_cast<uintptr_t>(KeGetPcr()->IdtBase);
        auto align_page = *reinterpret_cast<uintptr_t*>(Idt_base + 4) >> 0xc << 0xc;

        for (; align_page; align_page -= PAGE_SIZE)
        {
            for (int index = 0; index < PAGE_SIZE - 0x7; index++)
            {
                auto current_address = static_cast<intptr_t>(align_page) + index;

                if (*reinterpret_cast<uint8_t*>(current_address) == 0x48
                    && *reinterpret_cast<uint8_t*>(current_address + 1) == 0x8D
                    && *reinterpret_cast<uint8_t*>(current_address + 2) == 0x1D
                    && *reinterpret_cast<uint8_t*>(current_address + 6) == 0xFF)
                {
                    // rva
                    auto Ntosbase = resolve_relative_address(current_address, 3, 7);
                    if (!((UINT64)Ntosbase & 0xfff))
                    {
                        return Ntosbase;
                    }
                }
            }
        }
        return 0;
    }

    uintptr_t find_pattern(uintptr_t module_base, const char* pattern)
    {
        auto pattern_ = pattern;
        uintptr_t first_match = 0;

        if (!module_base)
        {
            return 0;
        }

        const auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(module_base + reinterpret_cast<IMAGE_DOS_HEADER*>(module_base)->e_lfanew);

        for (uintptr_t current = module_base; current < module_base + nt->OptionalHeader.SizeOfImage; current++)
        {
            if (!*pattern_)
            {
                return first_match;
            }

            if (*(BYTE*)pattern_ == '\?' || *(BYTE*)current == get_byte(pattern_))
            {
                if (!first_match)
                    first_match = current;

                if (!pattern_[2])
                    return first_match;

                if (*(WORD*)pattern_ == '\?\?' || *(BYTE*)pattern_ != '\?')
                    pattern_ += 3;

                else
                    pattern_ += 2;
            }
            else
            {
                pattern_ = pattern;
                first_match = 0;
            }
        }

        return 0;
    }

    bool write_protected_address(void* address, void* buffer, SIZE_T size, bool Restore)
    {
        NTSTATUS Status = { STATUS_SUCCESS };

        auto Mdl = qtx_import(IoAllocateMdl)(address, size, FALSE, FALSE, nullptr);
        qtx_import(MmProbeAndLockPages)(Mdl, KernelMode, IoReadAccess);

        auto Mapping = qtx_import(MmMapLockedPagesSpecifyCache)(Mdl, KernelMode, MmNonCached, (PVOID)NULL, FALSE, NormalPagePriority);

        Status = qtx_import(MmProtectMdlSystemAddress)(Mdl, PAGE_READWRITE);
        if (Status != STATUS_SUCCESS)
        {
            qtx_import(MmUnmapLockedPages)(Mapping, Mdl);
            qtx_import(MmUnlockPages)(Mdl);
            qtx_import(IoFreeMdl)(Mdl);
        }

        crt::MemCpy(Mapping, buffer, size);

        if (Restore)
        {
            Status = qtx_import(MmProtectMdlSystemAddress)(Mdl, PAGE_READONLY);
        }

        qtx_import(MmUnmapLockedPages)(Mapping, Mdl);
        qtx_import(MmUnlockPages)(Mdl);
        qtx_import(IoFreeMdl)(Mdl);

        return Status;
    }
}
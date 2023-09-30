#pragma once

namespace cache
{
	std::uintptr_t cave_base = 0;
	std::uintptr_t cave_base_2 = 0;
	std::uintptr_t ntos_image_base = 0;

	NTSTATUS(__fastcall* io_create_driver)(_In_opt_ PUNICODE_STRING Driver, PDRIVER_INITIALIZE INIT);

	std::uint8_t raw_shellcode[] = {
		0x90,															//nop
		0x48, 0x31, 0xC0,												//xor rax, rax
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		//mov rax, 0x0000000000000000
		0xFF, 0xE0														//jmp rax
	};
}

#define DEVICE_HANDLE _(L"{f8241d69-4464-47e6-8627-b3bcce202271}")
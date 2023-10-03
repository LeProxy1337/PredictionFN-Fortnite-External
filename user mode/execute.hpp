#pragma once
#include <cstdint>
#include <wtypes.h>
#include "comm/driver.hpp"
#include "structs.hpp"
#include "sdk.hpp"
#ifndef HOOK_H
#define HOOK_H

const std::uintptr_t mov = 0x8B; // move data between memory locations
const std::uintptr_t jmp = 0xE9; // transfer control to a different part of the program by performing an jump to a specified memory location
const std::uintptr_t nop = 0x90; // No Operation
const std::uintptr_t ret = 0xC3; // Return 

namespace exploiting
{
	namespace G
	{
		std::uintptr_t TextAddress;
		std::uintptr_t DataAddress;
		std::uintptr_t bSilentEnable;	
		std::uintptr_t SilentRotation;
	}

	BYTE Align20[] = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };
	BYTE GetViewPointHook[] = {
		0x44, 0x88, 0x44, 0x24, 0x18,                                   //mov[rsp + arg_10], r8b 4
		0x48, 0x89, 0x54, 0x24, 0x10,                                   //mov[rsp + arg_8], rdx 9
		0x48, 0x89, 0x4C, 0x24, 0x08,                                   //mov[rsp+arg_0], rcx 14
		0x56,                                                           //puhs rsi 15
		0x57,                                                           //push rdi 16
		0x48, 0x83, 0xEC, 0x28,                                         //sub rsp, 28h 20
		0x44, 0x0F, 0xB6, 0x44, 0x24, 0x50,                             //movzx r8d, [rsp+38h+arg_10] 26
		0x48, 0x8B, 0x54, 0x24, 0x48,                                   //mov rdx, [rsp+38h+arg_8] 31
		0x48, 0x8B, 0x4C, 0x24, 0x40,                                   //mov rcx, [rsp+38h+arg_0] 36
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     //mov rax, 0x0 46                         # GetViewPointOriginal
		0xFF, 0xD0,                                                     //call rax 48
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     //mov rax, 0x0 58                         # bSilentEnable
		0x0F, 0xB6, 0x00,                                               //movzx eax, byte ptr [rax] 61
		0x85, 0xC0,                                                     //test eax, eax 63
		0x74, 0x36,                                                     //jz short loc_FFFFFFFFF 65
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     //mov rax, 0x0 75                         # RealLocation
		0x48, 0x8B, 0x7C, 0x24, 0x48,                                   //mov rdi, [rsp+38h+arg_8] 80
		0x48, 0x8B, 0xF0,                                               //mov rsi, rax 83
		0xB9, 0x18, 0x00, 0x00, 0x00,                                   //mov ecx, 18h 88
		0xF3, 0xA4,                                                     //rep movsb 90
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     //mov rax, 0x0 100                         # RealRotation
		0x48, 0x8B, 0x4C, 0x24, 0x48,                                   //mov rcx, [rsp+38h+arg_8] 105
		0x48, 0x8D, 0x79, 0x18,                                         //lea rdi, [rcx+18h] 109
		0x48, 0x8B, 0xF0,                                               //mov rsi, rax 112
		0xB9, 0x18, 0x00, 0x00, 0x00,                                   //mov ecx, 18h 117
		0xF3, 0xA4,                                                     //rep movsb 119
		0x48, 0x83, 0xC4, 0x28,                                         //add rsp, 28h 123
		0x5F,                                                           //pop rdi 124
		0x5E,                                                           //pop rsi 125
		0xC3                                                            //retn 126
	};

	BYTE GetPlayerViewPointHook[] = {
		0x4C, 0x89, 0x44, 0x24, 0x18,                                   //mov [rsp+arg_10], r8 4
		0x48, 0x89, 0x54, 0x24, 0x10,                                   //mov[rsp + arg_8], rdx 9
		0x48, 0x89, 0x4C, 0x24, 0x08,                                   //mov[rsp + arg_0], rcx 14
		0x56,                                                           //push rsi 15
		0x57,                                                           //push rdi 16 
		0x48, 0x83, 0xEC, 0x28,                                         //sub rsp, 28h 20
		0x4C, 0x8B, 0x44, 0x24, 0x50,                                   //mov r8, [rsp+38h+arg_10] 25
		0x48, 0x8B, 0x54, 0x24, 0x48,                                   //mov rdx, [rsp+38h+arg_8] 30
		0x48, 0x8B, 0x4C, 0x24, 0x40,                                   //mov rcx, [rsp+38h+arg_0] 35
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     //mov rax, 0x0 45                         # GetPlayerViewPointOriginal
		0xFF, 0xD0,                                                     //call rax 47
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     //mov rax, 0x0 57                         # RealLocation
		0x48, 0x8B, 0xF8,                                               //mov rdi, rax 60
		0x48, 0x8B, 0x74, 0x24, 0x48,                                   //mov rsi, [rsp + 38h + arg_8] 65
		0xB9, 0x18, 0x00, 0x00, 0x00,                                   //mov ecx, 18h 70
		0xF3, 0xA4,                                                     //rep movsb 72
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     //mov rax, 0x0 82                         # RealRotation
		0x48, 0x8B, 0xF8,                                               //mov rdi, rax 85
		0x48, 0x8B, 0x74, 0x24, 0x50,                                   //mov rsi, [rsp+38h+arg_10] 90
		0xB9, 0x18, 0x00, 0x00, 0x00,                                   //mov ecx, 18h 95
		0xF3, 0xA4,                                                     //rep movsb 97
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     //mov rax, 0x0 107                         # bSilentEnable
		0x0F, 0xB6, 0x00,                                               //movzx eax, byte ptr [rax] 110
		0x85, 0xC0,                                                     //test eax, eax 112
		0x74, 0x19,                                                     //jz short loc_FFFFFFFFF 114
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     //mov rax, 0x0 124                         # SilentRotation
		0x48, 0x8B, 0x7C, 0x24, 0x50,                                   //mov rdi, [rsp+38h+arg_10] 129
		0x48, 0x8B, 0xF0,                                               //mov rsi, rax 132
		0xB9, 0x18, 0x00, 0x00, 0x00,                                   //mov ecx, 18h 137
		0xF3, 0xA4,                                                     //rep movsb 139
		0x48, 0x83, 0xC4, 0x28,                                         //add rsp, 28h 143
		0x5F,                                                           //pop rdi 144
		0x5E,                                                           //pop rsi 145
		0xC3                                                            //retn 146
	};

	__forceinline BOOL setup_silent(uintptr_t LocalPlayer, uintptr_t PlayerController)
	{
		if (!LocalPlayer || !PlayerController) return FALSE;
		uintptr_t TextAddress = driver::protect_virtual(0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		uintptr_t DataAddress = driver::protect_virtual(0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

		uintptr_t Addr_bSilentEnable = DataAddress;
		uintptr_t Addr_RealLocation = Addr_bSilentEnable + sizeof(bool);
		uintptr_t Addr_RealRotation = Addr_RealLocation + sizeof(FVector);
		uintptr_t Addr_SilentRotation = Addr_RealRotation + sizeof(FVector);

		driver::write<bool>(Addr_bSilentEnable, false);
		driver::write<FVector>(Addr_RealLocation, FVector(1.0, 2.0, 3.0));
		driver::write<FVector>(Addr_RealRotation, FVector(1.0, 2.0, 3.0));
		driver::write<FVector>(Addr_SilentRotation, FVector(1.0, 2.0, 3.0));

		uintptr_t LocalPlayerVTable = driver::read<uintptr_t>(LocalPlayer);
		uintptr_t PlayerControllerVTable = driver::read<uintptr_t>(PlayerController);

		uintptr_t GetViewPointOriginal = driver::read<uintptr_t>(LocalPlayerVTable + 0x2B8 + 8);
		uintptr_t GetPlayerViewPointOriginal = driver::read<uintptr_t>(PlayerControllerVTable + 0x7B8);

		*reinterpret_cast<void**>(&GetViewPointHook[39]) = (void*)GetViewPointOriginal;
		*reinterpret_cast<void**>(&GetViewPointHook[51]) = (void*)Addr_bSilentEnable;
		*reinterpret_cast<void**>(&GetViewPointHook[68]) = (void*)Addr_RealLocation;
		*reinterpret_cast<void**>(&GetViewPointHook[93]) = (void*)Addr_RealRotation;

		*reinterpret_cast<void**>(&GetPlayerViewPointHook[38]) = (void*)GetPlayerViewPointOriginal;
		*reinterpret_cast<void**>(&GetPlayerViewPointHook[50]) = (void*)Addr_RealLocation;
		*reinterpret_cast<void**>(&GetPlayerViewPointHook[75]) = (void*)Addr_RealRotation;
		*reinterpret_cast<void**>(&GetPlayerViewPointHook[100]) = (void*)Addr_bSilentEnable;
		*reinterpret_cast<void**>(&GetPlayerViewPointHook[117]) = (void*)Addr_SilentRotation;

		driver::write_physical(TextAddress, &Align20, sizeof(Align20));
		driver::write_physical(TextAddress + sizeof(Align20), &GetViewPointHook, sizeof(GetViewPointHook));
		driver::write_physical(TextAddress + sizeof(Align20) + sizeof(GetViewPointHook), &Align20, sizeof(Align20));
		driver::write_physical(TextAddress + sizeof(Align20) + sizeof(GetViewPointHook) + sizeof(Align20), &GetPlayerViewPointHook, sizeof(GetPlayerViewPointHook));
		driver::write_physical(TextAddress + sizeof(Align20) + sizeof(GetViewPointHook) + sizeof(Align20) + sizeof(GetPlayerViewPointHook), &Align20, sizeof(Align20));

		G::TextAddress = TextAddress;
		G::DataAddress = DataAddress;
		G::bSilentEnable = Addr_bSilentEnable;
		G::SilentRotation = Addr_SilentRotation;

		driver::protect_virtual(TextAddress, 0x1000, PAGE_EXECUTE_READ);
		Sleep(250);
		return TRUE;
	}

	__forceinline void update_silent(uintptr_t LocalPlayer, uintptr_t PlayerController)
	{
		static uintptr_t OLocalPlayer, OPlayerController;

		if (LocalPlayer != OLocalPlayer && LocalPlayer)
		{
			uintptr_t LocalPlayerVTable = driver::read<uintptr_t>(LocalPlayer);
			if (!LocalPlayerVTable) return;

			auto Original = driver::protect_virtual(LocalPlayerVTable, 0x1000, PAGE_READWRITE);
			driver::write<uintptr_t>(LocalPlayerVTable + 0x2B8 + 8, G::TextAddress + sizeof(Align20));
			driver::protect_virtual(LocalPlayerVTable, 0x1000, Original);

			OLocalPlayer = LocalPlayer;
		}

		if (PlayerController != OPlayerController && PlayerController)
		{
			uintptr_t PlayerControllerVTable = driver::read<uintptr_t>(PlayerController);
			if (!PlayerControllerVTable) return;

			auto Original = driver::protect_virtual(PlayerControllerVTable, 0x1000, PAGE_READWRITE);
			driver::write<uintptr_t>(PlayerControllerVTable + 0x7B8, G::TextAddress + sizeof(Align20) + sizeof(GetViewPointHook) + sizeof(Align20));
			driver::protect_virtual(PlayerControllerVTable, 0x1000, Original);

			OPlayerController = PlayerController;
		}
	}
}

void execute_exploits()
{
	while (true)
	{
		do {
			if (exploits->fire_rate != exploits->fire_rate_old) {
				uintptr_t GetFireRate = driver::read<uintptr_t>(module::image_base + 0x9BAE2B0);
				BYTE original_bytes[] = { 0xF3, 0x0F, 0x11, 0x03 };

				if (exploits->fire_rate) {
					for (int i = 0; i < sizeof(original_bytes); i++) {
						driver::write<BYTE>(GetFireRate + i, nop);
					}
				}
				else {
					for (int i = 0; i < sizeof(original_bytes); i++) {
						driver::write<BYTE>(GetFireRate + i, original_bytes[i]);
					}
				}

				exploits->fire_rate_old = exploits->fire_rate;
			}

			if (exploits->no_spread != exploits->no_spread_old) {
				uintptr_t GetCalculatedReticleSpread = driver::read<uintptr_t>(module::image_base + 0x9B995BF);
				BYTE original_bytes[] = { 0xF3, 0x0F, 0x11, 0x03 };

				if (exploits->no_spread) {
					for (int i = 0; i < sizeof(original_bytes); i++) {
						driver::write<BYTE>(GetCalculatedReticleSpread + i, nop);
					}
				}
				else {
					for (int i = 0; i < sizeof(original_bytes); i++) {
						driver::write<BYTE>(GetCalculatedReticleSpread + i, original_bytes[i]);
					}
				}

				exploits->no_spread = exploits->no_spread_old;
			}

			if (exploits->instant_reload != exploits->instant_reload_old) {
				uintptr_t GetReloadTime = driver::read<uintptr_t>(module::image_base + 0x9B84F2F);
				BYTE original_bytes[] = { 0xF3, 0x0F, 0x11, 0x03 };

				if (exploits->instant_reload) {
					for (int i = 0; i < sizeof(original_bytes); i++) {
						driver::write<BYTE>(GetReloadTime + i, nop);
					}
				}
				else {
					for (int i = 0; i < sizeof(original_bytes); i++) {
						driver::write<BYTE>(GetReloadTime + i, original_bytes[i]);
					}
				}

				exploits->instant_reload = exploits->instant_reload_old;
			}

			if (exploits->no_clip) {
				if (!exploits->no_clip_init) {
					uintptr_t Exploit = driver::read<uintptr_t>(module::image_base + 0x9AC7AFC + 0x3A);
					uintptr_t Exploit2 = driver::read<uintptr_t>(module::image_base + 0x9AC7B4E); //NOTE: fixed crash when shoot
					for (int32_t i = 0; i++;) {
						uintptr_t Address = driver::read<uintptr_t>(Exploit + i);
						if (Address == Exploit2 || !Exploit) break;

						driver::write<BYTE>(Address, ret);
					} exploits->no_clip_init = true;
				}
			}

			if (exploits->no_over_heat != exploits->no_over_heat_old) {
				uintptr_t GetOverheatingMaxValue = driver::read<uintptr_t>(module::image_base + 0x862223D);
				BYTE original_bytes[] = { 0x8B, 0x81, 0xD0, 0x00, 0x00, 0x00 };

				if (exploits->no_over_heat) {
					for (int i = 0; i < sizeof(original_bytes); i++) {
						driver::write<BYTE>(GetOverheatingMaxValue + i, nop);
					}
				}
				else {
					for (int i = 0; i < sizeof(original_bytes); i++) {
						driver::write<BYTE>(GetOverheatingMaxValue + i, original_bytes[i]);
					}
				}

				exploits->no_over_heat = exploits->no_over_heat_old;
			}
		} while (FALSE);
	}
}
#endif // HOOK_H
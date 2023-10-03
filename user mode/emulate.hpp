#pragma once
#include <cstdint>
#include <WinUser.h>
#include <random>
#include "variables.hpp"
#include "includes.hpp"
#include "spoof.hpp"
#include "sdk.hpp"
#include "execute.hpp"
#ifndef AIMBOT_H
#define AIMBOT_H

struct emulate_aimbot_input
{
	void AddInput(APlayerController PlayerController, FRotator TargetRotation, camera_ Camera)
	{
		if (PlayerController.IsValidClass())
		{
			TargetRotation.Pitch = (TargetRotation.Pitch - Camera.Rotation.x) / double(PlayerController.InputPitchScale());
			TargetRotation.Yaw = (TargetRotation.Yaw - Camera.Rotation.y) / double(PlayerController.InputYawScale());

			driver::write<float>(PlayerController.GetAddress() + offsets::InputYawScale, TargetRotation.Pitch);
			driver::write<float>(PlayerController.GetAddress() + offsets::InputPitchScale, TargetRotation.Yaw);
		}
	}
}; std::unique_ptr<emulate_aimbot_input> game_emulate = std::make_unique<emulate_aimbot_input>();

struct mouse_
{
	void mouse_aim(double x, double y, int smooth)
	{
		float ScreenCenterX = center_x;
		float ScreenCenterY = center_y;
		int AimSpeed = smooth;
		float TargetX = 0;
		float TargetY = 0;

		if (x != 0)
		{
			if (x > ScreenCenterX)
			{
				TargetX = -(ScreenCenterX - x);
				TargetX /= AimSpeed;
				if (TargetX + ScreenCenterX > ScreenCenterX * 2) TargetX = 0;
			}

			if (x < ScreenCenterX)
			{
				TargetX = x - ScreenCenterX;
				TargetX /= AimSpeed;
				if (TargetX + ScreenCenterX < 0) TargetX = 0;
			}
		}

		if (y != 0)
		{
			if (y > ScreenCenterY)
			{
				TargetY = -(ScreenCenterY - y);
				TargetY /= AimSpeed;
				if (TargetY + ScreenCenterY > ScreenCenterY * 2) TargetY = 0;
			}

			if (y < ScreenCenterY)
			{
				TargetY = y - ScreenCenterY;
				TargetY /= AimSpeed;
				if (TargetY + ScreenCenterY < 0) TargetY = 0;
			}
		}

		if (aimbot->humanization)
		{
			float targetx_min = TargetX - 1;
			float targetx_max = TargetX + 1;

			float targety_min = TargetY - 1;
			float targety_max = TargetY + 1;

			float offset_x = RandomFloat(targetx_min, targetx_max);
			float offset_y = RandomFloat(targety_min, targety_max);

			mouse_event(MOUSEEVENTF_MOVE, static_cast<int>((float)offset_x), static_cast<int>((float)offset_y / 1), NULL, NULL);
		}
		else
		{
			mouse_event(MOUSEEVENTF_MOVE, TargetX, TargetY, NULL, NULL);
		}

		return;
	}
}; std::unique_ptr<mouse_> mouse = std::make_unique<mouse_>();

struct aimbot_utilites
{
	FVector target_prediction(FVector TargetPosition, FVector ComponentVelocity, float player_distance, float ProjectileSpeed)
	{
		float gravity = abs(-336); // Gravity (Probably other ways to get this but I set it as a constant value)
		float time = player_distance / abs(ProjectileSpeed);
		float bulletDrop = (gravity / 250) * time * time;
		return FVector(TargetPosition.x += time * (ComponentVelocity.x), TargetPosition.y += time * (ComponentVelocity.y), TargetPosition.z += time * (ComponentVelocity.z));
	}
}; std::unique_ptr<aimbot_utilites> aimbot_helper = std::make_unique<aimbot_utilites>();

void aimbot_thread()
{
	while (true)
	{
		for (unsigned long i = 0; i < global::Player.size(); ++i)
		{
			AActor target_player = global::Player[i];
			AActor frozen_pawn = struct AActor(0);
			USkeletalMeshComponent closest_mesh = target_player.Mesh();
			float aim_raduis = (aimbot->fov * center_x / camera.FieldOfView);
			static bool setup_silent_aimbot = false;
			static bool set_pawn_feeze = false;
			FVector TargetPosition3D = { };
			FVector2D TargetPosition2D = { };
			static int BindedKey = 0;

			if (aimbot->bWeapon)
				if (global::LocalPawn.CurrentWeapon().IsValidClass() && global::LocalPawn.CurrentWeapon().GetAmmoCount() >= 0)
					continue;

			if (aimbot->bOnlyMale && !driver::read<BYTE>(target_player.GetAddress() + 0x5B14))
				continue;

			if (aimbot->Head) TargetPosition3D = closest_mesh.GetBone(106);
			else if (aimbot->Neck) TargetPosition3D = closest_mesh.GetBone(66);
			else if (aimbot->Chest) TargetPosition3D = closest_mesh.GetBone(7);
			else if (aimbot->Pelvis) TargetPosition3D = closest_mesh.GetBone(2);
			else if (aimbot->Rand) {
				std::random_device rd;
				std::mt19937 gen(rd());
				std::uniform_int_distribution<> dis(0, 3);
				int random_number = dis(gen);
				if (random_number == 0) TargetPosition3D = closest_mesh.GetBone(106);
				else if (random_number == 1) TargetPosition3D = closest_mesh.GetBone(66);
				else if (random_number == 2) TargetPosition3D = closest_mesh.GetBone(7);
				else if (random_number == 3) TargetPosition3D = closest_mesh.GetBone(2);
				else TargetPosition3D = closest_mesh.GetBone(106);
			}
			else if (aimbot->Closest) {
				FVector Head, Neck, Chest, Pelvis;
				FVector2D HeadWorld = global::LocalPlayer.W2S(Head = closest_mesh.GetBone(106));
				FVector2D NeckWorld = global::LocalPlayer.W2S(Neck = closest_mesh.GetBone(66));
				FVector2D ChestWorld = global::LocalPlayer.W2S(Chest = closest_mesh.GetBone(7));
				FVector2D PelvisWorld = global::LocalPlayer.W2S(Pelvis = closest_mesh.GetBone(2));

				ImVec2 HeadVector = ImVec2(HeadWorld.x - center_x, HeadWorld.y - center_y);
				auto HeadDistance = sqrtf(HeadVector.x * HeadVector.x + HeadVector.y * HeadVector.y);

				ImVec2 NeckVector = ImVec2(NeckWorld.x - center_x, NeckWorld.y - center_y);
				auto NeckDistance = sqrtf(NeckVector.x * NeckVector.x + NeckVector.y * NeckVector.y);

				ImVec2 ChestVector = ImVec2(ChestWorld.x - center_x, ChestWorld.y - center_y);
				auto ChestDistance = sqrtf(ChestVector.x * ChestVector.x + ChestVector.y * ChestVector.y);

				ImVec2 PelvisVector = ImVec2(PelvisWorld.x - center_x, PelvisWorld.y - center_y);
				auto PelvisDistance = sqrtf(PelvisVector.x * PelvisVector.x + PelvisVector.y * PelvisVector.y);

				if ((HeadDistance < NeckDistance) && (HeadDistance < ChestDistance) && (HeadDistance < PelvisDistance))
					TargetPosition3D = Head;
				if ((NeckDistance < HeadDistance) && (NeckDistance < ChestDistance) && (NeckDistance < PelvisDistance))
					TargetPosition3D = Neck;
				if ((ChestDistance < NeckDistance) && (ChestDistance < HeadDistance) && (ChestDistance < PelvisDistance))
					TargetPosition3D = Chest;
				if ((PelvisDistance < NeckDistance) && (PelvisDistance < ChestDistance) && (PelvisDistance < HeadDistance))
					TargetPosition3D = Pelvis;
				else
					TargetPosition3D = Head;
			}
			if (aimbot->prediction) {
				static auto speed = global::LocalPawn.CurrentWeapon().GetProjectileSpeed(); // ActiveWeapon->GetChargePercent()
				static auto distance = global::GWorld.GetCameraLocation().Distance(TargetPosition3D);
				aimbot_helper->target_prediction(TargetPosition3D, global::LocalPawn.RootComponent().ComponentVelocity(), distance, speed);
			}

			if (aimbot->waypoint && target_player.ActorFName().contains("BuildingWeakSpot"))
				if (auto ObjectID = target_player.GetClassId(target_player.GetAddress()); target_player.IsA(target_player.GetAddress(), ObjectID))
					TargetPosition3D = target_player.RootComponent().RelativeLocation();
			TargetPosition2D = global::LocalPlayer.W2S(TargetPosition3D);

			if (aimbot->RMouse)
				BindedKey = VK_RBUTTON;
			else if (aimbot->LMouse)
				BindedKey = VK_LBUTTON;
			else if (aimbot->LShift)
				BindedKey = VK_LSHIFT;
			else if (aimbot->LCtrl)
				BindedKey = VK_LCONTROL;

			double dx = TargetPosition2D.x - center_x;
			double dy = TargetPosition2D.y - center_y;
			float closest_head_distance = sqrtf(dx * dx + dy * dy);
			if (aimbot->Aimline && bIsInRectangle(center_x, center_y, aim_raduis, TargetPosition2D.x, TargetPosition2D.y))
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(center_x, center_y), ImVec2(TargetPosition2D.x, TargetPosition2D.y), global::LocalPawn.CurrentWeapon().CurrentReticleColor(), 0.5f);

			//std::this_thread::sleep_for(std::chrono::seconds(AimbotSlow));

			if (GetAsyncKeyState(BindedKey)) {
				if (TargetPosition2D.valid_location()) {
					if (bIsInRectangle(center_x, center_y, aim_raduis, TargetPosition2D.x, TargetPosition2D.y)) {
						if (closest_head_distance <= aimbot->fov * 2 && target_player.pawn_distance(TargetPosition3D) <= aimbot->AimDistance) {
							if (GetDistance(TargetPosition2D.x, TargetPosition2D.y, center_x, center_y) <= aimbot->fov) {
								if (aimbot->bAimbot)
									mouse->mouse_aim(TargetPosition2D.x, TargetPosition2D.y, aimbot->smoothness);
								else if (aimbot->bMemoryAim) {
									game_emulate->AddInput(global::PlayerController, GetRotation(TargetPosition3D), camera);
								}
								else if (aimbot->bSilentAimbot) {
									if (!setup_silent_aimbot) setup_silent_aimbot = exploiting::setup_silent(global::LocalPlayer.GetAddress(), global::PlayerController.GetAddress());
									if (setup_silent_aimbot)
										exploiting::update_silent(global::LocalPlayer.GetAddress(), global::PlayerController.GetAddress());
								}

								if (aimbot->Backtrack)
								{
									driver::write<float>(target_player.GetAddress() + 0x64, 0.f);
									frozen_pawn = target_player;
									set_pawn_feeze = true;
								}
							}
						}
					}
				}
			}

			if (aimbot->triggerbot)
			{
				if (closest_mesh.WasRecentlyRendered()) {
					if (TargetPosition2D.valid_location()) {
						if (bIsInRectangle(center_x, center_y, aim_raduis, TargetPosition2D.x, TargetPosition2D.y)) {
							if (closest_head_distance <= aimbot->fov * 2 && target_player.pawn_distance(TargetPosition3D) <= aimbot->AimDistance) {
								if (GetDistance(TargetPosition2D.x, TargetPosition2D.y, center_x, center_y) <= aimbot->fov) {
									mouse_event(MOUSEEVENTF_LEFTDOWN, DWORD(NULL), DWORD(NULL), DWORD(0x0002), ULONG_PTR(NULL));
									mouse_event(MOUSEEVENTF_LEFTUP, DWORD(NULL), DWORD(NULL), DWORD(0x0004), ULONG_PTR(NULL));
									int x = 0;
									while (x < aimbot->triggerSpeed * 10) x++;
									x = 0;
								}
							}
						}
					}
				}
			}

			if (set_pawn_feeze)
			{
				driver::write<float>(frozen_pawn.GetAddress() + 0x64, 0.f);
				frozen_pawn = struct AActor(0);
				set_pawn_feeze = false;
			}
		}
	}
}
#endif // AIMBOT_H
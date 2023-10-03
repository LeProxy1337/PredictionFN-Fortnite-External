#pragma once
#include <dwmapi.h>
#include <stdio.h>
#include <string>
#include "hexray.hpp"
#include "sdk.hpp"
#include "math.h"
#include "emulate.hpp"
#include "overlay.hpp"
#include "includes.hpp"
#include "spoof.hpp"
#include "menu.hpp"
#ifndef RENDER_HPP
#define RENDER_HPP
using namespace std;

void Range(double* x, double* y, float range)
{
	if (fabs((*x)) > range || fabs((*y)) > range)
	{
		if ((*y) > (*x))
		{
			if ((*y) > -(*x))
			{
				(*x) = range * (*x) / (*y);
				(*y) = range;
			}
			else
			{
				(*y) = -range * (*y) / (*x);
				(*x) = -range;
			}
		}
		else
		{
			if ((*y) > -(*x))
			{
				(*y) = range * (*y) / (*x);
				(*x) = range;
			}
			else
			{
				(*x) = -range * (*x) / (*y);
				(*y) = -range;
			}
		}
	}
}

FVector2D RotatePoint(FVector2D radar_pos, FVector2D radar_size, FVector LocalLocation, FVector TargetLocation) {
	auto dx = TargetLocation.x - LocalLocation.x;
	auto dy = TargetLocation.y - LocalLocation.y;

	auto x = dy * -1;
	x *= -1;
	auto y = dx * -1;

	double calcualted_range = 34 * 1000;

	Range(&x, &y, calcualted_range);

	int rad_x = (int)radar_pos.x;
	int rad_y = (int)radar_pos.y;

	double r_siz_x = radar_size.x;
	double r_siz_y = radar_size.y;

	int x_max = (int)r_siz_x + rad_x - 5;
	int y_max = (int)r_siz_y + rad_y - 5;

	auto return_value = FVector2D();

	return_value.x = rad_x + ((int)r_siz_x / 2 + int(x / calcualted_range * r_siz_x));
	return_value.y = rad_y + ((int)r_siz_y / 2 + int(y / calcualted_range * r_siz_y));

	if (return_value.x > x_max)
		return_value.x = x_max;

	if (return_value.x < rad_x)
		return_value.x = rad_x;

	if (return_value.y > y_max)
		return_value.y = y_max;

	if (return_value.y < rad_y)
		return_value.y = rad_y;

	return return_value;
}

void AddToRadar(FVector WorldLocation, FVector LocalLocation, ImColor Color, FVector2D RadarPos, FVector2D RadarSize)
{
	FVector2D Projected = RotatePoint(RadarPos, RadarSize, LocalLocation, WorldLocation);
	ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(Projected.x, Projected.y), 3, Color, 32);
}

void playerpawn_loop()
{
	do {
		std::unique_lock<std::mutex> lock(global::data_mutex);
		if (aimbot->showfov)
			ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(center_x, center_y), (aimbot->fov * center_x / camera.FieldOfView), ImColor(0, 0, 0, 255), 2000, 0.9f);

		for (unsigned long i = 0; i < global::Player.size(); ++i)
		{
			AActor entity = global::Player[i];
			USkeletalMeshComponent mesh = entity.Mesh();
			int MyTeamId = global::LocalPawn.PlayerState().TeamIndex();
			int ActorTeamId = entity.PlayerState().TeamIndex();
			bool IsVisible = mesh.WasRecentlyRendered();

			if (misc->bIgnoreTeam) {
				AFortAthenaVehicle Vehicle = driver::read<AFortAthenaVehicle>(entity.GetAddress() + offsets::CurrentVehicle);
				AFortAthenaVehicle CurrentVehicle = global::LocalPawn.CurrentVehicle();
				if (CurrentVehicle.CurrentTeam(Vehicle))
					continue;
				if (MyTeamId == ActorTeamId)
					continue;
			}

			if (misc->bIgnoreBots) {
				bool IsBot = driver::read<BYTE>(entity.GetAddress() + 0x292);
				if (!IsBot)
					continue;
			}

			if (misc->bIgnoreDead) {
				bool bIsDying = (driver::read<BYTE>(entity.GetAddress() + 0x750) & 0x10); // 0x710
				bool bisDBNO = (driver::read<BYTE>(entity.GetAddress() + 0x872) & 0x10); // 0x832
				if (bisDBNO || bIsDying)
					continue;
			}

			if (misc->bIgnoreAFK) {
				bool bIsDisconnectedPawn = driver::read<BYTE>(entity.GetAddress() + 0x11A8);
				if (!bIsDisconnectedPawn)
					continue;
			}

			if (visual->draw_radar)
			{
				float HUDScale = 0.f;

				if (UFortClientSettingsRecord ClientSettingsRecord = global::LocalPlayer.ClientSettingsRecord(); ClientSettingsRecord.IsValidClass())
				{
					HUDScale = ClientSettingsRecord.HUDScale();
				}

				double RadarSize = (15.00 * center_x * double(HUDScale) / 100.0) * 2;

				double RadarPositionOffset = RadarSize / 30.0;
				FVector2D RadarPosition = FVector2D(center_y - RadarSize - RadarPositionOffset, RadarPositionOffset);

				AddToRadar(mesh.GetBone(0), camera.Location, ImColor(255, 255, 255), RadarPosition, FVector2D(RadarSize, RadarSize));
			}

			if (misc->bIgnoreHidden) {
				bool bTargetedBuild = true;
				uintptr_t CurrentBuild = 0;
				uintptr_t TargetedBuild = global::PlayerController.TargetedBuild();
				if (TargetedBuild) {
					CurrentBuild = TargetedBuild;
					bTargetedBuild = false;
				}
				else {
					CurrentBuild = 0;
					bTargetedBuild = true;
				}

				if (CurrentBuild != 0 && CurrentBuild != TargetedBuild) {
					bTargetedBuild = false;
					continue;
				}
			}

			FVector head3d, root3d; double player_distance;
			FVector2D head = global::LocalPlayer.W2S(head3d = mesh.GetBone(106));
			FVector2D root = global::LocalPlayer.W2S(root3d = mesh.GetBone(88));
			if (player_distance = entity.pawn_distance(root3d) <= visual->MaxDistance && IsInScreen(root)) continue;
			FVector2D chest, pelvis, rshoulder, relbow, rhand, rknee, rfoot, lshoulder, lelbow, lhand, lknee, lfoot;
			chest = global::LocalPlayer.W2S(mesh.GetBone(7)); pelvis = global::LocalPlayer.W2S(mesh.GetBone(2));
			rshoulder = global::LocalPlayer.W2S(mesh.GetBone(35));
			relbow = global::LocalPlayer.W2S(mesh.GetBone(10)); rhand = global::LocalPlayer.W2S(mesh.GetBone(29));
			rknee = global::LocalPlayer.W2S(mesh.GetBone(72)); rfoot = global::LocalPlayer.W2S(mesh.GetBone(76));
			lshoulder = global::LocalPlayer.W2S(mesh.GetBone(64));
			lelbow = global::LocalPlayer.W2S(mesh.GetBone(65)); lhand = global::LocalPlayer.W2S(mesh.GetBone(62));
			lknee = global::LocalPlayer.W2S(mesh.GetBone(79)); lfoot = global::LocalPlayer.W2S(mesh.GetBone(83));
			FVector2D bones_to_check[] = { head, chest, pelvis, rhand, relbow, rfoot, lhand, lelbow, lfoot, root };

			auto most_left = DBL_MAX;
			auto most_right = DBL_MIN;
			auto most_top = DBL_MAX;
			auto most_bottom = FLT_MIN;
			for (int i = 0; i < sizeof(bones_to_check); i++) {
				auto bone = bones_to_check[i];

				if (bone.x < most_left)
					most_left = bone.x;

				if (bone.x > most_right)
					most_right = bone.x;

				if (bone.y < most_top)
					most_top = bone.y;

				if (bone.y > most_bottom)
					most_bottom = bone.y;
			}

			auto text_offset = double(0);
			auto actor_height = most_bottom - most_top;
			auto actor_width = most_right - most_left;

			auto calculated_distance = 225 - player_distance;
			auto offset = calculated_distance * 0.025;
			auto corner_width = actor_width / 3;
			auto corner_height = actor_height / 3;

			FVector2D top_middle = FVector2D(most_left + actor_width / 2, most_top);
			FVector2D bottom_middle = FVector2D(most_left + actor_width / 2, most_bottom);

			auto text_position = [&](double y) { return ImVec2(top_middle.x, y - text_offset); };

			auto color_box = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
			auto color_skeleton = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
			auto color_snapline = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

			if (MyTeamId != ActorTeamId)
				color_box = IsVisible ? ImVec4(visual->color->PlayerboxVisible[0], visual->color->PlayerboxVisible[1], visual->color->PlayerboxVisible[2], 1.0f) : ImVec4(visual->color->PlayerboxNotVisible[0], visual->color->PlayerboxNotVisible[1], visual->color->PlayerboxNotVisible[2], 1.0f);
			else
				color_box = IsVisible ? ImVec4(visual->color->TeamboxVisible[0], visual->color->TeamboxVisible[1], visual->color->TeamboxVisible[2], 1.0f) : ImVec4(visual->color->TeamboxNotVisible[0], visual->color->TeamboxNotVisible[1], visual->color->TeamboxNotVisible[2], 1.0f);

			color_skeleton = IsVisible ? ImVec4(visual->color->SkeletonVisible[0], visual->color->SkeletonVisible[1], visual->color->SkeletonVisible[2], 1.0f) : ImVec4(visual->color->SkeletonNotVisible[0], visual->color->SkeletonNotVisible[1], visual->color->SkeletonNotVisible[2], 1.0f);
			color_snapline = IsVisible ? ImVec4(visual->color->SnaplinesVisible[0], visual->color->SnaplinesVisible[1], visual->color->SnaplinesVisible[2], 1.0f) : ImVec4(visual->color->SnaplinesNotVisible[0], visual->color->SnaplinesNotVisible[1], visual->color->SnaplinesNotVisible[2], 1.0f);

			if (visual->box)
			{
				box_esp(offset, most_top, most_bottom, 
					most_right, most_left, 
					ImGui::GetColorU32(color_box), 1.f,
					visual->outline);
			}
			else if (visual->cornered_box)
			{
				corner_esp(offset, most_top, most_bottom,
					most_right, most_left,
					corner_width, corner_height,
					ImGui::GetColorU32(color_box), 1.f,
					visual->outline);
			}
			else if (visual->threed)
			{
				threed_box(30.0f, 
					root.x, root.y,
					head.x, head.y,
					ImGui::GetColorU32(color_box), 0.1f);
			}

			if (visual->fill) 
			{
				filled_box(most_left, most_bottom,
					actor_width, actor_height,
					ImColor(0, 0, 0, 100), 0.f);
			}

			if (visual->distance)
			{
				static std::string text_distance = "(" + std::to_string(player_distance) + "m)";
				if (!text_distance.empty())
					_DrawText(ESPFont, text_position(most_top), ImColor(255, 255, 255), text_distance.c_str(), visual->size, false);
			}

			if (visual->WeaponESP)
			{
				static std::string weapon_name = global::LocalPawn.CurrentWeapon().GetWeaponName(); 
				if (!weapon_name.empty()) {
					static std::string text_weapon = global::LocalPawn.CurrentWeapon().IsReloadingWeapon() ? ("reloading") : (weapon_name + " (" + std::to_string(global::LocalPawn.CurrentWeapon().GetAmmoCount()) + ")");
					_DrawText(ESPFont, text_position(most_bottom), ImColor(255, 255, 255), text_weapon.c_str(), visual->size, false);
				}
			}

			if (visual->name) 
			{
				static std::string player_name = entity.PlayerState().GetPlayerName();
				if (!player_name.empty())
					_DrawText(ESPFont, text_position(most_top + 20.0), ImColor(255, 255, 255), player_name.c_str(), visual->size, false);
			}

			if (visual->console)
			{
				static std::string player_platform = entity.PlayerState().GetPlatformName();
				if (!player_platform.empty()) {
					if (strstr(player_platform.c_str(), ("WIN")))
						_DrawText(ESPFont, text_position(most_bottom), ImColor(255, 255, 255), "Windows");
					else if (strstr(player_platform.c_str(), ("XBL")) || strstr(player_platform.c_str(), ("XSX")))
						_DrawText(ESPFont, text_position(most_bottom), ImColor(255, 255, 255), "Xbox");
					else if (strstr(player_platform.c_str(), ("PSN")) || strstr(player_platform.c_str(), ("PS5")))
						_DrawText(ESPFont, text_position(most_bottom), ImColor(255, 255, 255), "PlayStation");
					else if (strstr(player_platform.c_str(), ("SWT")))
						_DrawText(ESPFont, text_position(most_bottom), ImColor(255, 255, 255), "Nintendo");
					else if (strstr(player_platform.c_str(), ("MAC")))
						_DrawText(ESPFont, text_position(most_bottom), ImColor(255, 255, 255), "MacOS");
					else if (strstr(player_platform.c_str(), ("LNX")))
						_DrawText(ESPFont, text_position(most_bottom), ImColor(255, 255, 255), "Linux");
					else if (strstr(player_platform.c_str(), ("IOS")))
						_DrawText(ESPFont, text_position(most_bottom), ImColor(255, 255, 255), "IOS");
					else if (strstr(player_platform.c_str(), ("AND")))
						_DrawText(ESPFont, text_position(most_bottom), ImColor(255, 255, 255), "Android");
				}
			}

			if (visual->b2Dhead)
				ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(head.x, head.y), (float)(entity.pawn_distance(head3d) - 10.0), ImColor(255, 255, 255), 0.5f);

			if (visual->line)
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(center_x, Height), ImVec2(root.x, root.y), ImGui::GetColorU32(color_snapline), 0.5f);

			if (visual->bViewAngles)
			{
				FRotator player_rotation; FVector vplayer_rotation;
				FVector player_angle = entity.RootComponent().RelativeRotation();
				player_rotation = GetRotation(player_angle);
				vplayer_rotation.x = player_rotation.Pitch *= 360;
				vplayer_rotation.y = player_rotation.Yaw *= 360;
				vplayer_rotation.z = player_rotation.Roll *= 360;
				FVector2D head_rotation = global::LocalPlayer.W2S(head3d + vplayer_rotation);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(head.x, head.y), ImVec2(head_rotation.x, head_rotation.y), ImColor(255, 0, 255), 1.f);
			}

			if (visual->gayskeleton)
			{
				FVector2D neck, leftwrist, rightwrist, pelvis, leftthigh, lefthip, leftjoint, righthip, rightthigh, rightjoint;
				neck =  global::LocalPlayer.W2S(mesh.GetBone(67));
				leftwrist = global::LocalPlayer.W2S(mesh.GetBone(62));
				rightwrist = global::LocalPlayer.W2S(mesh.GetBone(33));
				pelvis = global::LocalPlayer.W2S(mesh.GetBone(2));
				lefthip = global::LocalPlayer.W2S(mesh.GetBone(78));
				leftthigh = global::LocalPlayer.W2S(mesh.GetBone(84));
				leftjoint = global::LocalPlayer.W2S(mesh.GetBone(80));
				righthip = global::LocalPlayer.W2S(mesh.GetBone(71));
				rightthigh = global::LocalPlayer.W2S(mesh.GetBone(77));
				rightjoint = global::LocalPlayer.W2S(mesh.GetBone(73));

				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(head.x, head.y), ImVec2(neck.x, neck.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(neck.x, neck.y), ImVec2(chest.x, chest.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(chest.x, chest.y), ImVec2(lshoulder.x, lshoulder.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(lshoulder.x, lshoulder.y), ImVec2(lelbow.x, lelbow.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(lelbow.x, lelbow.y), ImVec2(leftwrist.x, leftwrist.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(leftwrist.x, leftwrist.y), ImVec2(lhand.x, lhand.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(chest.x, chest.y), ImVec2(rshoulder.x, rshoulder.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(rshoulder.x, rshoulder.y), ImVec2(relbow.x, relbow.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(relbow.x, relbow.y), ImVec2(rightwrist.x, rightwrist.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(rightwrist.x, rightwrist.y), ImVec2(rhand.x, rhand.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(chest.x, chest.y), ImVec2(pelvis.x, pelvis.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(pelvis.x, pelvis.y), ImVec2(lefthip.x, lefthip.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(lefthip.x, lefthip.y), ImVec2(leftthigh.x, leftthigh.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(leftthigh.x, leftthigh.y), ImVec2(lknee.x, lknee.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(lknee.x, lknee.y), ImVec2(leftjoint.x, leftjoint.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(leftjoint.x, leftjoint.y), ImVec2(lfoot.x, lfoot.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(pelvis.x, pelvis.y), ImVec2(righthip.x, righthip.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(righthip.x, righthip.y), ImVec2(rightthigh.x, rightthigh.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(rightthigh.x, rightthigh.y), ImVec2(rknee.x, rknee.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(rknee.x, rknee.y), ImVec2(rightjoint.x, rightjoint.y), ImGui::GetColorU32(color_skeleton), 2.0f);
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(rightjoint.x, rightjoint.y), ImVec2(rfoot.x, rfoot.y), ImGui::GetColorU32(color_skeleton), 2.0f);
			}
		} 
		lock.unlock();
	} while (FALSE);
}

void itempawn_loop()
{
	do {
		std::unique_lock<std::mutex> lock(global::data_mutex);
		for (unsigned long i = 0; i < global::Item.size(); ++i)
		{
			AActor entity = global::Item[i];

			if (visual->loot->loot)
			{
				USceneComponent RootComponent = entity.RootComponent();
				if (RootComponent.IsValidClass())
					continue;

				FVector RelativeLocation = RootComponent.RelativeLocation();

				driver::write<bool>(entity.GetAddress() + 0x6fc, false); // bRandomRotation 0x6fc(0x01)

				if (misc->bIgnoreAirLoot) {
					float DefaultFlyTime = driver::read<float>(entity.GetAddress() + 0x5e0);
					bool bForceDefaultFlyTime = driver::read<float>(entity.GetAddress() + 0x5e0);
					if (DefaultFlyTime <= 0.f && bForceDefaultFlyTime <= 0.f)
						continue;
				}

				if (misc->bIgnoreWaterLoot) {
					float SimulatingTooLongLengthInWaterMoving = driver::read<float>(entity.GetAddress() + 0x2AC);
					float SimulatingTooLongLengthInWaterBobbing = driver::read<float>(entity.GetAddress() + 0x2B0);
					if (SimulatingTooLongLengthInWaterMoving <= 0.f && SimulatingTooLongLengthInWaterBobbing <= 0.f)
						continue;
				}

				if (misc->bIgnoreDespawned) {
					float DespawnTime = driver::read<float>(entity.GetAddress() + 0x728);
					float StormDespawnTime = driver::read<float>(entity.GetAddress() + 0x72C);
					if (DespawnTime >= 2.f && StormDespawnTime >= 2.f)
						continue;
				}

				double item_distance = 0.0;
				if (item_distance = entity.pawn_distance(RelativeLocation); item_distance <= visual->loot->MaxItemDistance)
					continue;

				{
					// Item inforatmiom
					uintptr_t PrimaryPickupItemEntry = driver::read<uintptr_t>(entity.GetAddress() + offsets::PrimaryPickupItemEntry);
					int32_t Count = driver::read<uintptr_t>(PrimaryPickupItemEntry + 0x0c); // 0x0c(0x04)
					int32_t Level = driver::read<uintptr_t>(PrimaryPickupItemEntry + 0x28); // 0x28(0x04)
					int32_t LoadedAmmo = driver::read<uintptr_t>(PrimaryPickupItemEntry + 0x2c); // 0x2c(0x04)
					debug_pointer(PrimaryPickupItemEntry, "PrimaryPickupItemEntry");
				}

				if (visual->loot->pickups && entity.ActorFName().contains("FortPickupAthena") || entity.ActorFName().contains("Fort_Pickup_Creative_C")) 
				{
					if (FVector2D Position = global::LocalPlayer.W2S(RelativeLocation); IsInScreen(Position)) {
						if (!driver::read<bool>(entity.GetAddress() + 0x5c9)) { // bPickedUp; 0x5c9(0x01)
							std::string DisplayName = driver::read<std::string>(entity.GetAddress() + 0x90);
							EFortRarity HighestRarity = (EFortRarity)driver::read<int>(entity.GetAddress() + 0xD90);
							FColor ItemColor;
							if (HighestRarity == EFortRarity::Common) //Rarity: Uncommon
								ItemColor.R = 61, ItemColor.G = 61, ItemColor.B = 76, ItemColor.A = 255;
							else if (HighestRarity == EFortRarity::Uncommon) //Rarity: Green
								ItemColor.R = 52, ItemColor.G = 71, ItemColor.B = 52, ItemColor.A = 255;
							else if (HighestRarity == EFortRarity::Rare) //Rarity: Blue
								ItemColor.R = 87, ItemColor.G = 134, ItemColor.B = 208, ItemColor.A = 255;
							else if (HighestRarity == EFortRarity::Epic) //Rarity: Purple
								ItemColor.R = 126, ItemColor.G = 0, ItemColor.B = 220, ItemColor.A = 255;
							else if (HighestRarity == EFortRarity::Legendary) //Rarity: Gold
								ItemColor.R = 195, ItemColor.G = 116, ItemColor.B = 43, ItemColor.A = 255; 
							else if (HighestRarity == EFortRarity::Mythic) //Rarity: Gold+
								ItemColor.R = 169, ItemColor.G = 139, ItemColor.B = 42, ItemColor.A = 255;
							std::string text_position = DisplayName + " [" + std::to_string(item_distance) + "m]";
							ImVec2 TextSize = ImGui::CalcTextSize(text_position.c_str());
							_DrawText(ESPFont, ImVec2((Position.x) - (TextSize.x / 2.0), (Position.y - 20.0)), ImColor((int)ItemColor.R, (int)ItemColor.G, (int)ItemColor.B, (int)ItemColor.A), text_position.c_str());
						}
					}
				}
				if (visual->loot->chests && entity.ActorFName().contains("Tiered_Chest"))
				{
					if (FVector2D Position = global::LocalPlayer.W2S(RelativeLocation); IsInScreen(Position)) {
						std::string text_position = "Chest [" + std::to_string(item_distance) + "m]";
						ImVec2 TextSize = ImGui::CalcTextSize(text_position.c_str());
						_DrawText(ESPFont, ImVec2((Position.x) - (TextSize.x / 2.0), (Position.y - 20.0)), ImColor(255, 255, 255), text_position.c_str());
					}
				}
				if (visual->loot->ammo && entity.ActorFName().contains("Tiered_Ammo"))
				{
					if (FVector2D Position = global::LocalPlayer.W2S(RelativeLocation); IsInScreen(Position)) {
						std::string text_position = "Ammo [" + std::to_string(item_distance) + "m]";
						ImVec2 TextSize = ImGui::CalcTextSize(text_position.c_str());
						_DrawText(ESPFont, ImVec2((Position.x) - (TextSize.x / 2.0), (Position.y - 20.0)), ImColor(255, 255, 255), text_position.c_str());
					}
				}
				if (visual->loot->supply && entity.ActorFName().contains("AthenaSupplyDrop_C"))
				{
					if (FVector2D Position = global::LocalPlayer.W2S(RelativeLocation); IsInScreen(Position)) {
						std::string text_position = "Supply Drop [" + std::to_string(item_distance) + "m]";
						ImVec2 TextSize = ImGui::CalcTextSize(text_position.c_str());
						_DrawText(ESPFont, ImVec2((Position.x) - (TextSize.x / 2.0), (Position.y - 20.0)), ImColor(255, 255, 255), text_position.c_str());
					}
				}
				if (visual->loot->vehicle && entity.ActorFName().contains("Vehicle") ||
					entity.ActorFName().contains("MeatballVehicle_L") ||
					entity.ActorFName().contains("Valet_Taxi") ||
					entity.ActorFName().contains("Valet_BigRig") ||
					entity.ActorFName().contains("Valet_BasicTr") ||
					entity.ActorFName().contains("Valet_SportsC") ||
					entity.ActorFName().contains("Valet_BasicC")) {
					if (FVector2D Position = global::LocalPlayer.W2S(RelativeLocation); IsInScreen(Position)) {
						std::string text_position = "Vehicle [" + std::to_string(item_distance) + "m]";
						ImVec2 TextSize = ImGui::CalcTextSize(text_position.c_str());
						_DrawText(ESPFont, ImVec2((Position.x) - (TextSize.x / 2.0), (Position.y - 20.0)), ImColor(255, 255, 255), text_position.c_str());
					}
				}
				if (visual->loot->animal && entity.ActorFName().contains("NPC_Pawn_Irwin_Predator_Robert_C") ||
					entity.ActorFName().contains("NPC_Pawn_Irwin_Prey_Burt_C") ||
					entity.ActorFName().contains("NPC_Pawn_Irwin_Simple_Smackie_C") ||
					entity.ActorFName().contains("NPC_Pawn_Irwin_Prey_Nug_C") ||
					entity.ActorFName().contains("NPC_Pawn_Irwin_Predator_Grandma_C")) {
					if (FVector2D Position = global::LocalPlayer.W2S(RelativeLocation); IsInScreen(Position)) {
						std::string text_position = "Animal [" + std::to_string(item_distance) + "m]";
						ImVec2 TextSize = ImGui::CalcTextSize(text_position.c_str());
						_DrawText(ESPFont, ImVec2((Position.x) - (TextSize.x / 2.0), (Position.y - 20.0)), ImColor(255, 255, 255), text_position.c_str());
					}
				}
				if (visual->loot->generades && entity.ActorFName().contains("Grenade")) {
					if (FVector2D Position = global::LocalPlayer.W2S(RelativeLocation); IsInScreen(Position)) {
						std::string text_position = "Grenade [" + std::to_string(item_distance) + "m]";
						ImVec2 TextSize = ImGui::CalcTextSize(text_position.c_str());
						_DrawText(ESPFont, ImVec2((Position.x) - (TextSize.x / 2.0), (Position.y - 20.0)), ImColor(255, 255, 255), text_position.c_str());
					}
				}
				if (visual->loot->trap && entity.ActorFName().contains("BuildingTrap")) {
					if (auto ObjectID = entity.GetClassId(entity.GetAddress()); entity.IsA(entity.GetAddress(), ObjectID)) {
						FVector2D Position = global::LocalPlayer.W2S(RelativeLocation);
						if (IsInScreen(Position)) {
							std::string text_position = "trap [" + std::to_string(item_distance) + "m]";
							ImVec2 TextSize = ImGui::CalcTextSize(text_position.c_str());
							_DrawText(ESPFont, ImVec2((Position.x) - (TextSize.x / 2.0), (Position.y - 20.0)), ImColor(255, 255, 255), text_position.c_str());
						}
					}
				}
				if (visual->loot->rift && entity.ActorFName().contains("FortAthenaRiftPortal")) {
					if (auto ObjectID = entity.GetClassId(entity.GetAddress()); entity.IsA(entity.GetAddress(), ObjectID)) {
						std::cout << " ObjectID " << ObjectID << std::endl; // decimal responce
						FVector2D Position = global::LocalPlayer.W2S(RelativeLocation);
						if (IsInScreen(Position)) {
							std::string text_position = "rift [" + std::to_string(item_distance) + "m]";
							ImVec2 TextSize = ImGui::CalcTextSize(text_position.c_str());
							_DrawText(ESPFont, ImVec2((Position.x) - (TextSize.x / 2.0), (Position.y - 20.0)), ImColor(255, 255, 255), text_position.c_str());
						}
					}
				}

			}
		}
		lock.unlock();
	} while (FALSE);
}

BOOL main_loop()
{
	while (overlay->Message.message != WM_QUIT)
	{
		if (PeekMessageA(&overlay->Message, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&overlay->Message);
			DispatchMessageA(&overlay->Message);
		}

		if (misc->StreamProof)
			SetWindowDisplayAffinity(overlay->Window, WDA_EXCLUDEFROMCAPTURE);
		else
			SetWindowDisplayAffinity(overlay->Window, WDA_NONE);

		// Start Frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// Draw
		playerpawn_loop();
		itempawn_loop();
		draw_menu();

		// End Frame
		ImGui::EndFrame();

		// Render Frame
		ImGui::Render();
		const ImVec4 clear_color = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		float render_color_rgba_array[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
		overlay->D3D11DeviceContext->OMSetRenderTargets(1, &overlay->MainRenderTargetView, nullptr);
		overlay->D3D11DeviceContext->ClearRenderTargetView(overlay->MainRenderTargetView, render_color_rgba_array);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		overlay->pSwapChain->Present(misc->VSync, 0);
	}

	return FALSE;
}
#endif // RENDER_HPP
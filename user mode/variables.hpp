#pragma once
#include <iostream>
#include "imgui/imgui.h"
#ifndef SETTINGS_HPP
#define SETTINGS_HPP
ImFont* ESPFont;
ImFont* bold_segu;
ImFont* titleicon;
ImFont* subtitle;

struct aimbot_
{
	bool bAimbot = true;
	bool bSilentAimbot = false;
	bool bMemoryAim = false;
	bool waypoint = false;
	bool prediction = true;
	bool humanization = true;
	bool triggerbot = false;
	int triggerSpeed = 1.f;
	int AimbotSlow = 0;
	float AimDistance = 100.f;
	float fov = 200.f;
	int smoothness = 6;

	bool Aimline = false;
	bool bWeapon = true;
	bool Backtrack = false;
	bool bOnlyMale = false;
	bool crosshair = true;
	bool showfov = true;

	bool LMouse = false,
		RMouse = false,
		LShift = false,
		LCtrl = false;

	bool Head = false, 
		Neck = false, 
		Chest = false, 
		Pelvis = false,
		Rand = false, 
		Closest = false;
}; std::unique_ptr<aimbot_> aimbot = std::make_unique<aimbot_>();

struct visuals_
{
	bool draw_radar = false;
	bool console = false;
	bool bViewAngles = false;
	bool b2Dhead = false;
	bool b3Dhead = false;
	bool name = false;
	bool box = false;
	bool cornered_box = false;
	bool threed = false;
	bool distance = false;
	bool PlayerDebug = false;
	bool WeaponESP = false;
	bool line = false;
	bool gayskeleton = false;
	bool CurrWeapon = false;
	bool outline = false;
	bool fill = false;
	bool chams = false;
	float MaxDistance = 100.f;
	float size = 18.0f;

	struct color_
	{
		float PlayerboxNotVisible[3] = { 1.0f , 1.0f , 1.0f };
		float TeamboxNotVisible[3] = { 1.0f , 1.0f , 1.0f };
		float SkeletonNotVisible[3] = { 1.0f , 1.0f , 1.0f };
		float SnaplinesNotVisible[3] = { 1.0f , 1.0f , 1.0f };

		float PlayerboxVisible[3] = { 1.0f , 1.0f , 1.0f };
		float TeamboxVisible[3] = { 1.0f , 1.0f , 1.0f };
		float SkeletonVisible[3] = { 1.0f , 1.0f , 1.0f };
		float SnaplinesVisible[3] = { 1.0f , 1.0f , 1.0f };
	}; std::unique_ptr<color_> color = std::make_unique<color_>();
	struct loot_
	{
		bool loot = false;
		bool pickups = false;
		bool chests = false;
		bool ammo = false;
		bool supply = false;
		bool vehicle = false;
		bool animal = false;
		bool generades = false;
		bool trap = false;
		bool rift = false;
		bool weakspot = false;
		float MaxItemDistance = 100.f;
	}; std::unique_ptr<loot_> loot = std::make_unique<loot_>();
}; std::unique_ptr<visuals_> visual = std::make_unique<visuals_>();

struct exploits_
{
	bool cheat_flying = false;
	bool fire_rate = false; bool fire_rate_old = false;
	bool no_spread = false; bool no_spread_old = false;
	bool instant_reload = false; bool instant_reload_old = false; // thanks vanta for the POC
	bool no_clip = false; bool no_clip_init = false;
	bool no_over_heat = false; bool no_over_heat_old = false;
	bool ads_in_air = false; bool ads_in_air_old = false;
	bool no_recoil = false;
	bool magic_bullet = false;
}; std::unique_ptr<exploits_> exploits = std::make_unique<exploits_>();

struct misc_
{
	bool StreamProof = false;
	bool VSync = false;
	bool ExitThread = false;
	bool PreformenceMode = false;
	bool bIgnoreDead = false;
	bool bIgnoreHidden = false;
	bool bIgnoreTeam = false;
	bool bIgnoreBots = false;
	bool bIgnoreAFK = false;
	bool bIgnorePickedup = false;
	bool bIgnoreDespawned = false;
	bool bIgnoreWaterLoot = false;
	bool bIgnoreAirLoot = false;
}; std::unique_ptr<misc_> misc = std::make_unique<misc_>();
#endif // SETTINGS_HPP
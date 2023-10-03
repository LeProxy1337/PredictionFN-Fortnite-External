#pragma once
#include <d3d9.h>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <mutex>
#include "comm/driver.hpp"
#include "structs.hpp"
#include "hexray.hpp"
#include "variables.hpp"
#ifndef SDK_HPP
#define SDK_HPP

inline void debug_pointer(uintptr_t p, const char* s) {
	printf(("%s %I64u \n"), s, p);
}

namespace offsets
{
	FortStorage Gworld = 0xEE2F8E8;
	FortStorage ViewPoint = 0xED57310;
	FortStorage OwningGameInstance = 0x1B8;
	FortStorage PlayerController = 0x30;
	FortStorage AcknowledgedPawn = 0x338;
	FortStorage PlayerCameraManager = 0x348;
	FortStorage LastFrameCameraCachePrivate = 0x2A80;
	FortStorage DefaultFOV = 0x2A4;
	FortStorage ControlRotation = 0x300;
	FortStorage MyHUD = 0x340;
	FortStorage Canvas = 0x2E0;
	FortStorage SizeX = 0x40;
	//FortStorage SizeY = 0x44;
	FortStorage WorldSettings = 0x2A0;
	FortStorage PlayerState = 0x2B0;
	FortStorage PlayerArray = 0x2A0;
	FortStorage RootComponent = 0x198;
	FortStorage CurrentVehicle = 0x2590;
	FortStorage RelativeLocation = 0x128;
	FortStorage CurrentWeapon = 0x948;
	FortStorage WeaponData = 0x450;
	FortStorage Mesh = 0x318;
	FortStorage TeamID = 0x10D0;
	FortStorage GameStates = 0x158;
	FortStorage RelativeRotation = 0x140;
	FortStorage InputYawScale = 0x538;
	FortStorage InputPitchScale = 0x53C;
	FortStorage ComponentVelocity = 0x170;
	FortStorage RelativeScale3D = 0x158;
	FortStorage TargetedBuild = 0x1840;
	FortStorage DisplayName = 0x98;
	FortStorage Length = 0x40;
	FortStorage levels = 0x170;
	FortStorage PrimaryPickupItemEntry = 0x330;
	FortStorage ItemDefinition = 0x18;
	FortStorage HUDScale = 0x348;
}

class camera_ {
public:
	FVector Location;
	FVector Rotation;
	float FieldOfView;
	char Useless[0x18];
}; camera_ camera;

camera_ get_camera();
std::string FNamePool(int key);

class UObject
{
public:
	explicit UObject(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	const std::uint64_t GetAddress() { return m_addr; }

	const bool IsValidClass() { return m_addr > 0; };

	int64_t GetClassId(uintptr_t targetedObject)
	{
		uintptr_t targetedClass = driver::read<uintptr_t>(targetedObject + 0x10);
		int64_t targetedId = driver::read<int64_t>(targetedClass + 0x18);
		return targetedId;
	}

	BOOL IsA(uintptr_t targetedObject, int64_t classId) const
	{
		uintptr_t targetedClass = driver::read<uintptr_t>(targetedObject + 0x10);
		int64_t targetedId = driver::read<int64_t>(targetedClass + 0x18);
		if (targetedId == classId)
			return true;
		return false;
	}
private:
	std::uint64_t m_addr = 0;
};

class USceneComponent
{
public:
	explicit USceneComponent(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	const std::uint64_t GetAddress() { return m_addr; }

	const bool IsValidClass() { return m_addr > 0; };

	FVector ComponentVelocity()
	{
		return driver::read<FVector>(m_addr + offsets::ComponentVelocity);
	}

	FVector RelativeLocation()
	{
		return driver::read<FVector>(m_addr + offsets::RelativeLocation);
	}

	FVector RelativeRotation()
	{
		return driver::read<FVector>(m_addr + offsets::RelativeRotation);
	}

	FVector RelativeScale3D()
	{
		return driver::read<FVector>(m_addr + offsets::RelativeScale3D);
	}
private:
	std::uint64_t m_addr = 0;
};

class UCanvas {
public:
	explicit UCanvas(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	const std::uint64_t GetAddress() { return m_addr; }

	const bool IsValidClass() { return m_addr > 0; };

	int SizeX()
	{
		return driver::read<int>(m_addr + offsets::SizeX);
	}

	int SizeY() 
	{
		return driver::read<int>(m_addr + offsets::SizeX + sizeof(int));
	}
private:
	std::uint64_t m_addr = 0;
};

class UFortClientSettingsRecord {
public:
	explicit UFortClientSettingsRecord(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	const std::uint64_t GetAddress() { return m_addr; }

	const bool IsValidClass() { return m_addr > 0; };

	float HUDScale() const
	{
		return driver::read<float>(m_addr + offsets::HUDScale);
	}
private:
	std::uint64_t m_addr = 0;
};

class AHUD {
public:
	explicit AHUD(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	std::uint64_t GetAddress() { return m_addr; }

	bool IsValidClass() { return m_addr > 0; };

	struct UCanvas Canvas()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + offsets::Canvas);
		return struct UCanvas(pointer);
	}
private:
	std::uint64_t m_addr = 0;
};

class APlayerState
{
public:
	explicit APlayerState(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	std::uint64_t GetAddress() { return m_addr; }

	bool IsValidClass() { return m_addr > 0; };

	int TeamIndex() const
	{
		return driver::read<int>(m_addr + offsets::TeamID);
	}

	std::string GetPlatformName() const
	{
		uintptr_t pNameStructure = driver::read<uintptr_t>(m_addr + 0x430);
		auto pNameLength = driver::read<int>(pNameStructure + 0x10);
		if (pNameLength <= 0) return "";

		wchar_t* pNameBuffer = new wchar_t[pNameLength];
		driver::read_array(pNameStructure, &pNameBuffer, pNameLength * sizeof(wchar_t));
		std::wstring temp_wstring(pNameBuffer);
		return std::string(temp_wstring.begin(), temp_wstring.end());
	}

	std::string GetPlayerName() const
	{
		int pNameLength; // rsi
		char v21; // al
		int v22; // r8d
		int i; // ecx
		int v25; // eax
		_WORD* v23;

		__int64 pNameStructure = driver::read<__int64>(m_addr + 0xAD8); // 0xAC8
		pNameLength = driver::read<int>(pNameStructure + 16);
		__int64 v6 = pNameLength;
		if (!v6) return std::string("bot");

		wchar_t* pNameBuffer = new wchar_t[pNameLength];
		uintptr_t pNameEncryptedBuffer = driver::read<__int64>(pNameStructure + 8);
		driver::read_array(pNameEncryptedBuffer, &pNameBuffer, pNameLength * sizeof(wchar_t));

		v21 = v6 - 1;
		if (!(_DWORD)v6)
			v21 = 0;
		v22 = 0;
		v23 = (_WORD*)pNameBuffer;

		for (i = (v21) & 3; ; *v23++ += i & 7)
		{
			v25 = v6 - 1;
			if (!(_DWORD)v6)
				v25 = 0;

			if (v22 >= v25)
				break;

			i += 3;
			++v22;
		}

		std::wstring Temp{ pNameBuffer };
		return std::string(Temp.begin(), Temp.end());
	}
private:
	std::uint64_t m_addr = 0;
};

class APlayerCameraManager 
{
public:
	explicit APlayerCameraManager(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	std::uint64_t GetAddress() { return m_addr; }

	bool IsValidClass() { return m_addr > 0; };

	camera_ LastFrameCameraCachePrivate() const
	{
		return driver::read<camera_>(m_addr + offsets::LastFrameCameraCachePrivate + 0x10);
	}

	float DefaultFOV() const 
	{
		return driver::read<float>(m_addr + offsets::DefaultFOV);
	}
private:
	std::uint64_t m_addr = 0;
};

class UFortWeaponItemDefinition {
public:
	explicit UFortWeaponItemDefinition(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	std::uint64_t GetAddress() { return m_addr; }

	bool IsValidClass() { return m_addr > 0; };

	// empty
private:
	std::uint64_t m_addr = 0;
};

class AFortWeapon
{
public:
	explicit AFortWeapon(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	std::uint64_t GetAddress() { return m_addr; }

	bool IsValidClass() { return m_addr > 0; };

	ImColor CurrentReticleColor() const 
	{
		return driver::read<bool>(m_addr + 0x62C);
	}

	struct UFortWeaponItemDefinition WeaponData()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + offsets::WeaponData);
		return struct UFortWeaponItemDefinition(pointer);
	}

	float GetProjectileSpeed() const
	{
		return driver::read<float>(m_addr + 0x174); // 0x9C0
	}

	int GetAmmoCount() const
	{
		return driver::read<int32>(m_addr + 0xCFC);
	}

	bool IsReloadingWeapon() const
	{
		return driver::read<bool>(m_addr + 0x358);
	}

	std::string GetWeaponName() const 
	{
		uintptr_t itemdef = driver::read<uintptr_t>(m_addr + offsets::WeaponData);
		if (!itemdef) return "";
		uintptr_t DisplayName = driver::read<uintptr_t>(itemdef + offsets::DisplayName);
		if (!DisplayName) return "";
		uintptr_t WeaponLength = driver::read<uint32_t>(DisplayName + offsets::Length);
		wchar_t* WeaponName = new wchar_t[uintptr_t(WeaponLength) + 1];

		driver::read_array((ULONG64)driver::read<PVOID>(DisplayName + 0x38), WeaponName, WeaponLength * sizeof(wchar_t));

		std::wstring wWeaponName(WeaponName);
		return std::string(wWeaponName.begin(), wWeaponName.end());
	}
private:
	std::uint64_t m_addr = 0;
};

class AFortAthenaVehicle {
public:
	explicit AFortAthenaVehicle(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	std::uint64_t GetAddress() { return m_addr; }

	bool IsValidClass() { return m_addr > 0; };

	bool CurrentTeam(AFortAthenaVehicle B)  const
	{
		uint8 EnemyTeamId = driver::read<uint8>(B.GetAddress() + 0x6D1);
		uint8 TeamId = driver::read<uint8>(m_addr + 0x6D1);
		if (EnemyTeamId == TeamId)
			return true;
		return false;
	}

	EVehicleSurface GetCurrentSurface() const
	{
		bool bOnRoad = driver::read<bool>(m_addr + 0x1968);
		bool bOnLandscape = driver::read<bool>(m_addr + 0x1969);
		bool bOnDirt = driver::read<bool>(m_addr + 0x196A);
		bool bOnGrass = driver::read<bool>(m_addr + 0x196B);
		bool bOnIce = driver::read<bool>(m_addr + 0x196C);
		bool bOnSnow = driver::read<bool>(m_addr + 0x196D);
		bool bOnMud = driver::read<bool>(m_addr + 0x196E);
		bool bOnVehicle = driver::read<bool>(m_addr + 0x196F);

		if (bOnRoad)
			return EVehicleSurface::bOnRoad;
		else if (bOnDirt)
			return EVehicleSurface::bOnDirt;
		else if (bOnGrass)
			return EVehicleSurface::bOnGrass;
		else if (bOnIce)
			return EVehicleSurface::bOnIce;
		else if (bOnSnow)
			return EVehicleSurface::bOnSnow;
		else if (bOnMud)
			return EVehicleSurface::bOnMud;
		else if (bOnVehicle)
			return EVehicleSurface::bOnVehicle;
		else if (bOnLandscape)
			return EVehicleSurface::bOnLandscape;
	}

	ETireSurfaces GetCurrentTireSurface() const
	{
		return (ETireSurfaces)driver::read<uint8>(m_addr + 0x1970);
	}

	ETireStates TireState() const
	{
		return (ETireStates)driver::read<int>(m_addr + 0x1998);
	}

	float GetCurrentFOV() const 
	{
		return driver::read<float>(m_addr + 0x764);
	}

	bool CarBoosting() const
	{
		bool PendingJumpCharge = driver::read<bool>(m_addr + 0x1B58);
		float JumpCooldownRemaining = driver::read<float>(m_addr + 0x1B4C);
		if (PendingJumpCharge || JumpCooldownRemaining != 0.f)
			return false;

		return driver::read<bool>(m_addr + 0x18A4);
	}

	float WaterLevel() const
	{
		if (this->GetCurrentTireSurface() == ETireSurfaces::Water)
			return driver::read<float>(m_addr + 0xCB4);
		return 0.f;
	}

	float VechileSpeed() const
	{
		return driver::read<float>(m_addr + 0xCB4);
	}
private:
	std::uint64_t m_addr = 0;
};

class USkeletalMeshComponent
{
public:
	explicit USkeletalMeshComponent(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	std::uint64_t GetAddress() { return m_addr; }

	bool IsValidClass() { return m_addr > 0; };

	FVector GetBone(int Bone)
	{
		int is_cached = driver::read<int>(m_addr + 0x648);
		auto bone_transform = driver::read<FTransform>(driver::read<uintptr_t>(m_addr + 0x10 * is_cached + 0x600) + 0x60 * Bone);

		FTransform ComponentToWorld = driver::read<FTransform>(m_addr + 0x240);

		D3DMATRIX Matrix;
		Matrix = MatrixMultiplication(bone_transform.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());

		return FVector(Matrix._41, Matrix._42, Matrix._43);
	}

	bool WasRecentlyRendered(float Tolerance = 0.06f) const // 0.015f
	{
		auto fLastSubmitTime = driver::read<float>(m_addr + 0x360);
		auto fLastRenderTimeOnScreen = driver::read<float>(m_addr + 0x368);
		return fLastRenderTimeOnScreen + Tolerance >= fLastSubmitTime;
	}
private:
	std::uint64_t m_addr = 0;
};

class AActor : public UObject
{
public:
	explicit AActor(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	std::uint64_t GetAddress() const { return m_addr; }

	bool IsValidClass() const { return m_addr > 0; };

	std::unordered_map<std::string, int> ActorFName() const
	{
		std::unordered_map<std::string, int> HashedFName = { { FNamePool(driver::read<INT32>(m_addr + 0x18)), 1 } };
		return HashedFName;
	}

	struct APlayerState PlayerState()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + offsets::PlayerState);
		return struct APlayerState(pointer);
	}

	struct USkeletalMeshComponent Mesh()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + offsets::Mesh);
		return struct USkeletalMeshComponent(pointer);
	}

	struct USceneComponent RootComponent()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + offsets::RootComponent);
		return struct USceneComponent(pointer);
	}

	double pawn_distance(FVector To) const
	{
		return (camera.Location.Distance(To) / 100.0);
	}
private:
	std::uint64_t m_addr = 0;
};

class AFortPawn : public AActor
{
public:
	explicit AFortPawn(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	std::uint64_t GetAddress() { return m_addr; }

	bool IsValidClass() { return m_addr > 0; };

	struct AFortAthenaVehicle CurrentVehicle()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + offsets::CurrentVehicle);
		return struct AFortAthenaVehicle(pointer);
	}

	struct AFortWeapon CurrentWeapon()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + offsets::CurrentWeapon);
		return struct AFortWeapon(pointer);
	}

	struct APlayerState PlayerState()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + offsets::PlayerState);
		return struct APlayerState(pointer);
	}

	bool IsInWater()
	{
		if (CurrentVehicle().GetCurrentTireSurface() == ETireSurfaces::Water || CurrentVehicle().WaterLevel() != 0.f)
			return true;
		return driver::read<bool>(m_addr + 0x46E0);
	}
private:
	std::uint64_t m_addr = 0;
};

class APlayerController
{
public:
	explicit APlayerController(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	std::uint64_t GetAddress() { return m_addr; }

	bool IsValidClass() { return m_addr > 0; };

	struct AFortPawn AcknowledgedPawn()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + offsets::AcknowledgedPawn);
		return struct AFortPawn(pointer);
	}

	struct APlayerCameraManager PlayerCameraManager()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + offsets::PlayerCameraManager);
		return struct APlayerCameraManager(pointer);
	}

	struct AHUD MyHUD()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + offsets::MyHUD);
		return struct AHUD(pointer);
	}

	uintptr_t ControlRotation() 
	{
		return driver::read<uintptr_t>(m_addr + offsets::ControlRotation);
	}

	uintptr_t TargetedBuild()
	{
		return driver::read<uintptr_t>(m_addr + offsets::TargetedBuild);
	}

	float InputPitchScale() const
	{
		return driver::read<float>(m_addr + offsets::InputYawScale);
	}

	float InputYawScale() const
	{
		return driver::read<float>(m_addr + offsets::InputPitchScale);
	}
private:
	std::uint64_t m_addr = 0;
};

class ULocalPlayer {
public:
	explicit ULocalPlayer(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	std::uint64_t GetAddress() { return m_addr; }

	bool IsValidClass() { return m_addr > 0; };

	FVector2D W2S(FVector WorldLocation) const
	{
		D3DMATRIX tempMatrix = Matrix(camera.Rotation);

		FVector vAxisX = FVector(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
		FVector vAxisY = FVector(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
		FVector vAxisZ = FVector(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);

		FVector vDelta = WorldLocation - camera.Location;
		FVector vTransformed = FVector(vDelta.Dot(vAxisY), vDelta.Dot(vAxisZ), vDelta.Dot(vAxisX));

		if (vTransformed.z < 1.f)
			vTransformed.z = 1.f;

		return FVector2D(center_x + vTransformed.x * ((center_x / tanf(camera.FieldOfView * (float)M_PI / 360.f))) / vTransformed.z, center_y - vTransformed.y * ((center_x / tanf(camera.FieldOfView * (float)M_PI / 360.f))) / vTransformed.z);
	}

	struct APlayerController PlayerController()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + 0x30);
		return struct APlayerController(pointer);
	}

	struct UFortClientSettingsRecord ClientSettingsRecord()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + 0x390);
		return struct UFortClientSettingsRecord(pointer);
	}
private:
	std::uint64_t m_addr = 0;
};

class ULocalPlayers
{
public:
	explicit ULocalPlayers(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	std::uint64_t GetAddress() { return m_addr; }

	bool IsValidClass() { return m_addr > 0; };

	struct ULocalPlayer LocalPlayer()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr);
		return struct ULocalPlayer(pointer);
	}
private:
	std::uint64_t m_addr = 0;
};

class ULevel
{
public:
	explicit ULevel(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	const std::uint64_t GetAddress() { return m_addr; }

	const bool IsValidClass() { return m_addr > 0; };

	uintptr_t ActorArray()
	{
		return driver::read<uintptr_t>(m_addr + 0x98);
	}

	int ActorCount() const
	{
		return driver::read<int>(m_addr + 0xA0);
	}
private:
	std::uint64_t m_addr = 0;
};

class UGameInstance 
{
public:
	explicit UGameInstance(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	std::uint64_t GetAddress() { return m_addr; }

	bool IsValidClass() { return m_addr > 0; };

	struct ULocalPlayers LocalPlayers()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + 0x38);
		return struct ULocalPlayers(pointer);
	}
private:
	std::uint64_t m_addr = 0;
};

class UWorld : UObject
{
public:
	explicit UWorld(std::uint64_t p_addr = 0) : m_addr(p_addr) {}

	std::uint64_t GetAddress() { return m_addr; }

	bool IsValidClass() { return m_addr > 0; };

	static struct UWorld GetWorld()
	{
		uintptr_t pointer = driver::read<uintptr_t>(module::image_base + offsets::Gworld);
		return struct UWorld(pointer);
	}

	struct UGameInstance GameInstance()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + offsets::OwningGameInstance);
		return struct UGameInstance(pointer);
	}

	struct ULevel PersistentLevel()
	{
		uintptr_t pointer = driver::read<uintptr_t>(m_addr + 0x30);
		return struct ULevel(pointer);
	}

	FVector GetCameraLocation()
	{
		return driver::read<FVector>(driver::read<uintptr_t>(m_addr + 0x110));
	}
private:
	std::uint64_t m_addr = 0;
};

namespace global
{
	UWorld GWorld;
	ULevel PersistentLevel;
	ULocalPlayer LocalPlayer;
	APlayerController PlayerController;
	AFortPawn LocalPawn;
	std::vector<AActor> Player;
	std::vector<AActor> Item;
	std::mutex data_mutex;
	int Players;
	int Items;
}

void cache_actors() 
{
	using namespace std::chrono;
	std::vector<AActor> temp_array;
	std::thread(get_camera).detach();

	while (true)
	{
		temp_array.reserve(global::Players);
		global::GWorld = UWorld::GetWorld();
		debug_pointer(global::GWorld.GetAddress(), "GWorld");
		if (global::GWorld.IsValidClass()) continue;
		global::PersistentLevel = global::GWorld.PersistentLevel();
		debug_pointer(global::PersistentLevel.GetAddress(), "PersistentLevel");
		if (global::PersistentLevel.IsValidClass()) continue;
		UGameInstance Gameinstance = global::GWorld.GameInstance();
		debug_pointer(Gameinstance.GetAddress(), "GameInstance");
		if (Gameinstance.IsValidClass()) continue;
		global::LocalPlayer = Gameinstance.LocalPlayers().LocalPlayer();
		debug_pointer(global::LocalPlayer.GetAddress(), "LocalPlayer");
		if (global::LocalPlayer.IsValidClass()) continue;
		global::PlayerController = global::LocalPlayer.PlayerController();
		debug_pointer(global::PlayerController.GetAddress(), "PlayerController");
		if (global::PlayerController.IsValidClass()) continue;
		global::LocalPawn = global::PlayerController.AcknowledgedPawn();
		debug_pointer(global::LocalPawn.GetAddress(), "AcknowledgedPawn");
		if (global::LocalPawn.IsValidClass()) continue;

		for (global::Players = 0; global::Players < global::PersistentLevel.ActorCount(); global::Players++)
		{
			AActor pAActors = driver::read<AActor>(global::PersistentLevel.ActorArray() + global::Players * sizeof(uintptr_t));
			debug_pointer(pAActors.GetAddress(), "PlayerPawn");
			if (pAActors.IsValidClass() && pAActors.GetAddress() == global::LocalPawn.GetAddress())
				temp_array.push_back(pAActors);
		}

		if (UCanvas Canvas = global::PlayerController.MyHUD().Canvas(); Canvas.IsValidClass())
		{
			Width = Canvas.SizeX(); center_x = Width / 2;
			Height = Canvas.SizeY(); center_y = Height / 2;
		}

		std::unique_lock<std::mutex> lock(global::data_mutex);
		global::Player = std::move(temp_array);
		lock.unlock();

		if (misc->PreformenceMode)
			std::this_thread::sleep_for(milliseconds(11));
		std::this_thread::sleep_for(milliseconds(9));
	}

}

void cache_levels()
{
	using namespace std::chrono;
	std::vector<AActor> temp_array;
	temp_array.reserve(global::Items);

	while (true)
	{
		if (visual->loot)
		{
			temp_array.clear();
			uintptr_t ulevel_array = driver::read<uintptr_t>(global::GWorld.GetAddress() + 0x170); // iterating uworld ulevels
			debug_pointer(ulevel_array, "ulevel array");
			for (int OwningWorld = 0; OwningWorld < driver::read<int>(global::GWorld.GetAddress() + (0x170 + sizeof(uintptr_t))); ++OwningWorld) { // running through levels and 0x8
				if (OwningWorld >= driver::read<int>(global::GWorld.GetAddress() + 0x178))
					break;

				ULevel PersistentLevel = driver::read<ULevel>(ulevel_array + sizeof(uintptr_t) * OwningWorld);
				debug_pointer(PersistentLevel.GetAddress(), "Levels PersistentLevel");
				if (PersistentLevel.IsValidClass())
					continue;
				for (int AActors = 0; AActors < driver::read<int>(ulevel_array + (PersistentLevel.ActorArray() + sizeof(uintptr_t))); ++AActors) { // running through levels actor array and 0x8
					if (AActors <= PersistentLevel.ActorCount())
						break;

					global::Items = PersistentLevel.ActorCount();
					AActor pAActors = driver::read<AActor>(PersistentLevel.ActorArray() + sizeof(uintptr_t) * AActors); // AActor, Inherits AFortPickup and ABuildingContainer
					debug_pointer(pAActors.GetAddress(), "Levels AActors");
					if (pAActors.IsValidClass())
						temp_array.push_back(pAActors);
				}
			}

			std::unique_lock<std::mutex> lock(global::data_mutex);
			global::Player = std::move(temp_array);
			lock.unlock();

			if (misc->PreformenceMode)
				std::this_thread::sleep_for(milliseconds(11));
			std::this_thread::sleep_for(milliseconds(9));
		}
	}
}

camera_ get_camera()
{
	char v1; // r8
	camera = driver::read<camera_>(module::image_base + offsets::ViewPoint);
	BYTE* v2 = (BYTE*)&camera;
	int i; // edx
	__int64 result; // rax

	v1 = 0x40;
	for (i = 0; i < 0x40; ++i)
	{
		*v2 ^= v1;
		result = (unsigned int)(i + 0x17);
		v1 += i + 0x17;
		v2++;
	}

	return camera;
}

std::string FName(int key)
{
	uint32_t ChunkOffset = (uint32_t)((int)(key) >> 16);
	uint16_t NameOffset = (uint16_t)key;

	uint64_t NamePoolChunk = driver::read<uint64_t>(module::image_base + 0xeeb2c80 + (8 * ChunkOffset) + 16) + (unsigned int)(4 * NameOffset); //((ChunkOffset + 2) * 8) ERROR_NAME_SIZE_EXCEEDED
	uint16_t nameEntry = driver::read<uint16_t>(NamePoolChunk);

	int nameLength = nameEntry >> 6;
	char buff[1024];
	if ((uint32_t)nameLength)
	{
		for (int x = 0; x < nameLength; ++x)
		{
			buff[x] = driver::read<char>(NamePoolChunk + 4 + x);
		}
		char* v2 = buff; // rbx
		unsigned int v4 = nameLength;
		unsigned int v5; // eax
		__int64 result; // rax
		int v7; // ecx
		unsigned int v8; // kr00_4
		__int64 v9; // ecx

		v5 = 0;
		result = driver::read<unsigned int>(module::image_base + 0x569fc70) >> 5;
		if (v4)
		{
			do
			{
				v7 = *v2++;
				v8 = result ^ (16 * v7) ^ (result ^ (v7 >> 4)) & 0xF;
				result = (unsigned int)(result + 4 * v5++);
				*(v2 - 1) = v8;
			} while (v5 < v4);
		}
		buff[nameLength] = '\0';
		return std::string(buff);
	}

	return "";
}

std::string FNamePool(int key)
{
	uint32_t ChunkOffset = (uint32_t)((int)(key) >> 16);
	uint16_t NameOffset = (uint16_t)key;

	uint64_t NamePoolChunk = driver::read<uint64_t>(module::image_base + 0xeeb2c80 + (8 * ChunkOffset) + 16) + (unsigned int)(4 * NameOffset); //((ChunkOffset + 2) * 8) ERROR_NAME_SIZE_EXCEEDED //gname
	if (driver::read<uint16_t>(NamePoolChunk) < 64)
	{
		auto a1 = driver::read<DWORD>(NamePoolChunk + 4);
		return FName(a1);
	}
	else
	{
		return FName(key);
	}
}
#endif // SDK_HPP
#include <corecrt_math.h>
#include <d3d9.h>
#include "hexray.hpp"
#include <dwmapi.h>
#ifndef STRUCTS_H
#define STRUCTS_H

#define M_PI 3.14159265358979323846264338327950288419716939937510

struct FColor
{
	double R = 0;
	double G = 0;
	double B = 0;
	double A = 0;
};

struct FLinearColor
{
	float R;
	float G;
	float B;
	float A;
};

enum EFortWeaponTriggerType : uint8
{
	OnPress = 0,
	Automatic = 1,
	OnRelease = 2,
	OnPressAndRelease = 3,
	Custom = 4,
	EFortWeaponTriggerType_MAX = 5,
};

enum EMovementMode : uint8
{
	MOVE_None = 0,
	MOVE_Walking = 1,
	MOVE_NavWalking = 2,
	MOVE_Falling = 3,
	MOVE_Swimming = 4,
	MOVE_Flying = 5,
	MOVE_Custom = 6,
	MOVE_MAX = 7,
};

enum ETireSurfaces : uint8
{
	Road = 0,
	Dirt = 1,
	Grass = 2,
	Air = 3,
	Water = 4,
	ETireSurfaces_MAX = 5,
};

enum ETireStates : uint8
{
	Default = 0,
	Popped = 1,
	ETireStates_MAX = 2,
};

enum EVehicleSurface : uint8
{
	bOnRoad = 0,
	bOnLandscape = 1,
	bOnDirt = 2,
	bOnGrass = 3,
	bOnIce = 4,
	bOnSnow = 5,
	bOnMud = 6,
	bOnVehicle = 7,
};

enum EFortRarity : uint8
{
	Common = 0,
	Uncommon = 1,
	Rare = 2,
	Epic = 3,
	Legendary = 4,
	Mythic = 5,
	Transcendent = 6,
	Unattainable = 7,
	NumRarityValues = 8,
	EFortRarity_MAX = 9,
};

enum EFortWeaponOverheatState : uint8
{
	None = 0,
	Heating = 1,
	Cooling = 2,
	Overheated = 3,
	EFortWeaponOverheatState_MAX = 4,
};

struct AFortPlayer
{
	float Score;
	int32 PlayerId;
	uint8_t CompressedPing;
	bool bIsEquippingWeapon;
	bool bIsChargingWeapon;
	bool bIsTargeting;
	float TimeToEquip;
	float LastReloadTime;
	float LastSuccessfulReloadTime;
	float CurrentReloadDuration;
	int32 WeaponLevel;
	int32 PhantomReserveAmmo;
	int32 BurstFireCounter;
	float ChargeTime;
	float AccumulatedChargeTime;
	float LastChargeTime;
	bool bIsSliding;
	bool bWeaponHolstered;
	bool bMovingEmote;
	EFortWeaponTriggerType TriggerType;
	EMovementMode MovementMode;
	bool IsValid = false;
}; AFortPlayer* Player;

class FVector
{

public:
	FVector() : x(0.f), y(0.f), z(0.f)
	{

	}

	FVector(float _x, float _y, float _z) : x(_x), y(_y), z(_z)
	{

	}
	~FVector()
	{

	}

	double x;
	double y;
	double z;

	inline float Dot(FVector v)
	{
		return x * v.x + y * v.y + z * v.z;
	}

	inline double Distance(FVector v)
	{
		return double(sqrtf(powf(v.x - x, 2.0) + powf(v.y - y, 2.0) + powf(v.z - z, 2.0)));
	}

	inline bool is_valid(FVector v)
	{
		return (x != 0 && y != 0 && z != 0);
	}

	FVector operator+(FVector v)
	{
		return FVector(x + v.x, y + v.y, z + v.z);
	}

	FVector operator-(FVector v)
	{
		return FVector(x - v.x, y - v.y, z - v.z);
	}

	FVector operator*(float number) const
	{
		return FVector(x * number, y * number, z * number);
	}

};

class FVector2D
{
public:
	FVector2D() : x(0.f), y(0.f)
	{

	}

	FVector2D(double _x, double _y) : x(_x), y(_y)
	{

	}
	~FVector2D()
	{

	}

	double x;
	double y;

	inline bool valid_location() {
		if (x > Width || y > Height)
			return false;
		else if (x <= 0 || y <= 0)
			return false;
		else if (std::isnan(x) || std::isnan(y))
			return false;

		return true;
	}

	FVector2D operator+(FVector2D v)
	{
		return FVector2D(x + v.x, y + v.y);
	}

};

class FRotator
{
public:
	FRotator() : Pitch(0.f), Yaw(0.f), Roll(0.f)
	{

	}

	FRotator(double _Pitch, double _Yaw, double _Roll) : Pitch(_Pitch), Yaw(_Yaw), Roll(_Roll)
	{

	}
	~FRotator()
	{

	}

	double Pitch;
	double Yaw;
	double Roll;

};

template<class T>
class TArray
{
protected:
	T* Data;
	int32 NumElements;
	int32 MaxElements;

public:

	inline T& operator[](uint32 Index)
	{
		return Data[Index];
	}

	inline const T& operator[](uint32 Index) const
	{
		return Data[Index];
	}

	inline int32 Num()
	{
		return NumElements;
	}

	inline int32 Max()
	{
		return MaxElements;
	}

	inline int32 GetSlack()
	{
		return MaxElements - NumElements;
	}

	inline bool IsValid()
	{
		return Data != nullptr;
	}

	inline bool IsValidIndex(int32 Index)
	{
		return Index >= 0 && Index < NumElements;
	}

	inline void ResetNum()
	{
		NumElements = 0;
	}
};

struct FQuat
{
	float x;
	float y;
	float z;
	float w;
};

struct FTransform
{
	FQuat rot;
	FVector translation;
	char pad[4];
	FVector scale;
	char pad1[4];
	D3DMATRIX ToMatrixWithScale()
	{
		D3DMATRIX m{};
		m._41 = translation.x;
		m._42 = translation.y;
		m._43 = translation.z;
		float x2 = rot.x + rot.x;
		float y2 = rot.y + rot.y;
		float z2 = rot.z + rot.z;
		float xx2 = rot.x * x2;
		float yy2 = rot.y * y2;
		float zz2 = rot.z * z2;
		m._11 = (1.0f - (yy2 + zz2)) * scale.x;
		m._22 = (1.0f - (xx2 + zz2)) * scale.y;
		m._33 = (1.0f - (xx2 + yy2)) * scale.y;
		float yz2 = rot.y * z2;
		float wx2 = rot.w * x2;
		m._32 = (yz2 - wx2) * scale.z;
		m._23 = (yz2 + wx2) * scale.y;
		float xy2 = rot.x * y2;
		float wz2 = rot.w * z2;
		m._21 = (xy2 - wz2) * scale.y;
		m._12 = (xy2 + wz2) * scale.x;
		float xz2 = rot.x * z2;
		float wy2 = rot.w * y2;
		m._31 = (xz2 + wy2) * scale.z;
		m._13 = (xz2 - wy2) * scale.x;
		m._14 = 0.0f;
		m._24 = 0.0f;
		m._34 = 0.0f;
		m._44 = 1.0f;
		return m;
	}
};

D3DMATRIX MatrixMultiplication(D3DMATRIX pM1, D3DMATRIX pM2)
{
	D3DMATRIX pOut{};
	pOut._11 = pM1._11 * pM2._11 + pM1._12 * pM2._21 + pM1._13 * pM2._31 + pM1._14 * pM2._41;
	pOut._12 = pM1._11 * pM2._12 + pM1._12 * pM2._22 + pM1._13 * pM2._32 + pM1._14 * pM2._42;
	pOut._13 = pM1._11 * pM2._13 + pM1._12 * pM2._23 + pM1._13 * pM2._33 + pM1._14 * pM2._43;
	pOut._14 = pM1._11 * pM2._14 + pM1._12 * pM2._24 + pM1._13 * pM2._34 + pM1._14 * pM2._44;
	pOut._21 = pM1._21 * pM2._11 + pM1._22 * pM2._21 + pM1._23 * pM2._31 + pM1._24 * pM2._41;
	pOut._22 = pM1._21 * pM2._12 + pM1._22 * pM2._22 + pM1._23 * pM2._32 + pM1._24 * pM2._42;
	pOut._23 = pM1._21 * pM2._13 + pM1._22 * pM2._23 + pM1._23 * pM2._33 + pM1._24 * pM2._43;
	pOut._24 = pM1._21 * pM2._14 + pM1._22 * pM2._24 + pM1._23 * pM2._34 + pM1._24 * pM2._44;
	pOut._31 = pM1._31 * pM2._11 + pM1._32 * pM2._21 + pM1._33 * pM2._31 + pM1._34 * pM2._41;
	pOut._32 = pM1._31 * pM2._12 + pM1._32 * pM2._22 + pM1._33 * pM2._32 + pM1._34 * pM2._42;
	pOut._33 = pM1._31 * pM2._13 + pM1._32 * pM2._23 + pM1._33 * pM2._33 + pM1._34 * pM2._43;
	pOut._34 = pM1._31 * pM2._14 + pM1._32 * pM2._24 + pM1._33 * pM2._34 + pM1._34 * pM2._44;
	pOut._41 = pM1._41 * pM2._11 + pM1._42 * pM2._21 + pM1._43 * pM2._31 + pM1._44 * pM2._41;
	pOut._42 = pM1._41 * pM2._12 + pM1._42 * pM2._22 + pM1._43 * pM2._32 + pM1._44 * pM2._42;
	pOut._43 = pM1._41 * pM2._13 + pM1._42 * pM2._23 + pM1._43 * pM2._33 + pM1._44 * pM2._43;
	pOut._44 = pM1._41 * pM2._14 + pM1._42 * pM2._24 + pM1._43 * pM2._34 + pM1._44 * pM2._44;
	return pOut;
}

D3DMATRIX Matrix(FVector rot, FVector origin = FVector(0, 0, 0))
{
	float radPitch = (rot.x * float(M_PI) / 180.f);
	float radYaw = (rot.y * float(M_PI) / 180.f);
	float radRoll = (rot.z * float(M_PI) / 180.f);

	float SP = sinf(radPitch);
	float CP = cosf(radPitch);
	float SY = sinf(radYaw);
	float CY = cosf(radYaw);
	float SR = sinf(radRoll);
	float CR = cosf(radRoll);

	D3DMATRIX matrix;
	matrix.m[0][0] = CP * CY;
	matrix.m[0][1] = CP * SY;
	matrix.m[0][2] = SP;
	matrix.m[0][3] = 0.f;

	matrix.m[1][0] = SR * SP * CY - CR * SY;
	matrix.m[1][1] = SR * SP * SY + CR * CY;
	matrix.m[1][2] = -SR * CP;
	matrix.m[1][3] = 0.f;

	matrix.m[2][0] = -(CR * SP * CY + SR * SY);
	matrix.m[2][1] = CY * SR - CR * SP * SY;
	matrix.m[2][2] = CR * CP;
	matrix.m[2][3] = 0.f;

	matrix.m[3][0] = origin.x;
	matrix.m[3][1] = origin.y;
	matrix.m[3][2] = origin.z;
	matrix.m[3][3] = 1.f;

	return matrix;
}

float powf_(float _X, float _Y) {
	return (_mm_cvtss_f32(_mm_pow_ps(_mm_set_ss(_X), _mm_set_ss(_Y))));
}

double GetDistance(double x1, double y1, double x2, double y2) {
	return sqrtf(powf((x2 - x1), 2) + powf_((y2 - y1), 2));
}

double RadianToDegree(double radian) {
	return radian * (180 / M_PI);
}

double DegreeToRadian(double degree) {
	return degree * (M_PI / 180);
}

FRotator GetRotation(const FVector& angles)
{
	FRotator forward;
	float sp, sy, cp, cy;

	sy = sin(DegreeToRadian(angles.y));
	cy = cos(DegreeToRadian(angles.y));

	sp = sin(DegreeToRadian(angles.x));
	cp = cos(DegreeToRadian(angles.x));

	forward.Yaw = cp * sy;
	forward.Pitch = cp * cy;
	forward.Roll = -sp;
	return forward;
}

boolean bIsInRectangle(double centerX, double centerY, double radius, double x, double y) {
	return x >= centerX - radius && x <= centerX + radius &&
		y >= centerY - radius && y <= centerY + radius;
}

float RandomFloat(float a, float b)
{
	float random = ((float)rand()) / (float)RAND_MAX;
	float diff = b - a;
	float r = random * diff;
	return a + r;
}

bool IsInScreen(FVector2D pos, int over = 30)
{
	if (pos.x > -over && pos.x < Width + over && pos.y > -over && pos.y < Height + over)
		return true;
	return false;
}

UINT64 find_code_cave(UINT64 BaseAddress, int index)
{
	BYTE tmp = 0;
	int i = 0;
	int Size = 0;
	int w = 0;
	for (int i = 0; i < 0x7ffffffff; i++)
	{
		tmp = driver::read<BYTE>(BaseAddress + i);
		if (tmp == 0xcc)
			Size++;
		else if (Size > 200)
		{
			if (w == index)
			{
				return (BaseAddress + i - Size);
			}
			Size = 0;

			w++;
		}

		i++;
	}
	return 0;
}
#endif // STRUCTS_H
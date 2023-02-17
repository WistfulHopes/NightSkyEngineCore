#pragma once

#include "../../CString.h"
#include "../CollisionBox.h"
#include <cstdint>
#include <cstdio>
#include <cstdio>
#include <functional>
#include <cstddef>

#pragma pack (push, 1)

class State;
class PlayerCharacter;
class FighterGameState;

constexpr int32_t CollisionArraySize = 50;

enum DistanceType
{
	DIST_Distance,
	DIST_DistanceX,
	DIST_DistanceY,
	DIST_FrontDistanceX,
};

enum HomingType
{
	HOMING_DistanceAccel,
	HOMING_FixAccel,
	HOMING_ToSpeed,
};

enum PosType
{
	POS_Player,
	POS_Self,
	POS_Center,
	POS_Enemy,
	POS_Hit,
};

enum ObjType
{
	OBJ_Self,
	OBJ_Enemy,
	OBJ_Parent,
	OBJ_Child0,
	OBJ_Child1,
	OBJ_Child2,
	OBJ_Child3,
	OBJ_Child4,
	OBJ_Child5,
	OBJ_Child6,
	OBJ_Child7,
	OBJ_Child8,
	OBJ_Child9,
	OBJ_Child10,
	OBJ_Child11,
	OBJ_Child12,
	OBJ_Child13,
	OBJ_Child14,
	OBJ_Child15,
	OBJ_Null,
};

enum class HitSFXType
{
	SFX_Punch,
	SFX_Kick,
	SFX_Slash,
};

enum class HitVFXType
{
	VFX_Strike,
	VFX_Slash,
};

enum InternalValue //internal values list
{
	VAL_StoredRegister,
	VAL_Angle,
	VAL_ActionFlag,
	VAL_StateVal1,
	VAL_StateVal2,
	VAL_StateVal3,
	VAL_StateVal4,
	VAL_StateVal5,
	VAL_StateVal6,
	VAL_StateVal7,
	VAL_StateVal8,
	VAL_PlayerVal1,
	VAL_PlayerVal2,
	VAL_PlayerVal3,
	VAL_PlayerVal4,
	VAL_PlayerVal5,
	VAL_PlayerVal6,
	VAL_PlayerVal7,
	VAL_PlayerVal8,
	VAL_SpeedX, 
	VAL_SpeedY,
	VAL_ActionTime,
	VAL_AnimTime,
	VAL_PosX,
	VAL_PosY,
	VAL_Inertia,
	VAL_FacingRight,
	VAL_DistanceToFrontWall,
	VAL_DistanceToBackWall,
	VAL_IsAir,
	VAL_IsLand,
	VAL_IsStunned,
	VAL_IsKnockedDown,
	VAL_HasHit,
	VAL_IsAttacking,
	VAL_Health,
	VAL_Meter,
	VAL_DefaultCommonAction,
};

enum HitAction
{
	HACT_None,
	HACT_GroundNormal,
	HACT_AirNormal,
	HACT_Crumple,
	HACT_ForceCrouch,
	HACT_ForceStand,
	HACT_GuardBreakStand,
	HACT_GuardBreakCrouch,
	HACT_AirFaceUp,
	HACT_AirVertical,
	HACT_AirFaceDown,
	HACT_Blowback,
};

enum BlockType
{
	BLK_Mid,
	BLK_High,
	BLK_Low,
	BLK_None,
};

struct WallBounceEffect
{
	int32_t WallBounceCount = 0;
	int32_t WallBounceUntech = 0;
	int32_t WallBounceXSpeed = 0;
	int32_t WallBounceYSpeed = 0;
	int32_t WallBounceGravity = 1900;
	bool WallBounceInCornerOnly = false;
};

struct GroundBounceEffect
{
	int32_t GroundBounceCount = 0;
	int32_t GroundBounceUntech = 0;
	int32_t GroundBounceXSpeed = 0;
	int32_t GroundBounceYSpeed = 0;
	int32_t GroundBounceGravity = 1900;
};

struct HitEffect
{
	int32_t AttackLevel = 0;
	BlockType CurBlockType = BLK_Mid;
	int32_t Hitstun = 0;
	int32_t Blockstun = 0;
	int32_t Untech = 0;
	int32_t Hitstop = 0;
	int32_t BlockstopModifier = 0;
	int32_t HitDamage = 0;
	int32_t MinimumDamagePercent = 0;
	int32_t ChipDamagePercent = 0;
	int32_t InitialProration = 100;
	int32_t ForcedProration = 100;
	int32_t HitPushbackX = 0;
	int32_t AirHitPushbackX = 0;
	int32_t AirHitPushbackY = 0;
	int32_t HitGravity = 1900;
	int32_t HitAngle = 0;
	HitAction GroundHitAction = HACT_GroundNormal;
	HitAction AirHitAction = HACT_AirNormal;
	int32_t KnockdownTime = 25;
	GroundBounceEffect CurGroundBounceEffect;
	WallBounceEffect CurWallBounceEffect;
	HitSFXType SFXType = HitSFXType::SFX_Punch;
	HitVFXType VFXType = HitVFXType::VFX_Strike;
	bool DeathCamOverride = false;
};

struct HomingParams
{
	HomingType Type;
	ObjType Target = OBJ_Null;
	PosType Pos;
	int32_t OffsetX;
	int32_t OffsetY;
	int32_t ParamA;
	int32_t ParamB;
};

struct Vector
{
	Vector(int32_t NewX, int32_t NewY)
	{
		X = NewX;
		Y = NewY;
	}

	int32_t X;
	int32_t Y;
};

class BattleActor
{
public:
	BattleActor();

	virtual ~BattleActor()
	{
	}

	unsigned char ObjSync; //starting from this until ObjSyncEnd, everything is saved/loaded for rollback
	bool IsActive = false;
protected:
	//internal values
	int32_t PosX = 0;
	int32_t PosY = 0;
	int32_t PrevPosX = 0;
	int32_t PrevPosY = 0;
	int32_t SpeedX = 0;
	int32_t SpeedY = 0;
	int32_t Gravity = 1900;
	int32_t Inertia = 0;
	int32_t ActionTime = 0;
	int32_t PushHeight = 0;
	int32_t PushHeightLow = 0;
	int32_t PushWidth = 0;
	int32_t PushWidthExpand = 0;
	int32_t Hitstop = 0;
public:
	int32_t L = 0;
	int32_t R = 0;
	int32_t T = 0;
	int32_t B = 0;
	bool HitActive = false;
	bool IsAttacking = false;
	bool InitOnNextFrame = false;
	HitEffect NormalHitEffect;
	HitEffect CounterHitEffect;
	HomingParams Homing;
protected:
	bool AttackHeadAttribute = false;
	bool AttackProjectileAttribute = true;
	bool RoundStart = true;
	bool HasHit = false;
	bool DeactivateOnNextUpdate = false;
	int32_t SpeedXPercent = 100;
	bool SpeedXPercentPerFrame = false;
	int32_t SpeedYPercent = 100;
	bool SpeedYPercentPerFrame = false;
	bool ScreenCollisionActive = false;

public:
	bool PushCollisionActive = false;
	bool ProrateOnce = false;
public:
	//script values stored here
	int32_t StateVal1 = 0;
	int32_t StateVal2 = 0;
	int32_t StateVal3 = 0;
	int32_t StateVal4 = 0;
	int32_t StateVal5 = 0;
	int32_t StateVal6 = 0;
	int32_t StateVal7 = 0;
	int32_t StateVal8 = 0;
	int32_t StoredRegister = 0;

	bool FacingRight = true;
	int32_t MiscFlags = 0;
	//disabled if not player
	bool IsPlayer = false;
	int32_t SuperFreezeTime = -1;
		
	bool DeactivateOnStateChange = false;
	bool DeactivateOnReceiveHit = true;

	//cel name
	CString<64> CelNameInternal;
	//for hit effect overrides
	CString<64> HitEffectName;
	
	//current animation time
	int32_t AnimTime = 0;

	//for spawning hit particles
	int32_t HitPosX = 0;
	int32_t HitPosY = 0;

	bool DefaultCommonAction = true;

	CollisionBox CollisionBoxes[CollisionArraySize]{};

	CString<64> ObjectStateName;
	uint32_t ObjectID = 0;

	//pointer to player. if this is not a player, it will point32_t to the owning player.
	PlayerCharacter* Player = nullptr;

	//anything past here isn't saved or loaded for rollback
	int32_t ObjSyncEnd = 0;

	State* ObjectState;

	int32_t ObjNumber;

	FighterGameState* GameState;

protected:
	//move object based on speed and inertia
	void Move();
	//calculates homing speed based on params
	void CalculateHoming();
	//get boxes based on cel name
	void GetBoxes();

public:
	void SaveForRollback(unsigned char* Buffer);
	void LoadForRollback(unsigned char* Buffer);

	//handles pushing objects
	void HandlePushCollision(BattleActor* OtherObj);
	//handles hitting objects
	void HandleHitCollision(PlayerCharacter* OtherChar);
	//handles appling hit effect
	void HandleHitEffect(PlayerCharacter* OtherChar, HitEffect InHitEffect);
	//handles object clashes
	void HandleClashCollision(BattleActor* OtherObj);
	//handles flip
	void HandleFlip();
	//gets position from pos type
	void PosTypeToPosition(PosType Type, int32_t* OutPosX, int32_t* OutPosY);

	virtual void LogForSyncTest(FILE* file);

	//initializes the object. not for use with players.
	void InitObject();
	//updates the object. called every frame
	virtual void Update();
	
	//static helpers
	static int32_t Vec2Angle_x1000(int32_t x, int32_t y);
	static int32_t Cos_x1000(int32_t Deg_x10);
	static int32_t Sin_x1000(int32_t Deg_x10);

	//script callable functions
	//gets internal value for script
	int32_t GetInternalValue(InternalValue InternalValue, ObjType ObjType = OBJ_Self);
	void SetInternalValue(InternalValue InternalValue, int32_t NewValue, ObjType ObjType = OBJ_Self);
	//checks if on frame
	bool IsOnFrame(int32_t Frame);
	//check if hitstop, super freeze, or throw lock is active
	bool IsStopped();
	//sets cel name
	void SetCelName(char* InCelName);
	//sets custom hit effect name
	void SetHitEffectName(char* InHitEffectName);
	//sets x position
	void SetPosX(int32_t InPosX);
	//sets y position
	void SetPosY(int32_t InPosY);
	//adds x position depending on direction
	void AddPosX(int32_t InPosX);
	//adds x position with no regard for direction
	void AddPosXRaw(int32_t InPosX);
	//adds y position
	void AddPosY(int32_t InPosY);
	//sets x speed
	void SetSpeedX(int32_t InSpeedX);
	//sets x speed with no regard for direction
	void SetSpeedXRaw(int32_t InSpeedX);
	//sets y speed
	void SetSpeedY(int32_t InSpeedY);
	//adds x speed
	void AddSpeedX(int32_t InSpeedX);
	//adds x speed with no regard for direction
	void AddSpeedXRaw(int32_t InSpeedX);
	//adds y speed
	void AddSpeedY(int32_t InSpeedY);
	//the current x speed will be set to this percent.
	void SetSpeedXPercent(int32_t Percent);
	//the current x speed will be set to this percent every frame.
	void SetSpeedXPercentPerFrame(int32_t Percent);
	//the current y speed will be set to this percent.
	void SetSpeedYPercent(int32_t Percent);
	//the current y speed will be set to this percent every frame.
	void SetSpeedYPercentPerFrame(int32_t Percent);
	//sets gravity
	void SetGravity(int32_t InGravity);
	//adds gravity
	void AddGravity(int32_t InGravity);
	//sets inertia. when inertia is enabled, inertia adds to your position every frame, but inertia decreases every frame
	void SetInertia(int32_t InInertia);
	//adds inertia
	void AddInertia(int32_t InInertia);
	//clears inertia
	void ClearInertia();
	//enables inertia
	void EnableInertia();
	//disables inertia
	void DisableInertia();
	//halts momentum
	void HaltMomentum();
	//sets homing parameters
	void SetHomingParam(HomingType Type, ObjType Target, PosType Pos, int32_t OffsetX, int32_t OffsetY, int32_t ParamA, int32_t ParamB);
	//clears homing parameters
	void ClearHomingParam();
	//calculates distance between points
	int32_t CalculateDistanceBetweenPoints(DistanceType Type, ObjType Obj1, PosType Pos1, ObjType Obj2, PosType Pos2);
	//sets x position by percent of screen
	void SetPosXByScreenPercent(int32_t ScreenPercent);
	//expands pushbox width temporarily
	void SetPushWidthExpand(int32_t Expand);
	//sets direction
	void SetFacing(bool NewFacingRight);
	//flips character
	void FlipCharacter();
	//enables auto flip
	void EnableFlip(bool Enabled);
	//enables hit
	void EnableHit(bool Enabled);
	//enables prorate once
	void EnableProrateOnce(bool Enabled);
	//toggles push collision
	void SetPushCollisionActive(bool Active);
	//sets attacking. while this is true, you can be counter hit, but you can hit the opponent and chain cancel.
	void SetAttacking(bool Attacking);
	//gives the current move the head attribute. for use with air attacks
	void SetHeadAttribute(bool HeadAttribute);
	//gives the current move the projectile attribute. for use with projectile attacks
	void SetProjectileAttribute(bool ProjectileAttribute);
	//sets hit effect on normal hit
	void SetHitEffect(HitEffect InHitEffect);
	//sets hit effect on counter hit
	void SetCounterHitEffect(HitEffect InHitEffect);
	//creates common particle
	std::function<void(char*, PosType, Vector, int32_t)> CreateCommonParticle;
	//creates character particle
	std::function<void(char*, PosType PosType, Vector, int32_t)> CreateCharaParticle;
	//creates common particle and attaches it to the object. only use with non-player objects.
	std::function<void(char*)> LinkCommonParticle;
	//creates character particle and attaches it to the object. only use with non-player objects.
	std::function<void(char*)> LinkCharaParticle;
	//plays common sound
	std::function<void(char*)> PlayCommonSound;
	//plays chara sound
	std::function<void(char*)> PlayCharaSound;
	//pauses round timer
	void PauseRoundTimer(bool Pause);
	//sets object id
	void SetObjectID(int32_t InObjectID);
	//gets object by type
	BattleActor* GetBattleActor(ObjType Type);
	//DO NOT USE ON PLAYERS. if object goes beyond screen bounds, deactivate
	void DeactivateIfBeyondBounds();
	//if player changes state, deactivate
	void EnableDeactivateOnStateChange(bool Enable);
	//if player receives hit, deactivate
	void EnableDeactivateOnReceiveHit(bool Enable);
	//DO NOT USE ON PLAYERS. sets the object to deactivate next frame.
	void DeactivateObject();
	//resets object for next use
	void ResetObject();
	//views collision. only usable in development or debug builds
	//void CollisionView();
};
#pragma pack(pop)

#define SIZEOF_BATTLEACTOR offsetof(BattleActor, BattleActor::ObjSyncEnd) - offsetof(BattleActor, BattleActor::ObjSync)

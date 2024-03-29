// Fill out your copyright notice in the Description page of Project Settings.

#include "BattleActor.h"
#include "FighterGameState.h"
#include "PlayerCharacter.h"
#include "../State.h"
#include <cstdlib>

#include "Data/CollisionData.h"
#include "../Globals.h"

BattleActor::BattleActor()
{
	ObjSync = 0;
	NormalHitEffect = HitEffect();
	CounterHitEffect = HitEffect();
	HitPosX = 0;
	HitPosY = 0;
	for (int i = 0; i < CollisionArraySize; i++)
	{
		CollisionBoxes[i] = CollisionBox();
	}
	ObjectID = 0;
	Player = nullptr;
	ObjSyncEnd = 0;
	ObjNumber = 0;
	GameState = nullptr;
	ObjectState = nullptr;
}

void BattleActor::InitObject()
{
	RoundStart = false;
	ObjectState->ObjectParent = this;
	ObjectState->Parent = Player;
	Hitstop = 0;
	if (PosY == 0 && PrevPosY != 0)
		Gravity = 1900;
	for (int32_t i = 0; i < CollisionArraySize; i++)
	{
		CollisionBoxes[i] = CollisionBox();
	}
	ObjectState->OnEnter();
	ObjectState->OnUpdate(0);
}

void BattleActor::Update()
{
	if (DeactivateOnNextUpdate)
	{
		ResetObject();
		return;
	}
	if (InitOnNextFrame)
	{
		InitObject();
		InitOnNextFrame = false;
		return;
	}

	//run input buffer before checking hitstop
	if (IsPlayer && Player != nullptr)
	{
		if (!FacingRight && !Player->FlipInputs || Player->FlipInputs && FacingRight) //flip inputs with direction
		{
			const unsigned int Bit1 = (Player->Inputs >> 2) & 1;
			const unsigned int Bit2 = (Player->Inputs >> 3) & 1;
			unsigned int x = (Bit1 ^ Bit2);

			x = x << 2 | x << 3;

			Player->Inputs = Player->Inputs ^ x;
		}
			
		if (!Player->Inputs << 27) //if no direction, set neutral input
		{
			Player->Inputs |= (int)InputNeutral;
		}
		else
		{
			Player->Inputs = Player->Inputs & ~(int)InputNeutral; //remove neutral input if directional input
		}
	}
	
	L = PosX - PushWidth / 2; //sets pushboxes
	R = PosX + PushWidth / 2;
	if (FacingRight)
		R += PushWidthExpand;
	else
		L -= PushWidthExpand;
	T = PosY + PushHeight;
	B = PosY - PushHeightLow;
	
	if (MiscFlags & MISC_FlipEnable)
		HandleFlip();
	
	if (SuperFreezeTime > 0)
	{
		SuperFreezeTime--;
		return;
	}
	if (SuperFreezeTime == 0)
	{
		if (ObjectState)
			ObjectState->OnSuperFreezeEnd();
		PauseRoundTimer(false);
		GameState->StoredBattleState.PauseParticles = false;
	}
	SuperFreezeTime--;
	
	if (Hitstop > 0) //break if hitstop active.
	{
		Hitstop--;
		return;
	}
	Hitstop--;

	if (!IsPlayer)
	{
		ObjectState->OnUpdate(1/60);
	}
	else
	{
		Player->CurStateMachine.Tick(0.0166666); //update current state
	}

	GetBoxes(); //get boxes from cel name
	if (IsPlayer && Player->IsThrowLock) //if locked by throw, return
		return;
	
	Move(); //handle movement
	
	AnimTime++; //increment counters
	ActionTime++;
	
	if (IsPlayer) //initializes player only values
	{
		if (Player->CurrentActionFlags == ACT_Standing) //set pushbox values based on state
		{
			PushWidth = Player->StandPushWidth;
			PushHeight = Player->StandPushHeight;
			PushHeightLow = 0;
		}
		else if (Player->CurrentActionFlags == ACT_Crouching)
		{
			PushWidth = Player->CrouchPushWidth;
			PushHeight = Player->CrouchPushHeight;
			PushHeightLow = 0;
		}
		else if (Player->CurrentActionFlags == ACT_Jumping)
		{
			PushWidth = Player->AirPushWidth;
			PushHeight = Player->AirPushHeight;
			PushHeightLow = Player->AirPushHeightLow;
		}
		if (RoundStart)
		{
			if (Player->PlayerIndex == 0)
			{
				FacingRight = true;
				PosX = -300000;
			}
			else
			{
				FacingRight = false;
				PosX = 300000;
			}
		}
	}

	RoundStart = false; //round has started
}

int32_t BattleActor::Vec2Angle_x1000(int32_t x, int32_t y)
{
	int32_t Angle = static_cast<int>(atan2(static_cast<double>(y), static_cast<double>(x)) * 57295.77791868204) % 360000;
	if (Angle < 0)
		Angle += 360000;
	return Angle;
}

int32_t BattleActor::Cos_x1000(int32_t Deg_x10)
{
	int32_t Tmp1 = (Deg_x10 + 900) % 3600;
	int32_t Tmp2 = Deg_x10 + 3600;
	if (Tmp1 >= 0)
		Tmp2 = Tmp1;
	if (Tmp2 < 900)
		return gSinTable[Tmp2];
	if (Tmp2 < 1800)
		return gSinTable[1799 - Tmp2];
	if (Tmp2 >= 2700)
		return -gSinTable[3599 - Tmp2];
	return -gSinTable[Tmp2 - 1800];
}

int32_t BattleActor::Sin_x1000(int32_t Deg_x10)
{
	int32_t Tmp1 = Deg_x10 % 3600;
	int32_t Tmp2 = Deg_x10 + 3600;
	if (Tmp1 >= 0)
		Tmp2 = Tmp1;
	if (Tmp2 < 900)
		return gSinTable[Tmp2];
	if (Tmp2 < 1800)
		return gSinTable[1799 - Tmp2];
	if (Tmp2 >= 2700)
		return -gSinTable[3599 - Tmp2];
	return -gSinTable[Tmp2 - 1800];
}

void BattleActor::Move()
{
	PrevPosX = PosX; //set previous pos values
	PrevPosY = PosY;
	
	CalculateHoming();
	
	int32_t FinalSpeedX = SpeedX * SpeedXPercent / 100;;
	int32_t FinalSpeedY = SpeedY * SpeedYPercent / 100;
	if (SpeedXPercentPerFrame)
		SpeedX = FinalSpeedX;
	if (SpeedYPercentPerFrame)
		SpeedY = FinalSpeedY;
	
	if (MiscFlags & MISC_InertiaEnable) //only use inertia if enabled
	{
		if (PosY <= 0) //only decrease inertia if grounded
		{
			Inertia = Inertia - Inertia / 10;
		}
		if (Inertia > -875 && Inertia < 875) //if inertia small enough, set to zero
		{
			ClearInertia();
		}
		AddPosX(Inertia);
	}
		
	AddPosX(FinalSpeedX); //apply speed
	
	if (IsPlayer && Player != nullptr)
	{
		if (Player->AirDashTimer <= 0) //only set gravity if air dash timer isn't active
		{
			AddPosY(FinalSpeedY);
			if (PosY > 0)
				SpeedY -= Gravity;
		}
	}
	else
	{
		AddPosY(SpeedY);
		if (PosY > 0)
			SpeedY -= Gravity;
	}	
	
	if (PosY < 0) //if on ground, force y values to zero
	{
		PosY = 0;
		SpeedY = 0;
	}
	if (ScreenCollisionActive)
	{
		if (PosX < -1800000)
		{
			PosX = -1800001;
		}
		else if (PosX > 1800000)
		{
			PosX = 1800001;
		}
	}
}

void BattleActor::CalculateHoming()
{
	if (Homing.Target != OBJ_Null)
	{
		BattleActor* Target = GetBattleActor(Homing.Target);

		if (Target != nullptr)
		{
			int32_t TargetPosX = 0;
			int32_t TargetPosY = 0;

			Target->PosTypeToPosition(Homing.Pos, &TargetPosX, &TargetPosY);
			
			bool TargetFacingRight = Target->FacingRight;
			int32_t HomingOffsetX = -Homing.OffsetX;
			if (!TargetFacingRight)
				HomingOffsetX = -HomingOffsetX;
			
			if (Homing.Type == HOMING_DistanceAccel)
			{
				SetSpeedXPercentPerFrame(Homing.ParamB);
				SetSpeedYPercentPerFrame(Homing.ParamB);
				AddSpeedXRaw(Homing.ParamA * (TargetPosX - PosX) / 100);
				AddSpeedY(Homing.ParamA * (TargetPosY - PosY) / 100);
			}
			else if (Homing.Type == HOMING_FixAccel)
			{
				int32_t TmpPosY = TargetPosY + Homing.OffsetY - PosY;
				int32_t TmpPosX = TargetPosX + HomingOffsetX - PosX;
				int32_t Angle = Vec2Angle_x1000(TmpPosX, TmpPosY) / 100;
				SetSpeedXPercentPerFrame(Homing.ParamB);
				SetSpeedYPercentPerFrame(Homing.ParamB);
				int32_t CosParamA = Homing.ParamA * Cos_x1000(Angle) / 1000; 
				int32_t SinParamA = Homing.ParamA * Sin_x1000(Angle) / 1000; 
				AddSpeedXRaw(CosParamA);
				AddSpeedY(SinParamA);
			}
			else if (Homing.Type == HOMING_ToSpeed)
			{
				int32_t TmpPosY = TargetPosY + Homing.OffsetY - PosY;
				int32_t TmpPosX = TargetPosX + HomingOffsetX - PosX;
				int32_t Angle = Vec2Angle_x1000(TmpPosX, TmpPosY) / 100;
				int32_t CosParamA = Homing.ParamA * Cos_x1000(Angle) / 1000; 
				int32_t SinParamA = Homing.ParamA * Sin_x1000(Angle) / 1000; 
				if (Homing.ParamB <= 0)
				{
					if (Homing.ParamB >= 0)
					{
						CosParamA = SpeedX;
					}
					else if (SpeedX < CosParamA)
					{
						CosParamA = Homing.ParamB + SpeedX;
					}
					int32_t TmpParamB = Homing.ParamB;
					while (SpeedX - CosParamA <= TmpParamB)
					{
						SetSpeedXRaw(CosParamA);
						if (TmpParamB <= 0)
						{
							if (TmpParamB >= 0)
							{
								SinParamA = SpeedY;
								SetSpeedY(SinParamA);
								return;
							}
							if (SpeedY < SinParamA)
							{
								SinParamA = SpeedY + TmpParamB;
								SetSpeedY(SinParamA);
								return;
							}
						}
						else
						{
							if (SpeedY < SinParamA)
							{
								if (SinParamA - SpeedY > TmpParamB)
								{
									SinParamA = SpeedY + TmpParamB;
								}
							}
							if (SpeedY - SinParamA <= TmpParamB)
							{
								SetSpeedY(SinParamA);
								return;
							}
						}
						SinParamA = SpeedY - TmpParamB;
						SetSpeedY(SinParamA);
						return;
					}
				}
				else
				{
					if (SpeedX < CosParamA)
					{
						if (CosParamA - SpeedX > Homing.ParamB)
						{
							CosParamA = Homing.ParamB + SpeedX;
						}
						int32_t TmpParamB = Homing.ParamB;
						while (SpeedX - CosParamA <= TmpParamB)
						{
							SetSpeedXRaw(CosParamA);
							if (TmpParamB <= 0)
							{
								if (TmpParamB >= 0)
								{
									SinParamA = SpeedY;
									SetSpeedY(SinParamA);
									return;
								}
								if (SpeedY < SinParamA)
								{
									SinParamA = SpeedY + TmpParamB;
									SetSpeedY(SinParamA);
									return;
								}
							}
							else
							{
								if (SpeedY < SinParamA)
								{
									if (SinParamA - SpeedY > TmpParamB)
									{
										SinParamA = SpeedY + TmpParamB;
									}
								}
								if (SpeedY - SinParamA <= TmpParamB)
								{
									SetSpeedY(SinParamA);
									return;
								}
							}
							SinParamA = SpeedY - TmpParamB;
							SetSpeedY(SinParamA);
							return;
						}
					}
					if (SpeedX - CosParamA <= Homing.ParamB)
					{
						int32_t TmpParamB = Homing.ParamB;
						while (SpeedX - CosParamA <= TmpParamB)
						{
							SetSpeedXRaw(CosParamA);
							if (TmpParamB <= 0)
							{
								if (TmpParamB >= 0)
								{
									SinParamA = SpeedY;
									SetSpeedY(SinParamA);
									return;
								}
								if (SpeedY < SinParamA)
								{
									SinParamA = SpeedY + TmpParamB;
									SetSpeedY(SinParamA);
									return;
								}
							}
							else
							{
								if (SpeedY < SinParamA)
								{
									if (SinParamA - SpeedY > TmpParamB)
									{
										SinParamA = SpeedY + TmpParamB;
									}
								}
								if (SpeedY - SinParamA <= TmpParamB)
								{
									SetSpeedY(SinParamA);
									return;
								}
							}
							SinParamA = SpeedY - TmpParamB;
							SetSpeedY(SinParamA);
							return;
						}
					}
				}
				int32_t TmpParamB = Homing.ParamB;
				while (SpeedX - CosParamA <= TmpParamB)
				{
					SetSpeedXRaw(CosParamA);
					if (TmpParamB <= 0)
					{
						if (TmpParamB >= 0)
						{
							SinParamA = SpeedY;
							SetSpeedY(SinParamA);
							return;
						}
						if (SpeedY < SinParamA)
						{
							SinParamA = SpeedY + TmpParamB;
							SetSpeedY(SinParamA);
							return;
						}
					}
					else
					{
						if (SpeedY < SinParamA)
						{
							if (SinParamA - SpeedY > TmpParamB)
							{
								SinParamA = SpeedY + TmpParamB;
							}
						}
						if (SpeedY - SinParamA <= TmpParamB)
						{
							SetSpeedY(SinParamA);
							return;
						}
					}
					SinParamA = SpeedY - TmpParamB;
					SetSpeedY(SinParamA);
					return;
				}
			}
		}
	}
}

void BattleActor::SetPosX(int32_t InPosX)
{
	PosX = InPosX;
}

void BattleActor::SetPosY(int32_t InPosY)
{
	PosY = InPosY;
}

void BattleActor::AddPosX(int32_t InPosX)
{
	if (FacingRight)
	{
		PosX += InPosX;
	}
	else
	{
		PosX -= InPosX;
	}
}

void BattleActor::AddPosXRaw(int32_t InPosX)
{
	PosX += InPosX;
}

void BattleActor::AddPosY(int32_t InPosY)
{
	PosY += InPosY;
}

void BattleActor::SetSpeedX(int32_t InSpeedX)
{
	SpeedX = InSpeedX;
}

void BattleActor::SetSpeedXRaw(int32_t InSpeedX)
{
	if (FacingRight)
		SpeedX = InSpeedX;
	else
		SpeedX = -InSpeedX;
}

void BattleActor::SetSpeedY(int32_t InSpeedY)
{
	SpeedY = InSpeedY;
}

void BattleActor::SetGravity(int32_t InGravity)
{
	Gravity = InGravity;
}

void BattleActor::AddGravity(int32_t InGravity)
{
	Gravity += InGravity;
}

void BattleActor::HaltMomentum()
{
	SpeedX = 0;
	SpeedY = 0;
	Gravity = 0;
	ClearInertia();
}

void BattleActor::SetPushWidthExpand(int32_t Expand)
{
	PushWidthExpand = Expand;
}

void BattleActor::SetHomingParam(HomingType Type, ObjType Target, PosType Pos, int32_t OffsetX, int32_t OffsetY,
	int32_t ParamA, int32_t ParamB)
{
	Homing.Type = Type;
	Homing.Target = Target;
	Homing.Pos = Pos;
	Homing.OffsetX = OffsetX;
	Homing.OffsetY = OffsetY;
	Homing.ParamA = ParamA;
	Homing.ParamB = ParamB;
}

void BattleActor::ClearHomingParam()
{
	Homing = HomingParams();
}

int32_t BattleActor::CalculateDistanceBetweenPoints(DistanceType Type, ObjType Obj1, PosType Pos1, ObjType Obj2, PosType Pos2)
{
	BattleActor* Actor1 = GetBattleActor(Obj1);
	BattleActor* Actor2 = GetBattleActor(Obj2);
	if (Actor1 != nullptr && Actor2 != nullptr)
	{
		int32_t PosX1 = 0;
		int32_t PosX2 = 0;
		int32_t PosY1 = 0;
		int32_t PosY2 = 0;

		Actor1->PosTypeToPosition(Pos1, &PosX1, &PosY1);
		Actor2->PosTypeToPosition(Pos2, &PosX2, &PosY2);

		int32_t ObjDist;
		
		switch (Type)
		{
		case DIST_Distance:
			ObjDist = isqrt(static_cast<int64_t>(PosX2 - PosX1) * static_cast<int64_t>(PosX2 - PosX1) + static_cast<int64_t>(PosY2 - PosY1) * static_cast<int64_t>(PosY2 - PosY1));
			break;
		case DIST_DistanceX:
			ObjDist = abs(PosX2 - PosX1);
			break;
		case DIST_DistanceY:
			ObjDist = abs(PosY2 - PosY1);
			break;
		case DIST_FrontDistanceX:
			{
				int Direction = 1;
				if (!Actor1->FacingRight)
				{
					Direction = 1;
				}
				ObjDist = abs(PosX2 - PosX1) * Direction;
			}
			break;
		default:
			return 0;
		}
		return ObjDist;
	}
	return 0;
}

int32_t BattleActor::GetInternalValue(InternalValue InternalValue, ObjType ObjType)
{
	BattleActor* Obj;
	switch (ObjType)
	{
	case OBJ_Self:
		Obj = this;
		break;
	case OBJ_Enemy:
		Obj = Player->Enemy;
		break;
	case OBJ_Parent:
		Obj = Player;
		break;
	default:
		Obj = this;
		break;
	}
	switch (InternalValue)
	{
	case VAL_StoredRegister:
		return Obj->StoredRegister;
	case VAL_ActionFlag:
		if (Obj->IsPlayer && Obj->Player != nullptr) //only available as player character
			return Obj->Player->CurrentActionFlags;
		break;
	case VAL_PosX:
		return Obj->PosX;
	case VAL_PosY:
		return Obj->PosY;
	case VAL_SpeedX:
		return Obj->SpeedX;
	case VAL_SpeedY:
		return Obj->SpeedY;
	case VAL_ActionTime:
		return Obj->ActionTime;
	case VAL_AnimTime:
		return Obj->AnimTime;
	case VAL_Inertia:
		return Obj->Inertia;
	case VAL_FacingRight:
		return Obj->FacingRight;
	case VAL_HasHit:
		return Obj->HasHit;
	case VAL_IsAttacking:
		return Obj->IsAttacking;
	case VAL_DistanceToBackWall:
		if (FacingRight)
			return 2160000 + Obj->PosX;
		return 2160000 - Obj->PosX;
	case VAL_DistanceToFrontWall:
		if (FacingRight)
			return 2160000 - Obj->PosX;
		return 2160000 + Obj->PosX;
	case VAL_IsAir:
		return Obj-> PosY > 0;
	case VAL_IsLand:
		return Obj->PosY <= 0;
	case VAL_IsStunned:
		if (Obj->IsPlayer && Obj->Player != nullptr) //only available as player character
			return Obj->Player->IsStunned;
		break;
	case VAL_IsKnockedDown:
		if (Obj->IsPlayer && Obj->Player != nullptr) //only available as player character
			return Obj->Player->IsKnockedDown;
		break;
	case VAL_Health:
		if (Obj->IsPlayer && Obj->Player != nullptr) //only available as player character
			return Obj->Player->CurrentHealth;
		break;
	case VAL_Meter:
		if (Obj->IsPlayer && Obj->Player != nullptr) //only available as player character
			return GameState->StoredBattleState.Meter[Obj->Player->PlayerIndex];
		break;
	case VAL_DefaultCommonAction:
		return Obj->DefaultCommonAction;
	case VAL_StateVal1:
		return Obj->StateVal1;
	case VAL_StateVal2:
		return Obj->StateVal2;
	case VAL_StateVal3:
		return Obj->StateVal3;
	case VAL_StateVal4:
		return Obj->StateVal4;
	case VAL_StateVal5:
		return Obj->StateVal5;
	case VAL_StateVal6:
		return Obj->StateVal6;
	case VAL_StateVal7:
		return Obj->StateVal7;
	case VAL_StateVal8:
		return Obj->StateVal8;
	case VAL_PlayerVal1:
		return Obj->Player->PlayerVal1;
	case VAL_PlayerVal2:
		return Obj->Player->PlayerVal2;
	case VAL_PlayerVal3:
		return Obj->Player->PlayerVal3;
	case VAL_PlayerVal4:
		return Obj->Player->PlayerVal4;
	case VAL_PlayerVal5:
		return Obj->Player->PlayerVal5;
	case VAL_PlayerVal6:
		return Obj->Player->PlayerVal6;
	case VAL_PlayerVal7:
		return Obj->Player->PlayerVal7;
	case VAL_PlayerVal8:
		return Obj->Player->PlayerVal8;
	default:
		return 0;
	}
	return 0;
}

void BattleActor::SetInternalValue(InternalValue InternalValue, int32_t NewValue, ObjType ObjType)
{
	BattleActor* Obj;
	switch (ObjType)
	{
	case OBJ_Self:
		Obj = this;
		break;
	case OBJ_Enemy:
		Obj = Player->Enemy;
		break;
	case OBJ_Parent:
		Obj = Player;
		break;
	default:
		Obj = this;
		break;
	}
	switch (InternalValue)
	{
	case VAL_StoredRegister:
		Obj->StoredRegister = NewValue;
		break;
	case VAL_ActionFlag:
		if (Obj->IsPlayer && Obj->Player != nullptr) //only available as player character
			Obj->Player->CurrentActionFlags = NewValue;
		break;
	case VAL_PosX:
		Obj->PosX = NewValue;
		break;
	case VAL_PosY:
		Obj->PosY = NewValue;
		break;
	case VAL_SpeedX:
		Obj->SpeedX = NewValue;
		break;
	case VAL_SpeedY:
		Obj->SpeedY = NewValue;
		break;
	case VAL_ActionTime:
		Obj->ActionTime = NewValue;
		break;
	case VAL_AnimTime:
		Obj->AnimTime = NewValue;
		break;
	case VAL_Inertia:
		Obj->Inertia = NewValue;
		break;
	case VAL_FacingRight:
		Obj->FacingRight = static_cast<bool>(NewValue);
		break;
	case VAL_Health:
		if (Obj->IsPlayer && Obj->Player != nullptr) //only available as player character
			Obj->Player->CurrentHealth = NewValue;
		break;
	case VAL_Meter:
		if (Obj->IsPlayer && Obj->Player != nullptr) //only available as player character
			GameState->StoredBattleState.Meter[Obj->Player->PlayerIndex] = NewValue;
		break;
	case VAL_DefaultCommonAction:
		Obj->DefaultCommonAction = static_cast<bool>(NewValue);
		break;
	case VAL_StateVal1:
		Obj->StateVal1 = NewValue;
		break;
	case VAL_StateVal2:
		Obj->StateVal2 = NewValue;
		break;
	case VAL_StateVal3:
		Obj->StateVal3 = NewValue;
		break;
	case VAL_StateVal4:
		Obj->StateVal4 = NewValue;
		break;
	case VAL_StateVal5:
		Obj->StateVal5 = NewValue;
		break;
	case VAL_StateVal6:
		Obj->StateVal6 = NewValue;
		break;
	case VAL_StateVal7:
		Obj->StateVal7 = NewValue;
		break;
	case VAL_StateVal8:
		Obj->StateVal8 = NewValue;
		break;
	case VAL_PlayerVal1:
		Obj->Player->PlayerVal1 = NewValue;
		break;
	case VAL_PlayerVal2:
		Obj->Player->PlayerVal2 = NewValue;
		break;
	case VAL_PlayerVal3:
		Obj->Player->PlayerVal3 = NewValue;
		break;
	case VAL_PlayerVal4:
		Obj->Player->PlayerVal4 = NewValue;
		break;
	case VAL_PlayerVal5:
		Obj->Player->PlayerVal5 = NewValue;
		break;
	case VAL_PlayerVal6:
		Obj->Player->PlayerVal6 = NewValue;
		break;
	case VAL_PlayerVal7:
		Obj->Player->PlayerVal7 = NewValue;
		break;
	case VAL_PlayerVal8:
		Obj->Player->PlayerVal8 = NewValue;
		break;
	default:
		break;
	}
}

bool BattleActor::IsOnFrame(int32_t Frame)
{
	if (Frame == AnimTime)
	{
		return true;
	}
	return false;
}

bool BattleActor::IsStopped()
{
	return SuperFreezeTime > 0 || Hitstop > 0 || IsPlayer && Player->IsThrowLock;
}

void BattleActor::SetCelName(char* InCelName)
{
	CelNameInternal.SetString(InCelName);
}

void BattleActor::SetHitEffectName(char* InHitEffectName)
{
	HitEffectName.SetString(InHitEffectName);
}

void BattleActor::AddSpeedX(int32_t InSpeedX)
{
	SpeedX += InSpeedX;
}

void BattleActor::AddSpeedXRaw(int32_t InSpeedX)
{
	if (FacingRight)
		SpeedX += InSpeedX;
	else
		SpeedX -= InSpeedX;
}

void BattleActor::AddSpeedY(int32_t InSpeedY)
{
	SpeedY += InSpeedY;
}

void BattleActor::SetSpeedXPercent(int32_t Percent)
{
	SpeedXPercent = Percent;
}

void BattleActor::SetSpeedXPercentPerFrame(int32_t Percent)
{
	SpeedXPercent = Percent;
	SpeedXPercentPerFrame = true;
}

void BattleActor::SetSpeedYPercent(int32_t Percent)
{
	SpeedYPercent = Percent;
}

void BattleActor::SetSpeedYPercentPerFrame(int32_t Percent)
{
	SpeedYPercent = Percent;
	SpeedYPercentPerFrame = true;
}

void BattleActor::SetInertia(int32_t InInertia)
{
	Inertia = InInertia;
}

void BattleActor::AddInertia(int32_t InInertia)
{
	Inertia += InInertia;
}

void BattleActor::ClearInertia()
{
	Inertia = 0;
}

void BattleActor::EnableInertia()
{
	MiscFlags |= MISC_InertiaEnable;
}

void BattleActor::DisableInertia()
{
	MiscFlags = MiscFlags & ~MISC_InertiaEnable;
}

void BattleActor::HandlePushCollision(BattleActor* OtherObj)
{
	if (PushCollisionActive && OtherObj->PushCollisionActive)
	{
		if (Hitstop <= 0 && (!OtherObj->IsPlayer || !OtherObj->Player->IsThrowLock) || (!IsPlayer || !Player->IsThrowLock))
		{
			if (T >= OtherObj->B && B <= OtherObj->T && R >= OtherObj->L && L <= OtherObj->R)
			{
				bool IsPushLeft;
				int32_t CollisionDepth;
				int32_t PosXOffset;
				if(PosX == OtherObj->PosX)
				{
					if (PrevPosX == OtherObj->PrevPosX)
					{
						if (IsPlayer == OtherObj->IsPlayer)
						{
							if (Player->WallTouchTimer == OtherObj->Player->WallTouchTimer)
							{
								IsPushLeft = Player->TeamIndex > 0;
							}
							else
							{
								IsPushLeft = Player->WallTouchTimer > OtherObj->Player->WallTouchTimer;
								if (PosX > 0)
								{
									IsPushLeft = Player->WallTouchTimer <= OtherObj->Player->WallTouchTimer;
								}
							}
						}
						else
						{
							IsPushLeft = IsPlayer > OtherObj->IsPlayer;
						}
					}
					else
					{
						IsPushLeft = PrevPosX < OtherObj->PrevPosX;
					}
				}
				else
				{
					IsPushLeft = PosX < OtherObj->PosX;
				}
				if (IsPushLeft)
				{
					CollisionDepth = OtherObj->L - R;
				}
				else
				{
					CollisionDepth = OtherObj->R - L;
				}
				PosXOffset = CollisionDepth / 2;
				AddPosXRaw(PosXOffset);
			}
		}
	}
}

void BattleActor::SetFacing(bool NewFacingRight)
{
	FacingRight = NewFacingRight;
}

void BattleActor::FlipCharacter()
{
	FacingRight = !FacingRight;
}

void BattleActor::EnableFlip(bool Enabled)
{
	if (Enabled)
	{
		MiscFlags |= MISC_FlipEnable;
	}
	else
	{
		MiscFlags = MiscFlags & ~MISC_FlipEnable;
	}
}

void BattleActor::GetBoxes()
{
	for (int i = 0; i < CollisionArraySize; i++)
	{
		CollisionBoxes[i].Type = BoxType::Hurtbox;
		CollisionBoxes[i].PosX = -24000000;
		CollisionBoxes[i].PosY = -24000000;
		CollisionBoxes[i].SizeX = 0;
		CollisionBoxes[i].SizeY = 0;
	}
	for (int i = 0; i < Player->ColData.size(); i++)
	{
		if (!strcmp(Player->ColData[i]->Name.GetString(), CelNameInternal.GetString()))
		{
			for (int j = 0; j < CollisionArraySize; j++)
			{
				if (Player->ColData[i]->BoxTypeCount > 0)
				{
					if (j < Player->ColData[i]->HurtCount)
					{
						CollisionBoxes[j].Type = BoxType::Hurtbox;
						CollisionBoxes[j].PosX = Player->ColData[i]->Boxes[j].PosX;
						CollisionBoxes[j].PosY = Player->ColData[i]->Boxes[j].PosY;
						CollisionBoxes[j].SizeX = Player->ColData[i]->Boxes[j].SizeX;
						CollisionBoxes[j].SizeY = Player->ColData[i]->Boxes[j].SizeY;
					}
				}
				if (Player->ColData[i]->BoxTypeCount > 1)
				{
					if (j < Player->ColData[i]->HitCount + Player->ColData[i]->HurtCount)
					{
						CollisionBoxes[j].Type = BoxType::Hitbox;
						CollisionBoxes[j].PosX = Player->ColData[i]->Boxes[j].PosX;
						CollisionBoxes[j].PosY = Player->ColData[i]->Boxes[j].PosY;
						CollisionBoxes[j].SizeX = Player->ColData[i]->Boxes[j].SizeX;
						CollisionBoxes[j].SizeY = Player->ColData[i]->Boxes[j].SizeY;
					}
				}
			}
			return;
		}
	}
}

void BattleActor::HandleHitCollision(PlayerCharacter* OtherChar)
{
	if (IsAttacking && HitActive && !OtherChar->StrikeInvulnerable && !OtherChar->StrikeInvulnerableForTime && OtherChar != Player)
	{
		if (!(AttackHeadAttribute && OtherChar->HeadInvulnerable) && !(AttackProjectileAttribute && OtherChar->ProjectileInvulnerable))
		{
			for (int i = 0; i < CollisionArraySize; i++)
			{
				if (CollisionBoxes[i].Type == BoxType::Hitbox)
				{
					for (int j = 0; j < CollisionArraySize; j++)
					{
						if (OtherChar->CollisionBoxes[j].Type == BoxType::Hurtbox)
						{
							CollisionBox Hitbox = CollisionBoxes[i];

							CollisionBox Hurtbox = OtherChar->CollisionBoxes[j];

							if (FacingRight)
							{
								Hitbox.PosX += PosX;
							}
							else
							{
								Hitbox.PosX = -Hitbox.PosX + PosX;  
							}
							Hitbox.PosY += PosY;
							if (OtherChar->FacingRight)
							{
								Hurtbox.PosX += OtherChar->PosX;
							}
							else
							{
								Hurtbox.PosX = -Hurtbox.PosX + OtherChar->PosX;  
							}
							Hurtbox.PosY += OtherChar->PosY;
							
							if (Hitbox.PosY + Hitbox.SizeY / 2 >= Hurtbox.PosY - Hurtbox.SizeY / 2
								&& Hitbox.PosY - Hitbox.SizeY / 2 <= Hurtbox.PosY + Hurtbox.SizeY / 2
								&& Hitbox.PosX + Hitbox.SizeX / 2 >= Hurtbox.PosX - Hurtbox.SizeX / 2
								&& Hitbox.PosX - Hitbox.SizeX / 2 <= Hurtbox.PosX + Hurtbox.SizeX / 2)
							{
								OtherChar->HandleFlip();
								OtherChar->IsStunned = true;
								OtherChar->HaltMomentum();
								HitActive = false;
								HasHit = true;
								int CollisionDepthX;
								if (Hitbox.PosX < Hurtbox.PosX)
									CollisionDepthX = Hurtbox.PosX - Hurtbox.SizeX / 2 - (Hitbox.PosX + Hitbox.SizeX / 2);
								else
									CollisionDepthX = Hitbox.PosX - Hitbox.SizeX / 2 - (Hurtbox.PosX + Hurtbox.SizeX / 2);
								int CollisionDepthY;
								if (Hitbox.PosY < Hurtbox.PosY)
									CollisionDepthY = Hurtbox.PosY - Hurtbox.SizeY / 2 - (Hitbox.PosY + Hitbox.SizeY / 2);
								else
									CollisionDepthY = Hitbox.PosY - Hitbox.SizeY / 2 - (Hurtbox.PosY + Hurtbox.SizeY / 2);
								HitPosX = Hitbox.PosX - CollisionDepthX / 2;
								HitPosY = Hitbox.PosY - CollisionDepthY / 2;
								
								if (IsPlayer)
									Player->CurStateMachine.CurrentState->OnHitOrBlock();
								else
									ObjectState->OnHitOrBlock();
								
								if ((OtherChar->CurrentEnableFlags & ENB_Block || OtherChar->Blockstun > 0) && OtherChar->IsCorrectBlock(NormalHitEffect.CurBlockType)) //check blocking
								{
									CreateCommonParticle("cmn_guard", POS_Hit, Vector(0, 0), -NormalHitEffect.HitAngle);
									if (NormalHitEffect.AttackLevel < 1)
									{
										switch (NormalHitEffect.SFXType)
										{
										case HitSFXType::SFX_Kick:
											PlayCommonSound("GuardMeleeAltS");
											break;
										case HitSFXType::SFX_Slash:
											PlayCommonSound("GuardSlashS");
											break;
										case HitSFXType::SFX_Punch:
										default:
											PlayCommonSound("GuardMeleeS");
											break;
										}
									}
									else if (NormalHitEffect.AttackLevel < 3)
									{
										switch (NormalHitEffect.SFXType)
										{
										case HitSFXType::SFX_Kick:
											PlayCommonSound("GuardMeleeAltM");
											break;
										case HitSFXType::SFX_Slash:
											PlayCommonSound("GuardSlashM");
											break;
										case HitSFXType::SFX_Punch:
										default:
											PlayCommonSound("GuardMeleeM");
											break;
										}
									}
									else
									{
										switch (NormalHitEffect.SFXType)
										{
										case HitSFXType::SFX_Kick:
											PlayCommonSound("GuardMeleeAltL");
											break;
										case HitSFXType::SFX_Slash:
											PlayCommonSound("GuardSlashL");
											break;
										case HitSFXType::SFX_Punch:
										default:
											PlayCommonSound("GuardMeleeL");
											break;
										}
									}
									if (IsPlayer)
										Player->CurStateMachine.CurrentState->OnBlock();
									else
										ObjectState->OnBlock();
									OtherChar->Hitstop = NormalHitEffect.Hitstop;
									OtherChar->Blockstun = NormalHitEffect.Blockstun;
									Hitstop = NormalHitEffect.Hitstop;
									const int32_t ChipDamage = NormalHitEffect.HitDamage * NormalHitEffect.ChipDamagePercent / 100;
									OtherChar->CurrentHealth -= ChipDamage;
									if (OtherChar->CurrentHealth <= 0)
									{
										OtherChar->Blockstun = -1;
										OtherChar->Hitstun = 999;
										OtherChar->Untech = 999;
										if (OtherChar->PosY == 0 && OtherChar->KnockdownTime < 0)
										{
											switch (NormalHitEffect.GroundHitAction)
											{
											case HACT_GroundNormal:
											case HACT_ForceCrouch:
											case HACT_ForceStand:
												OtherChar->Hitstun = NormalHitEffect.Hitstun;
												OtherChar->Untech = -1;
												OtherChar->SetInertia(-NormalHitEffect.HitPushbackX);
												if (OtherChar->TouchingWall)
												{
													if (IsPlayer && Player != nullptr)
													{
														if (PosY > 0)
														{
															ClearInertia();
															AddSpeedX(-NormalHitEffect.HitPushbackX * 2/ 3);
														}
														else
														{
															SetInertia(-NormalHitEffect.HitPushbackX * 2/ 3);
														}
													}
												}
												break;
											case HACT_AirNormal:
											case HACT_AirFaceUp:
											case HACT_AirVertical:
											case HACT_AirFaceDown:
												OtherChar->CurrentGroundBounceEffect = NormalHitEffect.CurGroundBounceEffect;
												OtherChar->CurrentWallBounceEffect = NormalHitEffect.CurWallBounceEffect;
												OtherChar->Untech = NormalHitEffect.Untech;
												OtherChar->Hitstun = -1;
												OtherChar->KnockdownTime = NormalHitEffect.KnockdownTime;
												OtherChar->ClearInertia();
												OtherChar->SetSpeedX(-NormalHitEffect.AirHitPushbackX);
												if (OtherChar->TouchingWall)
												{
													if (IsPlayer && Player != nullptr)
													{
														if (PosY > 0)
														{
															ClearInertia();
															AddSpeedX(-NormalHitEffect.HitPushbackX * 2/ 3);
														}
														else
														{
															SetInertia(-NormalHitEffect.HitPushbackX * 2/ 3);
														}
													}
												}
												break;
											case HACT_Blowback:
												OtherChar->CurrentGroundBounceEffect = NormalHitEffect.CurGroundBounceEffect;
												OtherChar->CurrentWallBounceEffect = NormalHitEffect.CurWallBounceEffect;
												OtherChar->Untech = NormalHitEffect.Untech;
												OtherChar->Hitstun = -1;
												OtherChar->KnockdownTime = NormalHitEffect.KnockdownTime;
												OtherChar->ClearInertia();
												OtherChar->SetSpeedX(-NormalHitEffect.AirHitPushbackX);
												if (OtherChar->TouchingWall)
												{
													if (IsPlayer && Player != nullptr)
													{
														if (PosY > 0)
														{
															ClearInertia();
															AddSpeedX(-NormalHitEffect.HitPushbackX * 2/ 3);
														}
														else
														{
															SetInertia(-NormalHitEffect.HitPushbackX * 2/ 3);
														}
													}
												}
											default:
												break;
											}
											OtherChar->ReceivedHitAction = NormalHitEffect.GroundHitAction;
											OtherChar->ReceivedAttackLevel = NormalHitEffect.AttackLevel;
										}
										else
										{
											switch (NormalHitEffect.AirHitAction)
											{
											case HACT_GroundNormal:
											case HACT_ForceCrouch:
											case HACT_ForceStand:
												OtherChar->Hitstun = NormalHitEffect.Hitstun;
												OtherChar->Untech = -1;
												OtherChar->SetInertia(-NormalHitEffect.HitPushbackX);
												if (OtherChar->TouchingWall)
												{
													if (IsPlayer && Player != nullptr)
													{
														if (PosY > 0)
														{
															ClearInertia();
															AddSpeedX(-NormalHitEffect.HitPushbackX * 2/ 3);
														}
														else
														{
															SetInertia(-NormalHitEffect.HitPushbackX * 2/ 3);
														}
													}
												}
												break;
											case HACT_AirNormal:
											case HACT_AirFaceUp:
											case HACT_AirVertical:
											case HACT_AirFaceDown:
												OtherChar->CurrentGroundBounceEffect = NormalHitEffect.CurGroundBounceEffect;
												OtherChar->CurrentWallBounceEffect = NormalHitEffect.CurWallBounceEffect;
												OtherChar->Untech = NormalHitEffect.Untech;
												OtherChar->Hitstun = -1;
												OtherChar->KnockdownTime = NormalHitEffect.KnockdownTime;
												OtherChar->ClearInertia();
												OtherChar->SetSpeedX(-NormalHitEffect.AirHitPushbackX);
												if (OtherChar->TouchingWall)
												{
													if (IsPlayer && Player != nullptr)
													{
														if (PosY > 0)
														{
															ClearInertia();
															AddSpeedX(-NormalHitEffect.HitPushbackX * 2/ 3);
														}
														else
														{
															SetInertia(-NormalHitEffect.HitPushbackX * 2/ 3);
														}
													}
												}
												break;
											case HACT_Blowback:
												OtherChar->CurrentGroundBounceEffect = NormalHitEffect.CurGroundBounceEffect;
												OtherChar->CurrentWallBounceEffect = NormalHitEffect.CurWallBounceEffect;
												OtherChar->Untech = NormalHitEffect.Untech;
												OtherChar->Hitstun = -1;
												OtherChar->KnockdownTime = NormalHitEffect.KnockdownTime;
												OtherChar->ClearInertia();
												OtherChar->SetSpeedX(-NormalHitEffect.AirHitPushbackX);
												if (OtherChar->TouchingWall)
												{
													if (IsPlayer && Player != nullptr)
													{
														if (PosY > 0)
														{
															ClearInertia();
															AddSpeedX(-NormalHitEffect.HitPushbackX * 2/ 3);
														}
														else
														{
															SetInertia(-NormalHitEffect.HitPushbackX * 2/ 3);
														}
													}
												}
											default:
												break;
											}
											OtherChar->ReceivedHitAction = NormalHitEffect.AirHitAction;
											OtherChar->ReceivedAttackLevel = NormalHitEffect.AttackLevel;
											OtherChar->AirDashTimer = 0;
										}
									}
									else
									{
										if (OtherChar->PosY <= 0)
										{
											OtherChar->SetInertia(-NormalHitEffect.HitPushbackX / 2);
											if (OtherChar->TouchingWall)
											{
												if (IsPlayer && Player != nullptr)
												{
													if (PosY > 0)
													{
														ClearInertia();
														AddSpeedX(-NormalHitEffect.AirHitPushbackX * 2/ 3);
													}
													else
													{
														SetInertia(-NormalHitEffect.HitPushbackX * 2/ 3);
													}
												}
											}
										}
										else
										{
											OtherChar->SetInertia(-NormalHitEffect.AirHitPushbackX / 2);
											if (OtherChar->TouchingWall)
											{
												if (IsPlayer && Player != nullptr)
												{
													if (PosY > 0)
													{
														ClearInertia();
														AddSpeedX(-NormalHitEffect.AirHitPushbackX * 2/ 3);
													}
													else
													{
														SetInertia(-NormalHitEffect.HitPushbackX * 2/ 3);
													}
												}
											}
											OtherChar->SetSpeedY(NormalHitEffect.AirHitPushbackY / 2);
											OtherChar->AirDashTimer = 0;
										}
										OtherChar->HandleBlockAction(NormalHitEffect.CurBlockType);
									}
									OtherChar->AddMeter(NormalHitEffect.HitDamage * OtherChar->MeterPercentOnReceiveHitGuard / 100);
									Player->AddMeter(NormalHitEffect.HitDamage * Player->MeterPercentOnHitGuard / 100);
								}
								else if (!OtherChar->IsAttacking)
								{
									OtherChar->DeathCamOverride = NormalHitEffect.DeathCamOverride;
									if (IsPlayer)
										Player->CurStateMachine.CurrentState->OnHit();
									else
										ObjectState->OnHit();
									HandleHitEffect(OtherChar, NormalHitEffect);
								}
								else
								{
									OtherChar->DeathCamOverride = CounterHitEffect.DeathCamOverride;
									if (IsPlayer)
										Player->CurStateMachine.CurrentState->OnCounterHit();
									else
										ObjectState->OnCounterHit();
									HandleHitEffect(OtherChar, CounterHitEffect);
								}
								if (OtherChar->PosX < PosX)
								{
									OtherChar->SetFacing(true);
								}
								else if (OtherChar->PosX > PosX)
								{
									OtherChar->SetFacing(false);
								}
								OtherChar->Move();
								OtherChar->DisableAll();
								if (OtherChar->CurrentHealth <= 0 && !OtherChar->DeathCamOverride && !OtherChar->IsDead)
								{
									if (Player->CurrentHealth == 0)
									{
										return;
									}
									Player->BattleHudVisibility(false);
									if (Player->Enemy->ReceivedAttackLevel < 2)
									{
										Player->StartSuperFreeze(40);
										Player->PlayCommonCameraAnim("KO_Shake");
									}
									else if (Player->Enemy->ReceivedAttackLevel < 4)
									{
										Player->StartSuperFreeze(70);
										Player->PlayCommonCameraAnim("KO_Zoom");
									}
									else
									{
										Player->StartSuperFreeze(110);
										Player->PlayCommonCameraAnim("KO_Turnaround");
									}
									Player->Hitstop = 0;
									Player->Enemy->Hitstop = 0;
								}
								return;
							}
						}
					}
				}
			}
		}
	}
}

void BattleActor::HandleHitEffect(PlayerCharacter* OtherChar, HitEffect InHitEffect)
{
	int32_t Proration = InHitEffect.ForcedProration;
	if (Player->ComboCounter == 0)
		Proration *= InHitEffect.InitialProration;
	else
		Proration *= 100;
	if (Player->ComboCounter == 0)
		OtherChar->TotalProration = 10000;
	Proration = Proration * OtherChar->TotalProration / 10000;
	
	if (!ProrateOnce || ProrateOnce && !HasHit)
		OtherChar->TotalProration = OtherChar->TotalProration * InHitEffect.ForcedProration / 100;
	
	int FinalDamage;
	if (Player->ComboCounter == 0)
		FinalDamage = InHitEffect.HitDamage;
	else
		FinalDamage = InHitEffect.HitDamage * Proration * Player->ComboRate / 1000000;

	if (FinalDamage < InHitEffect.MinimumDamagePercent * InHitEffect.HitDamage / 100)
		FinalDamage = InHitEffect.HitDamage * InHitEffect.MinimumDamagePercent / 100;

	const int FinalHitPushbackX = InHitEffect.HitPushbackX + Player->ComboCounter * InHitEffect.HitPushbackX / 60;
	const int FinalAirHitPushbackX = InHitEffect.AirHitPushbackX + Player->ComboCounter * InHitEffect.AirHitPushbackX / 60;
	const int FinalAirHitPushbackY = InHitEffect.AirHitPushbackY - Player->ComboCounter * InHitEffect.AirHitPushbackY / 120;
	const int FinalGravity = InHitEffect.HitGravity - Player->ComboCounter * InHitEffect.HitGravity / 60;

	OtherChar->CurrentHealth -= FinalDamage;
	OtherChar->AddMeter(FinalDamage * OtherChar->MeterPercentOnReceiveHit / 100);
	Player->AddMeter(FinalDamage * OtherChar->MeterPercentOnHit / 100);
	Player->ComboCounter++;
	if (OtherChar->CurrentHealth < 0)
		OtherChar->CurrentHealth = 0;
	
	OtherChar->Hitstop = InHitEffect.Hitstop;
	Hitstop = NormalHitEffect.Hitstop;
	OtherChar->Blockstun = -1;
	OtherChar->Gravity = FinalGravity;

	int FinalUntech = InHitEffect.Untech;
	if (Player->ComboTimer >= 14 * 60)
	{
		FinalUntech = FinalUntech * 60 / 100;
	}
	else if (Player->ComboTimer >= 10 * 60)
	{
		FinalUntech = FinalUntech * 70 / 100;
	}
	else if (Player->ComboTimer >= 7 * 60)
	{
		FinalUntech = FinalUntech * 80 / 100;
	}
	else if (Player->ComboTimer >= 5 * 60)
	{
		FinalUntech = FinalUntech * 90 / 100;
	}
	else if (Player->ComboTimer >= 3 * 60)
	{
		FinalUntech = FinalUntech * 95 / 100;
	}
	int FinalHitstun = InHitEffect.Hitstun;
	if (Player->ComboTimer >= 14 * 60)
	{
		FinalHitstun = FinalHitstun * 70 / 100;
	}
	else if (Player->ComboTimer >= 10 * 60)
	{
		FinalHitstun = FinalHitstun * 75 / 100;
	}
	else if (Player->ComboTimer >= 7 * 60)
	{
		FinalHitstun = FinalHitstun * 80 / 100;
	}
	else if (Player->ComboTimer >= 5 * 60)
	{
		FinalHitstun = FinalHitstun * 85 / 100;
	}
	else if (Player->ComboTimer >= 3 * 60)
	{
		FinalHitstun = FinalHitstun * 90 / 100;
	}

	if (OtherChar->PosY == 0 && (!OtherChar->IsKnockedDown && OtherChar->KnockdownTime <= 0))
	{
		switch (InHitEffect.GroundHitAction)
		{
		case HACT_GroundNormal:
		case HACT_ForceCrouch:
		case HACT_ForceStand:
			OtherChar->Hitstun = FinalHitstun;
			OtherChar->Untech = -1;
			OtherChar->SetInertia(-FinalHitPushbackX);
			if (OtherChar->TouchingWall)
			{
				if (IsPlayer && Player != nullptr)
				{
					if (PosY > 0)
					{
						ClearInertia();
						AddSpeedX(-FinalHitPushbackX * 2/ 3);
					}
					else
					{
						SetInertia(-FinalHitPushbackX * 2/ 3);
					}
				}
			}
			break;
		case HACT_Crumple:
			OtherChar->Untech = FinalUntech;
			OtherChar->Hitstun = -1;
			OtherChar->KnockdownTime = -1;
			OtherChar->SetInertia(-FinalHitPushbackX);
			if (OtherChar->TouchingWall)
			{
				if (IsPlayer && Player != nullptr)
				{
					if (PosY > 0)
					{
						ClearInertia();
						AddSpeedX(-FinalHitPushbackX * 2/ 3);
					}
					else
					{
						SetInertia(-FinalHitPushbackX * 2/ 3);
					}
				}
			}
			break;
		case HACT_AirNormal:
		case HACT_AirFaceUp:
		case HACT_AirVertical:
		case HACT_AirFaceDown:
			OtherChar->CurrentGroundBounceEffect = InHitEffect.CurGroundBounceEffect;
			OtherChar->CurrentWallBounceEffect = InHitEffect.CurWallBounceEffect;
			OtherChar->Untech = FinalUntech;
			OtherChar->Hitstun = -1;
			OtherChar->KnockdownTime = InHitEffect.KnockdownTime;
			OtherChar->ClearInertia();
			OtherChar->SetSpeedX(-FinalAirHitPushbackX);
			if (OtherChar->TouchingWall)
			{
				if (IsPlayer && Player != nullptr)
				{
					if (PosY > 0)
					{
						ClearInertia();
						AddSpeedX(-FinalHitPushbackX * 2/ 3);
					}
					else
					{
						SetInertia(-FinalHitPushbackX * 2/ 3);
					}
				}
			}
			OtherChar->SetSpeedY(FinalAirHitPushbackY);
			if (FinalAirHitPushbackY <= 0 && OtherChar->PosY <= 0)
				OtherChar->PosY = 1;
			break;
		case HACT_Blowback:
			OtherChar->CurrentGroundBounceEffect = InHitEffect.CurGroundBounceEffect;
			OtherChar->CurrentWallBounceEffect = InHitEffect.CurWallBounceEffect;
			OtherChar->Untech = FinalUntech;
			OtherChar->Hitstun = -1;
			OtherChar->KnockdownTime = InHitEffect.KnockdownTime;
			OtherChar->ClearInertia();
			OtherChar->SetSpeedX(-FinalAirHitPushbackX * 3 / 2);
			if (OtherChar->TouchingWall)
			{
				if (IsPlayer && Player != nullptr)
				{
					if (PosY > 0)
					{
						ClearInertia();
						AddSpeedX(-FinalHitPushbackX * 2/ 3);
					}
					else
					{
						SetInertia(-FinalHitPushbackX * 2/ 3);
					}
				}
			}
			OtherChar->SetSpeedY(FinalAirHitPushbackY * 2);
			if (FinalAirHitPushbackY != 0 && OtherChar->PosY <= 0)
				OtherChar->PosY = 1000;
		default:
			break;
		}
		OtherChar->ReceivedHitAction = InHitEffect.GroundHitAction;
		OtherChar->ReceivedAttackLevel = InHitEffect.AttackLevel;
	}
	else
	{
		switch (InHitEffect.AirHitAction)
		{
		case HACT_GroundNormal:
		case HACT_ForceCrouch:
		case HACT_ForceStand:
			OtherChar->Hitstun = FinalHitstun;
			OtherChar->Untech = -1;
			OtherChar->SetInertia(-FinalHitPushbackX);
			if (OtherChar->TouchingWall)
			{
				if (IsPlayer && Player != nullptr)
				{
					if (PosY > 0)
					{
						ClearInertia();
						AddSpeedX(-FinalHitPushbackX * 2/ 3);
					}
					else
					{
						SetInertia(-FinalHitPushbackX * 2/ 3);
					}
				}
			}
			break;
		case HACT_Crumple:
			OtherChar->Untech = FinalUntech;
			OtherChar->Hitstun = -1;
			OtherChar->KnockdownTime = -1;
			OtherChar->SetInertia(-FinalHitPushbackX);
			if (OtherChar->TouchingWall)
			{
				if (IsPlayer && Player != nullptr)
				{
					if (PosY > 0)
					{
						ClearInertia();
						AddSpeedX(-FinalHitPushbackX * 2/ 3);
					}
					else
					{
						SetInertia(-FinalHitPushbackX * 2/ 3);
					}
				}
			}
			break;
		case HACT_AirNormal:
		case HACT_AirFaceUp:
		case HACT_AirVertical:
		case HACT_AirFaceDown:
			OtherChar->CurrentGroundBounceEffect = InHitEffect.CurGroundBounceEffect;
			OtherChar->CurrentWallBounceEffect = InHitEffect.CurWallBounceEffect;
			OtherChar->Untech = FinalUntech;
			OtherChar->Hitstun = -1;
			OtherChar->KnockdownTime = InHitEffect.KnockdownTime;
			OtherChar->ClearInertia();
			OtherChar->SetSpeedX(-FinalAirHitPushbackX);
			if (OtherChar->TouchingWall)
			{
				if (IsPlayer && Player != nullptr)
				{
					if (PosY > 0)
					{
						ClearInertia();
						AddSpeedX(-FinalHitPushbackX * 2/ 3);
					}
					else
					{
						SetInertia(-FinalHitPushbackX * 2/ 3);
					}
				}
			}
			OtherChar->SetSpeedY(FinalAirHitPushbackY);
			if (NormalHitEffect.AirHitPushbackY <= 0 && OtherChar->PosY <= 0)
				OtherChar->PosY = 1;
			break;
		case HACT_Blowback:
			OtherChar->CurrentGroundBounceEffect = InHitEffect.CurGroundBounceEffect;
			OtherChar->CurrentWallBounceEffect = InHitEffect.CurWallBounceEffect;
			OtherChar->Untech = FinalUntech;
			OtherChar->Hitstun = -1;
			OtherChar->KnockdownTime = InHitEffect.KnockdownTime;
			OtherChar->ClearInertia();
			OtherChar->SetSpeedX(-FinalAirHitPushbackX * 3 / 2);
			if (OtherChar->TouchingWall)
			{
				if (IsPlayer && Player != nullptr)
				{
					if (PosY > 0)
					{
						ClearInertia();
						AddSpeedX(-FinalHitPushbackX * 2/ 3);
					}
					else
					{
						SetInertia(-FinalHitPushbackX * 2/ 3);
					}
				}
			}
			OtherChar->SetSpeedY(FinalAirHitPushbackY * 2);
			if (FinalAirHitPushbackY <= 0 && OtherChar->PosY <= 0)
				OtherChar->PosY = 1;
		default:
			break;
		}
		OtherChar->ReceivedHitAction = InHitEffect.AirHitAction;
		OtherChar->ReceivedAttackLevel = InHitEffect.AttackLevel;
		OtherChar->AirDashTimer = 0;
	}
					
	if (OtherChar->PosY <= 0 && OtherChar->KnockdownTime > 0)
	{
		OtherChar->IsKnockedDown = false;
		OtherChar->HasBeenOTG++;
		OtherChar->TotalProration = OtherChar->TotalProration * Player->OtgProration / 100;
	}				
	if (OtherChar->HasBeenOTG > GameState->MaxOtgCount)
	{
		OtherChar->ClearInertia();
		OtherChar->SetSpeedY(5000);
		OtherChar->SetSpeedX(-35000);
		OtherChar->Hitstun = -1;
		OtherChar->Untech = 999;
		OtherChar->KnockdownTime = 6;
		OtherChar->CurrentGroundBounceEffect = GroundBounceEffect();
		OtherChar->CurrentWallBounceEffect = WallBounceEffect();
		OtherChar->ReceivedHitAction = HACT_Blowback;
		OtherChar->ReceivedAttackLevel = 4;
	}
									
	if (strcmp(HitEffectName.GetString(), ""))
	{
		CreateCharaParticle(HitEffectName.GetString(), POS_Hit, Vector(0, 0), -InHitEffect.HitAngle);
		if (InHitEffect.AttackLevel < 1)
		{
			switch (InHitEffect.SFXType)
			{
			case HitSFXType::SFX_Kick:
				PlayCommonSound("HitMeleeAltS");
				break;
			case HitSFXType::SFX_Slash:
				PlayCommonSound("HitSlashS");
				break;
			case HitSFXType::SFX_Punch:
			default:
				PlayCommonSound("HitMeleeS");
				break;
			}
		}
		else if (InHitEffect.AttackLevel < 3)
		{
			switch (InHitEffect.SFXType)
			{
			case HitSFXType::SFX_Kick:
				PlayCommonSound("HitMeleeAltM");
				break;
			case HitSFXType::SFX_Slash:
				PlayCommonSound("HitSlashM");
				break;
			case HitSFXType::SFX_Punch:
			default:
				PlayCommonSound("HitMeleeM");
				break;
			}
		}
		else if (InHitEffect.AttackLevel < 4)
		{
			switch (InHitEffect.SFXType)
			{
			case HitSFXType::SFX_Kick:
				PlayCommonSound("HitMeleeAltL");
				break;
			case HitSFXType::SFX_Slash:
				PlayCommonSound("HitSlashL");
				break;
			case HitSFXType::SFX_Punch:
			default:
				PlayCommonSound("HitMeleeL");
				break;
			}
		}
		else 
		{
			switch (InHitEffect.SFXType)
			{
			case HitSFXType::SFX_Kick:
				PlayCommonSound("HitMeleeAltXL");
				break;
			case HitSFXType::SFX_Slash:
				PlayCommonSound("HitSlashL");
				break;
			case HitSFXType::SFX_Punch:
			default:
				PlayCommonSound("HitMeleeXL");
				break;
			}
		}								    
	}
	else if (ObjectState != nullptr)
	{
		if (ObjectState->Type == StateType::SpecialAttack || ObjectState->Type == StateType::SuperAttack)
		{
			CreateCommonParticle("cmn_hit_sp", POS_Hit, Vector(0, 0), -NormalHitEffect.HitAngle);
			if (InHitEffect.AttackLevel < 1)
			{
				switch (InHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltS");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashS");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeS");
					break;
				}
			}
			else if (InHitEffect.AttackLevel < 3)
			{
				switch (InHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltM");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashM");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeM");
					break;
				}
			}
			else if (InHitEffect.AttackLevel < 4)
			{
				switch (InHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltL");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashL");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeL");
					break;
				}
			}
			else 
			{
				switch (InHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltXL");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashL");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeXL");
					break;
				}
			}
		}
		else
		{
			if (InHitEffect.AttackLevel < 1)
			{
				CreateCommonParticle("cmn_hit_s", POS_Hit, Vector(0, 0), -NormalHitEffect.HitAngle);
				switch (InHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltS");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashS");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeS");
					break;
				}
			}
			else if (InHitEffect.AttackLevel < 3)
			{
				CreateCommonParticle("cmn_hit_m", POS_Hit, Vector(0, 0), -NormalHitEffect.HitAngle);
				switch (InHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltM");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashM");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeM");
					break;
				}
			}
			else if (InHitEffect.AttackLevel < 4)
			{
				CreateCommonParticle("cmn_hit_l", POS_Hit, Vector(0, 0), -NormalHitEffect.HitAngle);
				switch (InHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltL");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashL");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeL");
					break;
				}
			}
			else 
			{
				CreateCommonParticle("cmn_hit_l", POS_Hit, Vector(0, 0), -NormalHitEffect.HitAngle);
				switch (InHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltXL");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashL");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeXL");
					break;
				}
			}								    
		}
	}
	else if (IsPlayer)
	{
		if (Player->CurStateMachine.CurrentState->Type == StateType::SpecialAttack || Player->CurStateMachine.CurrentState->Type == StateType::SuperAttack)
		{
			CreateCommonParticle("cmn_hit_sp", POS_Hit, Vector(0, 0), -NormalHitEffect.HitAngle);
			if (InHitEffect.AttackLevel < 1)
			{
				switch (InHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltS");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashS");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeS");
					break;
				}
			}
			else if (InHitEffect.AttackLevel < 3)
			{
				switch (InHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltM");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashM");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeM");
					break;
				}
			}
			else if (InHitEffect.AttackLevel < 4)
			{
				switch (InHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltL");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashL");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeL");
					break;
				}
			}
			else 
			{
				switch (NormalHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltXL");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashL");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeXL");
					break;
				}
			}								    
		}
		else
		{
			if (InHitEffect.AttackLevel < 1)
			{
				CreateCommonParticle("cmn_hit_s", POS_Hit, Vector(0, 0), -NormalHitEffect.HitAngle);
				switch (InHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltS");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashS");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeS");
					break;
				}
			}
			else if (InHitEffect.AttackLevel < 3)
			{
				CreateCommonParticle("cmn_hit_m", POS_Hit, Vector(0, 0), -NormalHitEffect.HitAngle);
				switch (InHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltM");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashM");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeM");
					break;
				}
			}
			else if (InHitEffect.AttackLevel < 4)
			{
				CreateCommonParticle("cmn_hit_l", POS_Hit, Vector(0, 0), -NormalHitEffect.HitAngle);
				switch (InHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltL");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashL");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeL");
					break;
				}
			}
			else 
			{
				CreateCommonParticle("cmn_hit_l", POS_Hit, Vector(0, 0), -NormalHitEffect.HitAngle);
				switch (InHitEffect.SFXType)
				{
				case HitSFXType::SFX_Kick:
					PlayCommonSound("HitMeleeAltXL");
					break;
				case HitSFXType::SFX_Slash:
					PlayCommonSound("HitSlashL");
					break;
				case HitSFXType::SFX_Punch:
				default:
					PlayCommonSound("HitMeleeXL");
					break;
				}
			}								    
		}
	}
}

void BattleActor::HandleClashCollision(BattleActor* OtherObj)
{
	if (IsAttacking && HitActive && OtherObj != Player && OtherObj->IsAttacking && OtherObj->HitActive)
	{
		for (int32_t i = 0; i < CollisionArraySize; i++)
		{
			BoxType Hitbox;
			if (CollisionBoxes[i].Type == Hitbox)
			{
				for (int32_t j = 0; j < CollisionArraySize; j++)
				{
					if (OtherObj->CollisionBoxes[j].Type == BoxType::Hitbox)
					{
						CollisionBox Hitbox = CollisionBoxes[i];

						CollisionBox OtherHitbox = OtherObj->CollisionBoxes[j];

						if (FacingRight)
						{
							Hitbox.PosX += PosX;
						}
						else
						{
							Hitbox.PosX = -Hitbox.PosX + PosX;  
						}
						Hitbox.PosY += PosY;
						if (OtherObj->FacingRight)
						{
							OtherHitbox.PosX += OtherObj->PosX;
						}
						else
						{
							OtherHitbox.PosX = -OtherHitbox.PosX + OtherObj->PosX;  
						}
						OtherHitbox.PosY += OtherObj->PosY;
							
						if (Hitbox.PosY + Hitbox.SizeY / 2 >= OtherHitbox.PosY - OtherHitbox.SizeY / 2
							&& Hitbox.PosY - Hitbox.SizeY / 2 <= OtherHitbox.PosY + OtherHitbox.SizeY / 2
							&& Hitbox.PosX + Hitbox.SizeX / 2 >= OtherHitbox.PosX - OtherHitbox.SizeX / 2
							&& Hitbox.PosX - Hitbox.SizeX / 2 <= OtherHitbox.PosX + OtherHitbox.SizeX / 2)
						{
							int32_t CollisionDepthX;
							if (Hitbox.PosX < OtherHitbox.PosX)
								CollisionDepthX = OtherHitbox.PosX - OtherHitbox.SizeX / 2 - (Hitbox.PosX + Hitbox.SizeX / 2);
							else
								CollisionDepthX = Hitbox.PosX - Hitbox.SizeX / 2 - (OtherHitbox.PosX + OtherHitbox.SizeX / 2);
							int32_t CollisionDepthY;
							if (Hitbox.PosY < OtherHitbox.PosY)
								CollisionDepthY = OtherHitbox.PosY - OtherHitbox.SizeY / 2 - (Hitbox.PosY + Hitbox.SizeY / 2);
							else
								CollisionDepthY = Hitbox.PosY - Hitbox.SizeY / 2 - (OtherHitbox.PosY + OtherHitbox.SizeY / 2);
							HitPosX = Hitbox.PosX - CollisionDepthX / 2;
							HitPosY = Hitbox.PosY - CollisionDepthY / 2;

							CreateCommonParticle("cmn_hit_clash", POS_Hit, Vector(0, 0), 0);
							
							if (IsPlayer && OtherObj->IsPlayer)
							{
								Hitstop = 16;
								OtherObj->Hitstop = 16;
								HitActive = false;
								OtherObj->HitActive = false;
								OtherObj->HitPosX = HitPosX;
								OtherObj->HitPosY = HitPosY;
								Player->EnableAttacks();
								OtherObj->Player->EnableAttacks();
								OtherObj->Player->CurStateMachine.CurrentState->OnHitOrBlock();
								Player->CurStateMachine.CurrentState->OnHitOrBlock();
								return;
							}
							if (!IsPlayer && !OtherObj->IsPlayer)
							{
								OtherObj->Hitstop = 16;
								Hitstop = 16;
								OtherObj->HitActive = false;
								HitActive = false;
								OtherObj->HitPosX = HitPosX;
								OtherObj->HitPosY = HitPosY;
								OtherObj->ObjectState->OnHitOrBlock();
								ObjectState->OnHitOrBlock();
								CreateCommonParticle("cmn_hit_clash", POS_Hit, Vector(0,0), 0);
                                PlayCommonSound("HitClash");
								return;
							}
							return;
						}
					}
				}
			}
		}
	}
}

void BattleActor::HandleFlip()
{
	bool CurrentFacing = FacingRight;
	if (!Player->Enemy) return;
	if (PosX < Player->Enemy->PosX)
	{
		SetFacing(true);
	}
	else if (PosX > Player->Enemy->PosX)
	{
		SetFacing(false);
	}
	if (CurrentFacing != FacingRight)
	{
		SpeedX = -SpeedX;
		Inertia = -Inertia;
		if (IsPlayer)
		{
			Player->CurInputBuffer.FlipInputsInBuffer();
			if (Player->CurrentActionFlags == ACT_Standing && Player->CurrentEnableFlags & ENB_Standing)
				Player->JumpToState("StandFlip");
			else if (Player->CurrentActionFlags == ACT_Crouching && Player->CurrentEnableFlags & ENB_Crouching)
				Player->JumpToState("CrouchFlip");
			else if (Player->CurrentEnableFlags & ENB_Jumping)
				Player->JumpToState("JumpFlip");
		}
	}
}

void BattleActor::PosTypeToPosition(PosType Type, int32_t* OutPosX, int32_t* OutPosY)
{
	switch (Type)
	{
	case POS_Self:
		*OutPosX = PosX;
		*OutPosY = PosY;
		break;
	case POS_Player:
		*OutPosX = Player->PosX;
		*OutPosY = Player->PosY;
		break;
	case POS_Center:
		*OutPosX = PosX;
		*OutPosY = PosY + PushHeight;
		break;
	case POS_Enemy:
		*OutPosX = Player->Enemy->PosX;
		*OutPosY = Player->Enemy->PosY;
		break;
	case POS_Hit:
		*OutPosX = HitPosX;
		*OutPosY = HitPosY;
		break;
	default:
		break;
	}
}

void BattleActor::EnableHit(bool Enabled)
{
	HitActive = Enabled;
}

void BattleActor::EnableProrateOnce(bool Enabled)
{
	ProrateOnce = Enabled;
}

void BattleActor::SetPushCollisionActive(bool Active)
{
	PushCollisionActive = Active;
}

void BattleActor::SetAttacking(bool Attacking)
{
	IsAttacking = Attacking;
}

void BattleActor::SetHeadAttribute(bool HeadAttribute)
{
	AttackHeadAttribute = HeadAttribute;
}

void BattleActor::SetProjectileAttribute(bool ProjectileAttribute)
{
	AttackProjectileAttribute = ProjectileAttribute;
}

void BattleActor::SetHitEffect(HitEffect InHitEffect)
{
	NormalHitEffect = InHitEffect;
}

void BattleActor::SetCounterHitEffect(HitEffect InHitEffect)
{
	CounterHitEffect = InHitEffect;
}

void BattleActor::PauseRoundTimer(bool Pause)
{
	GameState->StoredBattleState.PauseTimer = Pause;
}

void BattleActor::SetObjectID(int32_t InObjectID)
{
	ObjectID = InObjectID;
}

void BattleActor::DeactivateIfBeyondBounds()
{
	if (IsPlayer)
		return;
	if (PosX > 2220000 + GameState->StoredBattleState.CurrentScreenPos || PosX < -2220000 + GameState->StoredBattleState.CurrentScreenPos)
		DeactivateObject();
}

void BattleActor::EnableDeactivateOnStateChange(bool Enable)
{
	DeactivateOnStateChange = Enable;
}

void BattleActor::EnableDeactivateOnReceiveHit(bool Enable)
{
	DeactivateOnReceiveHit = Enable;
}

void BattleActor::DeactivateObject()
{
	if (IsPlayer)
		return;
	ObjectState->OnExit();
	for (int32_t i = 0; i < 32; i++)
	{
		if (this == Player->ChildBattleActors[i])
		{
			Player->ChildBattleActors[i] = nullptr;
			break;
		}
	}
	for (int32_t i = 0; i < 16; i++)
	{
		if (this == Player->StoredBattleActors[i])
		{
			Player->StoredBattleActors[i] = nullptr;
			break;
		}
	}
	DeactivateOnNextUpdate = true;
}

void BattleActor::ResetObject()
{
	if (IsPlayer)
		return;
	DeactivateOnNextUpdate = false;
	IsActive = false;
	PosX = 0;
	PosY = 0;
	PrevPosX = 0;
	PrevPosY = 0;
	SpeedX = 0;
	SpeedY = 0;
	Gravity = 1900;
	Inertia = 0;
	ActionTime = -1;
	PushHeight = 0;
	PushHeightLow = 0;
	PushWidth = 0;
	PushWidthExpand = 0;
	Hitstop = 0;
	L = 0;
	R = 0;
	T = 0;
	B = 0;
	NormalHitEffect = HitEffect();
	CounterHitEffect = HitEffect();
	ClearHomingParam();
	HitActive = false;
	IsAttacking = false;
	AttackHeadAttribute = false;
	AttackProjectileAttribute = true;
	RoundStart = false;
	FacingRight = false;
	HasHit = false;
	SpeedXPercent = 100;
	SpeedXPercentPerFrame = false;
	SpeedYPercent = 100;
	SpeedYPercentPerFrame = false;
	ScreenCollisionActive = false;
	PushCollisionActive = false;
	ProrateOnce = false;
	StoredRegister = 0;
	StateVal1 = 0;
	StateVal2 = 0;
	StateVal3 = 0;
	StateVal4 = 0;
	StateVal5 = 0;
	StateVal6 = 0;
	StateVal7 = 0;
	StateVal8 = 0;
	MiscFlags = 0;
	IsPlayer = false;
	SuperFreezeTime = -1;
	CelNameInternal.SetString("");
	HitEffectName.SetString("");
	AnimTime = -1;
	HitPosX = 0;
	HitPosY = 0;
	DefaultCommonAction = true;
	for (int32_t i = 0; i < CollisionArraySize; i++)
	{
		CollisionBoxes[i] = CollisionBox();
	}
	ObjectStateName.SetString("");
	ObjectID = 0;
	Player = nullptr;
}

void BattleActor::SaveForRollback(unsigned char* Buffer)
{
	memcpy(Buffer, &ObjSync, SIZEOF_BATTLEACTOR);
}

void BattleActor::LoadForRollback(unsigned char* Buffer)
{
	memcpy(&ObjSync, Buffer, SIZEOF_BATTLEACTOR);
	if (!IsPlayer)
	{
		int Index = 0;
		for (CString<64> String : Player->ObjectStateNames)
		{
			if (!strcmp(String.GetString(), ObjectStateName.GetString()))
			{
				break;
			}
			Index++;
		}
		if (strcmp(Player->ObjectStateNames[Index].GetString(), ObjectStateName.GetString()))
		{
		    if (ObjectState != nullptr)
		    	delete ObjectState;
			ObjectState = Player->ObjectStates[Index]->Clone();
			ObjectState->ObjectParent = this;
		}
	}
}

void BattleActor::LogForSyncTest(FILE* file)
{
	if(file)
	{
		fprintf(file,"BattleActor:\n");
		fprintf(file,"\tPosX: %d\n", PosX);
		fprintf(file,"\tPosY: %d\n", PosY);
		fprintf(file,"\tPrevPosX: %d\n", PrevPosX);
		fprintf(file,"\tPrevPosY: %d\n", PrevPosY);
		fprintf(file,"\tSpeedX: %d\n", SpeedX);
		fprintf(file,"\tSpeedY: %d\n", SpeedY);
		fprintf(file,"\tGravity: %d\n", Gravity);
		fprintf(file,"\tInertia: %d\n", Inertia);
		fprintf(file,"\tActionTime: %d\n", ActionTime);
		fprintf(file,"\tPushHeight: %d\n", PushHeight);
		fprintf(file,"\tPushHeightLow: %d\n", PushHeightLow);
		fprintf(file,"\tPushWidth: %d\n", PushWidth);
		fprintf(file,"\tHitstop: %d\n", Hitstop);
		fprintf(file,"\tCelName: %s\n", CelNameInternal.GetString());
		fprintf(file,"\tHitActive: %d\n", HitActive);
		fprintf(file,"\tIsAttacking: %d\n", IsAttacking);
		fprintf(file,"\tFacingRight: %d\n", FacingRight);
		fprintf(file,"\tHasHit: %d\n", HasHit);
		fprintf(file,"\tMiscFlags: %d\n", MiscFlags);
		fprintf(file,"\tAnimTime: %d\n", AnimTime);
		fprintf(file,"\tDefaultCommonAction: %d\n", DefaultCommonAction);
	}
}

BattleActor* BattleActor::GetBattleActor(ObjType Type)
{
	switch (Type)
	{
	case OBJ_Self:
		return this;
	case OBJ_Enemy:
		return Player->Enemy;
	case OBJ_Parent:
		return Player;
	case OBJ_Child0:
		if (IsPlayer && Player->StoredBattleActors[0])
			if (Player->StoredBattleActors[0]->IsActive)
				return Player->StoredBattleActors[0];
		return nullptr;
	case OBJ_Child1:
		if (IsPlayer && Player->StoredBattleActors[1])
			if (Player->StoredBattleActors[1]->IsActive)
				return Player->StoredBattleActors[1];
		return nullptr;
	case OBJ_Child2:
		if (IsPlayer && Player->StoredBattleActors[2])
			if (Player->StoredBattleActors[2]->IsActive)
				return Player->StoredBattleActors[2];
		return nullptr;
	case OBJ_Child3:
		if (IsPlayer && Player->StoredBattleActors[3])
			if (Player->StoredBattleActors[3]->IsActive)
				return Player->StoredBattleActors[3];
		return nullptr;
	case OBJ_Child4:
		if (IsPlayer && Player->StoredBattleActors[4])
			if (Player->StoredBattleActors[4]->IsActive)
				return Player->StoredBattleActors[4];
		return nullptr;
	case OBJ_Child5:
		if (IsPlayer && Player->StoredBattleActors[5])
			if (Player->StoredBattleActors[5]->IsActive)
				return Player->StoredBattleActors[5];
		return nullptr;
	case OBJ_Child6:
		if (IsPlayer && Player->StoredBattleActors[6])
			if (Player->StoredBattleActors[6]->IsActive)
				return Player->StoredBattleActors[6];
		return nullptr;
	case OBJ_Child7:
		if (IsPlayer && Player->StoredBattleActors[7])
			if (Player->StoredBattleActors[7]->IsActive)
				return Player->StoredBattleActors[7];
		return nullptr;
	case OBJ_Child8:
		if (IsPlayer && Player->StoredBattleActors[8])
			if (Player->StoredBattleActors[8]->IsActive)
				return Player->StoredBattleActors[8];
		return nullptr;
	case OBJ_Child9:
		if (IsPlayer && Player->StoredBattleActors[9])
			if (Player->StoredBattleActors[9]->IsActive)
				return Player->StoredBattleActors[9];
		return nullptr;
	case OBJ_Child10:
		if (IsPlayer && Player->StoredBattleActors[10])
			if (Player->StoredBattleActors[10]->IsActive)
				return Player->StoredBattleActors[10];
		return nullptr;
	case OBJ_Child11:
		if (IsPlayer && Player->StoredBattleActors[11])
			if (Player->StoredBattleActors[11]->IsActive)
				return Player->StoredBattleActors[11];
		return nullptr;
	case OBJ_Child12:
		if (IsPlayer && Player->StoredBattleActors[12])
			if (Player->StoredBattleActors[12]->IsActive)
				return Player->StoredBattleActors[12];
		return nullptr;
	case OBJ_Child13:
		if (IsPlayer && Player->StoredBattleActors[13])
			if (Player->StoredBattleActors[13]->IsActive)
				return Player->StoredBattleActors[13];
		return nullptr;
	case OBJ_Child14:
		if (IsPlayer && Player->StoredBattleActors[14])
			if (Player->StoredBattleActors[14]->IsActive)
				return Player->StoredBattleActors[14];
		return nullptr;
	case OBJ_Child15:
		if (IsPlayer && Player->StoredBattleActors[15])
			if (Player->StoredBattleActors[15]->IsActive)
				return Player->StoredBattleActors[15];
		return nullptr;
	default:
		return nullptr;
	}
}



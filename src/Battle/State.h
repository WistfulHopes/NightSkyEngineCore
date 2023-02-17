#pragma once

#include "../CString.h"
#include "NightSkyScript/ScriptAnalyzer.h"
#include <vector>

class PlayerCharacter;
class BattleActor;

enum class EntryState //character state required to enter state
{
	None, //any
	Standing,
	Crouching,
	Jumping,
};

enum class StateType
{
	Standing,
	Crouching,
	NeutralJump,
	ForwardJump,
	BackwardJump,
	ForwardWalk,
	BackwardWalk,
	ForwardDash,
	BackwardDash,
	ForwardAirDash,
	BackwardAirDash,
	NormalAttack,
	NormalThrow,
	SpecialAttack,
	SuperAttack,
	Hitstun,
	Blockstun,
	Tech,
	Burst,
};

enum class StateCondition
{
	None,
	AirJumpOk,
	AirJumpMinimumHeight,
	AirDashOk,
	AirDashMinimumHeight,
	IsAttacking,
	HitstopCancel,
	IsStunned,
	CloseNormal,
	FarNormal,
	MeterNotZero,
	MeterQuarterBar,
	MeterHalfBar,
	MeterOneBar,
	MeterTwoBars,
	MeterThreeBars,
	MeterFourBars,
	MeterFiveBars,
	UniversalGaugeOneBar,
	UniversalGaugeTwoBars,
	UniversalGaugeThreeBars,
	UniversalGaugeFourBars,
	UniversalGaugeFiveBars,
	UniversalGaugeSixBars,
	PlayerVal1True,
	PlayerVal2True,
	PlayerVal3True,
	PlayerVal4True,
	PlayerVal5True,
	PlayerVal6True,
	PlayerVal7True,
	PlayerVal8True,
	PlayerVal1False,
	PlayerVal2False,
	PlayerVal3False,
	PlayerVal4False,
	PlayerVal5False,
	PlayerVal6False,
	PlayerVal7False,
	PlayerVal8False,
};

enum class InputMethod : uint8_t
{
	Normal,
	Strict,
	Once,
	OnceStrict,
};

struct InputBitmask
{
	InputBitmask()
	{
		InputFlag = InputNone;
	};
	InputBitmask(InputFlags Input)
	{
		InputFlag = Input;
	};

	int InputFlag;
};

struct InputCondition
{
	std::vector<InputBitmask> Sequence;
	int Lenience = 8;
	int ImpreciseInputCount = 0;
	bool bInputAllowDisable = true;
	InputMethod Method = InputMethod::Normal;
};

struct InputConditionList
{
	std::vector<InputCondition> InputConditions;
};

class State
{
public:
	virtual State* Clone() = 0;
	virtual ~State() = default;
	PlayerCharacter* Parent;
	BattleActor* ObjectParent;
	CString<64> Name;
	EntryState StateEntryState;
	std::vector<InputConditionList> InputConditionList;
	StateType Type;
	std::vector<StateCondition> StateConditions;
	bool IsFollowupState;
	int32_t ObjectID;
	
	virtual void OnEnter() = 0; //executes on enter. write in script
	virtual void OnUpdate(float DeltaTime) = 0; //executes every frame. write in script
	virtual void OnExit() = 0; //executes on exit. write in script
	virtual void OnLanding() = 0; //executes on landing. write in script
	virtual void OnHit() = 0; //executes on hit. write in script
	virtual void OnBlock() = 0; //executes on hit. write in script
	virtual void OnHitOrBlock() = 0; //executes on hit. write in script
    virtual void OnCounterHit() = 0; //executes on counter hit. write in script
	virtual void OnSuperFreeze() = 0; //executes on super freeze. write in script
	virtual void OnSuperFreezeEnd() = 0; //executes on super freeze. write in script
};

struct ScriptBlockOffsets 
{
	uint32_t OnEnterOffset = -1;
	uint32_t OnUpdateOffset = -1;
	uint32_t OnExitOffset = -1;
	uint32_t OnLandingOffset = -1;
	uint32_t OnHitOffset = -1;
	uint32_t OnBlockOffset = -1;
	uint32_t OnHitOrBlockOffset = -1;
	uint32_t OnCounterHitOffset = -1;
	uint32_t OnSuperFreezeOffset = -1;
	uint32_t OnSuperFreezeEndOffset = -1;
};

class ScriptState : public State
{
public:
	virtual ScriptState* Clone() override;
	
	ScriptState* ParentState;
	uint32_t OffsetAddress;
	uint32_t Size;
	ScriptBlockOffsets Offsets;
	bool CommonState;
	
	virtual void OnEnter() override; //executes on enter. write in script
	virtual void OnUpdate(float DeltaTime) override; //executes every frame. write in script
	virtual void OnExit() override; //executes on exit. write in script
	virtual void OnLanding() override; //executes on landing. write in script
	virtual void OnHit() override; //executes on hit. write in script
	virtual void OnBlock() override; //executes on hit. write in script
	virtual void OnHitOrBlock() override; //executes on hit. write in script
    virtual void OnCounterHit() override; //executes on counter hit. write in script
	virtual void OnSuperFreeze() override; //executes on super freeze. write in script
	virtual void OnSuperFreezeEnd() override; //executes on super freeze. write in script
};

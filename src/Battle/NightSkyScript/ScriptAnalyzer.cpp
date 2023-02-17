#include "ScriptAnalyzer.h"
#include "../Actors/PlayerCharacter.h"

void ScriptAnalyzer::Initialize(char *Addr, uint32_t Size, std::vector<State *> *States, std::vector<Subroutine *> *Subroutines)
{
    DataAddress = Addr;
    StateCount = *reinterpret_cast<int *>(Addr);
    SubroutineCount = *reinterpret_cast<int *>(Addr + 4);
    StateAddresses = reinterpret_cast<StateAddress *>(Addr + 8);
    SubroutineAddresses = &StateAddresses[StateCount];
    ScriptAddress = (char *)&SubroutineAddresses[SubroutineCount];

    for (int i = 0; i < StateCount; i++)
    {
        ScriptState *NewState = new ScriptState();
        NewState->Name = StateAddresses[i].Name;
        NewState->OffsetAddress = StateAddresses[i].OffsetAddress;
        uint32_t StateSize;
        if (i == StateCount - 1)
        {
            StateSize = Size - NewState->OffsetAddress;
        }
        else
        {
            StateSize = StateAddresses[i + 1].OffsetAddress - NewState->OffsetAddress;
        }
        InitStateOffsets(reinterpret_cast<char *>(NewState->OffsetAddress) + (uint64_t)ScriptAddress, StateSize, NewState); // NOLINT(performance-no-int-to-ptr)
        States->push_back(NewState);
    }
    for (int i = 0; i < SubroutineCount; i++)
    {
        ScriptSubroutine *NewSubroutine = new ScriptSubroutine;
        NewSubroutine->Name = SubroutineAddresses[i].Name;
        NewSubroutine->OffsetAddress = SubroutineAddresses[i].OffsetAddress;
        Subroutines->push_back(NewSubroutine);
    }
}

void ScriptAnalyzer::InitStateOffsets(char *Addr, uint32_t Size, ScriptState *State)
{
    while (true)
    {
        OpCodes code = *reinterpret_cast<OpCodes *>(Addr);
		if (code == OPC_OnEnter)
			State->Offsets.OnEnterOffset = Addr - ScriptAddress;
		else if (code == OPC_OnUpdate)
			State->Offsets.OnUpdateOffset = Addr - ScriptAddress;
		else if (code == OPC_OnExit)
			State->Offsets.OnExitOffset = Addr - ScriptAddress;
		else if (code == OPC_OnLanding)
			State->Offsets.OnLandingOffset = Addr - ScriptAddress;\
		else if (code == OPC_OnHit)
			State->Offsets.OnHitOffset = Addr - ScriptAddress;
		else if (code == OPC_OnBlock)
			State->Offsets.OnBlockOffset = Addr - ScriptAddress;
		else if (code == OPC_OnHitOrBlock)
			State->Offsets.OnHitOrBlockOffset = Addr - ScriptAddress;
		else if (code == OPC_OnCounterHit)
			State->Offsets.OnCounterHitOffset = Addr - ScriptAddress;
		else if (code == OPC_OnSuperFreeze)
			State->Offsets.OnSuperFreezeOffset = Addr - ScriptAddress;
		else if (code == OPC_OnSuperFreezeEnd)
			State->Offsets.OnSuperFreezeEndOffset = Addr - ScriptAddress;
		else if (code == OPC_EndState)
			return;
		Addr += OpCodeSizes[code];
    }
}

void ScriptAnalyzer::Analyze(char *Addr, BattleActor *Actor)
{
    Addr += (uint64_t)ScriptAddress;
    bool CelExecuted = false;
    std::vector<StateAddress> Labels;
    GetAllLabels(Addr, &Labels);
    State *StateToModify = nullptr;
    char* ElseAddr = 0;
    while (true)
    {
        OpCodes code = *reinterpret_cast<OpCodes*>(Addr);
        switch (code)
        {
        case OPC_SetCel:
            {
                if (CelExecuted)
                    return;
                int32_t AnimTime = *reinterpret_cast<int32_t*>(Addr + 68);
                if (Actor->AnimTime == AnimTime)
                {
                    Actor->SetCelName(Addr + 4);
                    CelExecuted = true;
                }
                else if (Actor->AnimTime > AnimTime)
                {
                    while (Actor->AnimTime > AnimTime)
                    {
                        char* BakAddr = Addr;
                        Addr += OpCodeSizes[code];
                        if (FindNextCel(&Addr, Actor->AnimTime))
                        {
                            AnimTime = *reinterpret_cast<int32_t*>(Addr + 68);
                            if (Actor->AnimTime == AnimTime)
                            {
                                Actor->SetCelName(Addr + 4);
                                CelExecuted = true;
                                break;
                            }
                            continue;
                        }
                        Addr = BakAddr;
                        break;
                    }
                    break;
                }
                else
                {
                    return;
                }
                break;
            }
        case OPC_CallSubroutine:
        {
            Actor->Player->CallSubroutine(Addr + 4);
            break;
        }
        case OPC_ExitState:
        {
            if (Actor->IsPlayer)
            {
                switch (Actor->Player->CurrentActionFlags)
                {
                case ACT_Standing:
                    Actor->Player->JumpToState("Stand");
                    return;
                case ACT_Crouching:
                    Actor->Player->JumpToState("Crouch");
                    return;
                case ACT_Jumping:
                    Actor->Player->JumpToState("VJump");
                    return;
                default:
                    return;
                }
            }
        }
        case OPC_EndBlock:
        {
            return;
        }
        case OPC_GotoLabel:
        {
            CString<64> LabelName;
            LabelName.SetString(Addr + 4);
            for (StateAddress Label : Labels)
            {
                if (!strcmp(Label.Name.GetString(), LabelName.GetString()))
                {
                    Addr = ScriptAddress + Label.OffsetAddress;
                    char* CelAddr = Addr;
                    if (FindNextCel(&CelAddr, Actor->AnimTime))
                    {
                        Addr = CelAddr;
                        Actor->AnimTime = *reinterpret_cast<int32_t *>(Addr + 68);
                        Actor->SetCelName(Addr + 4);
                        code = OPC_SetCel;
                    }
                    break;
                }
            }
            break;
        }
        case OPC_EndLabel:
        {
            int32_t AnimTime = *reinterpret_cast<int32_t *>(Addr + 4);
            if (Actor->AnimTime < AnimTime)
                return;
            break;
        }
        case OPC_BeginStateDefine:
        {
            CString<64> StateName;
            StateName.SetString(Addr + 4);
            int32_t Index = Actor->Player->StateMachine.GetStateIndex(StateName);
            if (Index != -1)
                StateToModify = Actor->Player->StateMachine.States[Index];
            break;
        }
        case OPC_EndStateDefine:
            StateToModify = nullptr;
            break;
        case OPC_SetStateType:
            if (StateToModify)
            {
                StateToModify->Type = *reinterpret_cast<StateType*>(Addr + 4);
            }
            break;
        case OPC_SetEntryState:
            if (StateToModify)
            {
                StateToModify->StateEntryState = *reinterpret_cast<EntryState*>(Addr + 4);
            }
            break;
        case OPC_AddInputCondition:
            if (StateToModify)
            {
                if (StateToModify->InputConditionList.size() == 0)
                    StateToModify->InputConditionList.push_back(InputConditionList());
                InputCondition Condition;
                for (int i = 0; i < 32; i++)
                {
                    if (Actor->Player->SavedInputCondition.Sequence[i].InputFlag != InputNone)
                    {
                        Condition.Sequence.push_back(Actor->Player->SavedInputCondition.Sequence[i]);
                        continue;
                    }
                    break;
                }
                Condition.Lenience = Actor->Player->SavedInputCondition.Lenience;
                Condition.ImpreciseInputCount = Actor->Player->SavedInputCondition.ImpreciseInputCount;
                Condition.bInputAllowDisable = Actor->Player->SavedInputCondition.bInputAllowDisable;
                Condition.Method = Actor->Player->SavedInputCondition.Method;
                StateToModify->InputConditionList[StateToModify->InputConditionList.size() - 1].InputConditions.push_back(Condition);
                break;
            }
        case OPC_AddInputConditionList:
            if (StateToModify)
            {
                StateToModify->InputConditionList.push_back(InputConditionList());
            }
            break;
        case OPC_AddStateCondition:
            if (StateToModify)
            {
                StateToModify->StateConditions.push_back(*reinterpret_cast<StateCondition*>(Addr + 4));
            }
            break;
        case OPC_IsFollowupMove:
            if (StateToModify)
            {
                StateToModify->IsFollowupState = *reinterpret_cast<bool *>(Addr + 4);
            }
            break;
        case OPC_SetStateObjectID:
            if (StateToModify)
            {
                StateToModify->ObjectID = *reinterpret_cast<int32_t *>(Addr + 4);
            }
            break;
        case OPC_BeginState:
            break;
        case OPC_EndState:
            return;
        case OPC_BeginSubroutine:
            break;
        case OPC_EndSubroutine:
            return;
        case OPC_CallSubroutineWithArgs:
            break;
        case OPC_OnEnter:
            break;
        case OPC_OnUpdate:
            break;
        case OPC_OnExit:
            break;
        case OPC_OnLanding:
            break;
        case OPC_OnHit:
            break;
        case OPC_OnBlock:
            break;
        case OPC_OnHitOrBlock:
            break;
        case OPC_OnCounterHit:
            break;
        case OPC_OnSuperFreeze:
            break;
        case OPC_OnSuperFreezeEnd:
            break;
        case OPC_BeginLabel:
            break;
        case OPC_If:
        {
            int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
            if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
            {
                Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
            }
            if (Operand != 0)
            {
                break;
            }

            FindMatchingEnd(&Addr, OPC_EndIf);
            ElseAddr = Addr;
            FindElse(&ElseAddr);
            code = OPC_EndIf;
            break;
        }
        case OPC_IsOnFrame:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                if (Actor->IsOnFrame(Operand))
                    Actor->StoredRegister = 1;
                else
                    Actor->StoredRegister = 0;
                break;
            }
        case OPC_EndIf:
            break;
        case OPC_IfOperation:
        {
            int32_t Operand1 = *reinterpret_cast<int32_t *>(Addr + 12);
            if (*reinterpret_cast<int32_t *>(Addr + 8) > 0)
            {
                Operand1 = Actor->GetInternalValue(static_cast<InternalValue>(Operand1));
            }
            int32_t Operand2 = *reinterpret_cast<int32_t *>(Addr + 20);
            if (*reinterpret_cast<int32_t *>(Addr + 16) > 0)
            {
                Operand2 = Actor->GetInternalValue(static_cast<InternalValue>(Operand2));
            }
            Operation Op = *reinterpret_cast<Operation *>(Addr + 4);
            CheckOperation(Op, Operand1, Operand2, &Actor->StoredRegister);
            if (Actor->StoredRegister != 0)
            {
                break;
            }

            FindMatchingEnd(&Addr, OPC_EndIf);
            ElseAddr = Addr;
            FindElse(&ElseAddr);
            code = OPC_EndIf;
            break;
        }
        case OPC_IfNot:
        {
            int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
            if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
            {
                Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
            }
            if (Operand == 0)
            {
                break;
            }
                
            FindMatchingEnd(&Addr, OPC_EndIf);
            ElseAddr = Addr;
            FindElse(&ElseAddr);
            code = OPC_EndIf;
            break;
        };
        case OPC_Else:
            if (ElseAddr == Addr)
            {
                ElseAddr = nullptr;
                break;
            }
            else
            {
                FindMatchingEnd(&Addr, OPC_EndElse);
                code = OPC_EndElse;
                break;
            }
        case OPC_EndElse:
            break;
        case OPC_GotoLabelIf:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8 + 64);
                if (*reinterpret_cast<int32_t *>(Addr + 4 + 64) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                if (Operand != 0)
                {
                    CString<64> LabelName;
                    LabelName.SetString(Addr + 4);
                    for (StateAddress Label : Labels)
                    {
                        if (!strcmp(Label.Name.GetString(), LabelName.GetString()))
                        {
                            Addr = ScriptAddress + Label.OffsetAddress;
                            char* CelAddr = Addr;
                            if (FindNextCel(&CelAddr, Actor->AnimTime))
                            {
                                Addr = CelAddr;
                                Actor->AnimTime = *reinterpret_cast<int32_t *>(Addr + 68) - 1;
                                Actor->SetCelName(Addr + 4);
                                code = OPC_SetCel;
                            }
                            break;
                        }
                    }
                    break;
                }
                break;
            }
        case OPC_GotoLabelIfOperation:
            {
                int32_t Operand1 = *reinterpret_cast<int32_t *>(Addr + 12 + 64);
                if (*reinterpret_cast<int32_t *>(Addr + 8 + 64) > 0)
                {
                    Operand1 = Actor->GetInternalValue(static_cast<InternalValue>(Operand1));
                }
                int32_t Operand2 = *reinterpret_cast<int32_t *>(Addr + 20 + 64);
                if (*reinterpret_cast<int32_t *>(Addr + 16 + 64) > 0)
                {
                    Operand2 = Actor->GetInternalValue(static_cast<InternalValue>(Operand2));
                }
                Operation Op = *reinterpret_cast<Operation *>(Addr + 4 + 64);
                CheckOperation(Op, Operand1, Operand2, &Actor->StoredRegister);
                if (Actor->StoredRegister != 0)
                {
                    CString<64> LabelName;
                    LabelName.SetString(Addr + 4);
                    for (StateAddress Label : Labels)
                    {
                        if (!strcmp(Label.Name.GetString(), LabelName.GetString()))
                        {
                            Addr = ScriptAddress + Label.OffsetAddress;
                            char* CelAddr = Addr;
                            if (FindNextCel(&CelAddr, Actor->AnimTime))
                            {
                                Addr = CelAddr;
                                Actor->AnimTime = *reinterpret_cast<int32_t *>(Addr + 68) - 1;
                                Actor->SetCelName(Addr + 4);
                                code = OPC_SetCel;
                            }
                            break;
                        }
                    }
                    break;
                }
                break;
            }
        case OPC_GotoLabelIfNot:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8 + 64);
                if (*reinterpret_cast<int32_t *>(Addr + 4 + 64) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                if (Operand == 0)
                {
                    CString<64> LabelName;
                    LabelName.SetString(Addr + 4);
                    for (StateAddress Label : Labels)
                    {
                        if (!strcmp(Label.Name.GetString(), LabelName.GetString()))
                        {
                            Addr = ScriptAddress + Label.OffsetAddress;
                            char* CelAddr = Addr;
                            if (FindNextCel(&CelAddr, Actor->AnimTime))
                            {
                                Addr = CelAddr;
                                Actor->AnimTime = *reinterpret_cast<int32_t *>(Addr + 68) - 1;
                                Actor->SetCelName(Addr + 4);
                                code = OPC_SetCel;
                            }
                            break;
                        }
                    }
                    break;
                }
                break;
            }
        case OPC_GetPlayerStats:
        {
            PlayerStats Stat = *reinterpret_cast<PlayerStats*>(Addr + 4);
            int32_t Val = 0;
            switch(Stat)
            {
            case PLY_FWalkSpeed:
                Val = Actor->Player->FWalkSpeed;
                break;
            case PLY_BWalkSpeed:
                Val = -Actor->Player->BWalkSpeed;
                break;
            case PLY_FDashInitSpeed:
                Val = Actor->Player->FDashInitSpeed;
                break;
            case PLY_FDashAccel:
                Val = Actor->Player->FDashAccel;
                break;
            case PLY_FDashMaxSpeed:
                Val = Actor->Player->FDashMaxSpeed;
                break;
            case PLY_FDashFriction:
                Val = Actor->Player->FDashFriction;
                break;
            case PLY_BDashSpeed:
                Val = -Actor->Player->BDashSpeed;
                break;
            case PLY_BDashHeight:
                Val = Actor->Player->BDashHeight;
                break;
            case PLY_BDashGravity:
                Val = Actor->Player->BDashGravity;
                break;
            case PLY_JumpHeight:
                Val = Actor->Player->JumpHeight;
                break;
            case PLY_FJumpSpeed:
                Val = Actor->Player->FJumpSpeed;
                break;
            case PLY_BJumpSpeed:
                Val = -Actor->Player->BJumpSpeed;
                break;
            case PLY_JumpGravity:
                Val = Actor->Player->JumpGravity;
                break;
            case PLY_SuperJumpHeight:
                Val = Actor->Player->SuperJumpHeight;
                break;
            case PLY_FSuperJumpSpeed:
                Val = Actor->Player->FSuperJumpSpeed;
                break;
            case PLY_BSuperJumpSpeed:
                Val = -Actor->Player->BSuperJumpSpeed;
                break;
            case PLY_SuperJumpGravity:
                Val = Actor->Player->SuperJumpGravity;
                break;
            case PLY_FAirDashSpeed:
                Val = Actor->Player->FAirDashSpeed;
                break;
            case PLY_BAirDashSpeed:
                Val = -Actor->Player->BAirDashSpeed;
                break;
            }
            Actor->StoredRegister = Val;
            break;
        }
        case OPC_SetPosX:
        {
            int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
            if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
            {
                Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
            }
            Actor->SetPosX(Operand);
            break;
        }
        case OPC_AddPosX:
        {
            int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
            if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
            {
                Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
            }
            Actor->AddPosX(Operand);
            break;
        }
        case OPC_AddPosXRaw:
        {
            int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
            if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
            {
                Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
            }
            Actor->AddPosXRaw(Operand);
            break;
        }
        case OPC_SetPosY:
        {
            int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
            if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
            {
                Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
            }
            Actor->SetPosY(Operand);
            break;
        }
        case OPC_AddPosY:
        {
            int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
            if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
            {
                Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
            }
            Actor->AddPosY(Operand);
            break;
        }
        case OPC_SetSpeedX:
        {
            int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
            if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
            {
                Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
            }
            Actor->SetSpeedX(Operand);
            break;
        }
        case OPC_AddSpeedX:
        {
            int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
            if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
            {
                Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
            }
            Actor->AddSpeedX(Operand);
            break;
        }
        case OPC_SetSpeedY:
        {
            int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
            if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
            {
                Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
            }
            Actor->SetSpeedY(Operand);
            break;
        }
        case OPC_AddSpeedY:
        {
            int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
            if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
            {
                Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
            }
            Actor->AddSpeedY(Operand);
            break;
        }
        case OPC_SetSpeedXPercent:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->SetSpeedXPercent(Operand);
                break;
            }
        case OPC_SetSpeedXPercentPerFrame:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->SetSpeedXPercentPerFrame(Operand);
                break;
            }
        case OPC_SetSpeedYPercent:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->SetSpeedYPercent(Operand);
                break;
            }
        case OPC_SetSpeedYPercentPerFrame:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->SetSpeedYPercentPerFrame(Operand);
                break;
            }
        case OPC_SetGravity:
        {
            int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
            if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
            {
                Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
            }
            Actor->SetGravity(Operand);
            break;
        }
        case OPC_EnableState:
        {
            if (Actor->IsPlayer)
            {
                Actor->Player->EnableState(*reinterpret_cast<EnableFlags *>(Addr + 4));
            }
            break;
        }
        case OPC_DisableState:
        {
            if (Actor->IsPlayer)
            {
                Actor->Player->DisableState(*reinterpret_cast<EnableFlags *>(Addr + 4));
            }
            break;
        }
        case OPC_EnableAll:
        {
            if (Actor->IsPlayer)
            {
                Actor->Player->EnableAll();
            }
            break;
        }
        case OPC_DisableAll:
        {
            if (Actor->IsPlayer)
            {
                Actor->Player->DisableAll();
            }
            break;
        }
        case OPC_EnableFlip:
            Actor->EnableFlip(*reinterpret_cast<bool *>(Addr + 4));
            break;
        case OPC_ForceEnableFarNormal:
        {
            if (Actor->IsPlayer)
            {
                Actor->Player->ForceEnableFarNormal(*reinterpret_cast<bool *>(Addr + 4));
            }
            break;
        }
        case OPC_HaltMomentum: 
            Actor->HaltMomentum();
            break;
        case OPC_ClearInertia:
            Actor->ClearInertia();
            break;
        case OPC_SetActionFlags:
            if (Actor->IsPlayer)
            {
                Actor->Player->SetActionFlags(*reinterpret_cast<ActionFlags*>(Addr + 4));
            }
            break;
        case OPC_SetDefaultLandingAction:
            if (Actor->IsPlayer)
            {
                Actor->Player->SetDefaultLandingAction(*reinterpret_cast<bool*>(Addr + 4));
            }
            break;
        case OPC_CheckInput:
            {
                InputCondition Condition;
                for (int i = 0; i < 32; i++)
                {
                    if (Actor->Player->SavedInputCondition.Sequence[i].InputFlag != InputNone)
                    {
                        Condition.Sequence.push_back(Actor->Player->SavedInputCondition.Sequence[i]);
                    }
                }
                Condition.Lenience = Actor->Player->SavedInputCondition.Lenience;
                Condition.ImpreciseInputCount = Actor->Player->SavedInputCondition.ImpreciseInputCount;
                Condition.bInputAllowDisable = Actor->Player->SavedInputCondition.bInputAllowDisable;
                Condition.Method = Actor->Player->SavedInputCondition.Method;
                Actor->StoredRegister = Actor->Player->CheckInput(Condition);
                break;
            }
        case OPC_CheckInputRaw: 
            Actor->StoredRegister = Actor->Player->CheckInputRaw(*reinterpret_cast<InputFlags*>(Addr + 4));
            break;
        case OPC_JumpToState:
            if (Actor->IsPlayer)
            {
                Actor->Player->JumpToState(Addr + 4);
                return;
            }
            break;
        case OPC_SetParentState:
            {
                if (StateToModify)
                {
                    for (auto State : Actor->Player->CommonStates)
                    {
                        CString<64> TmpString;
                        TmpString.SetString(Addr + 4);
                        if (strcmp(State->Name.GetString(), TmpString.GetString()) == 0)
                        {
                            reinterpret_cast<ScriptState*>(StateToModify)->ParentState = reinterpret_cast<ScriptState*>(State);
                            break;
                        }
                    }
                }
                break;
            }
        case OPC_AddAirJump:
            if (Actor->IsPlayer)
            {
                Actor->Player->AddAirJump(*reinterpret_cast<int32_t*>(Addr + 4));
            }
            break;
        case OPC_AddAirDash: 
            if (Actor->IsPlayer)
            {
                Actor->Player->AddAirDash(*reinterpret_cast<int32_t*>(Addr + 4));
            }
            break;
        case OPC_AddGravity: 
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->AddGravity(Operand);
                break;
            }
        case OPC_SetInertia:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->SetInertia(Operand);
                break;
            }
        case OPC_AddInertia:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->AddInertia(Operand);
                break;
            }
        case OPC_EnableInertia:
            {
                if (*reinterpret_cast<int32_t *>(Addr + 4) != 0)
                    Actor->EnableInertia();
                else
                    Actor->DisableInertia();
                break;
            }
        case OPC_ModifyInternalValue:
            {
                int32_t Operand1 = *reinterpret_cast<int32_t *>(Addr + 12);
                bool IsOperand1InternalVal = false;
                InternalValue Val1;
                if (*reinterpret_cast<int32_t *>(Addr + 8) > 0)
                {
                    Val1 = static_cast<InternalValue>(Operand1);
                    Operand1 = Actor->GetInternalValue(static_cast<InternalValue>(Operand1));
                    IsOperand1InternalVal = true;
                }
                int32_t Operand2 = *reinterpret_cast<int32_t *>(Addr + 20);
                if (*reinterpret_cast<int32_t *>(Addr + 16) > 0)
                {
                    Operand2 = Actor->GetInternalValue(static_cast<InternalValue>(Operand2));
                }
                Operation Op = *reinterpret_cast<Operation *>(Addr + 4);
                int32_t Temp;
                CheckOperation(Op, Operand1, Operand2, &Temp);
                if (IsOperand1InternalVal)
                {
                    Actor->SetInternalValue(Val1, Temp);
                }
                break;
            }
        case OPC_StoreInternalValue:
            {
                int32_t Operand1 = *reinterpret_cast<int32_t *>(Addr + 8);
                bool IsOperand1InternalVal = false;
                InternalValue Val1;
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Val1 = static_cast<InternalValue>(Operand1);
                    Operand1 = Actor->GetInternalValue(static_cast<InternalValue>(Operand1));
                    IsOperand1InternalVal = true;
                }
                int32_t Operand2 = *reinterpret_cast<int32_t *>(Addr + 16);
                if (*reinterpret_cast<int32_t *>(Addr + 12) > 0)
                {
                    Operand2 = Actor->GetInternalValue(static_cast<InternalValue>(Operand2));
                }
                if (IsOperand1InternalVal)
                {
                    Actor->SetInternalValue(Val1, Operand2);
                }
                break;
            }
        case OPC_ModifyInternalValueAndSave:
            {
                int32_t Operand1 = *reinterpret_cast<int32_t *>(Addr + 12);
                if (*reinterpret_cast<int32_t *>(Addr + 8) > 0)
                {
                    Operand1 = Actor->GetInternalValue(static_cast<InternalValue>(Operand1));
                }
                int32_t Operand2 = *reinterpret_cast<int32_t *>(Addr + 20);
                if (*reinterpret_cast<int32_t *>(Addr + 16) > 0)
                {
                    Operand2 = Actor->GetInternalValue(static_cast<InternalValue>(Operand2));
                }
                Operation Op = *reinterpret_cast<Operation *>(Addr + 4);
                int32_t Temp;
                CheckOperation(Op, Operand1, Operand2, &Temp);
                int32_t Operand3 = *reinterpret_cast<int32_t *>(Addr + 28);
                bool IsOperand3InternalVal = false;
                InternalValue Val3;
                if (*reinterpret_cast<int32_t *>(Addr + 24) > 0)
                {
                    Val3 = static_cast<InternalValue>(Operand3);
                    Operand3 = Actor->GetInternalValue(static_cast<InternalValue>(Operand3));
                    IsOperand3InternalVal = true;
                }
                if (IsOperand3InternalVal)
                {
                    Actor->SetInternalValue(Val3, Temp);
                }
                break;
            }
        case OPC_SetAirDashTimer: 
            if (Actor->IsPlayer)
            {
                Actor->Player->SetAirDashTimer(*reinterpret_cast<bool *>(Addr + 4));
            }
            break;
        case OPC_SetAirDashNoAttackTimer: 
            if (Actor->IsPlayer)
            {
                Actor->Player->SetAirDashNoAttackTimer(*reinterpret_cast<bool *>(Addr + 4));
            }
            break;
        case OPC_MakeInput:
            {
                if (Actor->IsPlayer)
                {
                    Actor->Player->SavedInputCondition = SavedInputCondition();
                }
                break;
            }
        case OPC_MakeInputSequenceBitmask:
            {
                if (Actor->IsPlayer)
                {
                    for (int i = 0; i < 32; i++)
                    {
                        if (Actor->Player->SavedInputCondition.Sequence[i].InputFlag == InputNone)
                        {
                            Actor->Player->SavedInputCondition.Sequence[i].InputFlag = *reinterpret_cast<InputFlags*>(Addr + 4);
                            break;
                        }
                    }
                }
                break;
            }
        case OPC_MakeInputLenience:
            {
                if (Actor->IsPlayer)
                {
                    Actor->Player->SavedInputCondition.Lenience = *reinterpret_cast<int*>(Addr + 4);
                }
                break;
            }
        case OPC_MakeInputImpreciseCount:
            {
                if (Actor->IsPlayer)
                {
                    Actor->Player->SavedInputCondition.ImpreciseInputCount = *reinterpret_cast<int*>(Addr + 4);
                }
                break;
            }
        case OPC_MakeInputAllowDisable:
            {
                if (Actor->IsPlayer)
                {
                    Actor->Player->SavedInputCondition.bInputAllowDisable = *reinterpret_cast<bool*>(Addr + 4);
                }
                break;
            }
        case OPC_MakeInputMethod:
            {
                if (Actor->IsPlayer)
                {
                    Actor->Player->SavedInputCondition.Method = *reinterpret_cast<InputMethod*>(Addr + 4);
                }
                break;
            }
        case OPC_CreateParticle:
            {
                char* ParticleName = Addr + 4;
                if (strncmp(ParticleName, "cmn", 3) == 0)
                {
                    Actor->CreateCommonParticle(ParticleName, *reinterpret_cast<PosType*>(Addr + 68), 
                        Vector(*reinterpret_cast<int32_t*>(Addr + 72), *reinterpret_cast<int32_t*>(Addr + 76)),
                        *reinterpret_cast<int32_t*>(Addr + 80));
                }
                else
                {
                    Actor->CreateCharaParticle(ParticleName, *reinterpret_cast<PosType*>(Addr + 68), 
                        Vector(*reinterpret_cast<int32_t*>(Addr + 72), *reinterpret_cast<int32_t*>(Addr + 76)),
                        *reinterpret_cast<int32_t*>(Addr + 80));
                }
            }
        case OPC_AddBattleActor:
            {
                char* ParticleName = Addr + 4;
                if (strncmp(ParticleName, "cmn", 3) == 0)
                {
                    Actor->Player->AddCommonBattleActor(ParticleName, *reinterpret_cast<int32_t*>(Addr + 68),
                        *reinterpret_cast<int32_t*>(Addr + 72), *reinterpret_cast<PosType*>(Addr + 76));
                }
                else
                {
                    Actor->Player->AddBattleActor(ParticleName, *reinterpret_cast<int32_t*>(Addr + 68),
                        *reinterpret_cast<int32_t*>(Addr + 72), *reinterpret_cast<PosType*>(Addr + 76));
                }
            }
        case OPC_EnableHit:
            Actor->EnableHit(*reinterpret_cast<bool*>(Addr + 4));
            break;
        case OPC_SetAttacking:
            Actor->SetAttacking(*reinterpret_cast<bool*>(Addr + 4));
            break;
        case OPC_EnableSpecialCancel:
            if (Actor->IsPlayer)
                Actor->Player->EnableSpecialCancel(*reinterpret_cast<bool*>(Addr + 4));
            break;
        case OPC_EnableSuperCancel:
            if (Actor->IsPlayer)
                Actor->Player->EnableSuperCancel(*reinterpret_cast<bool*>(Addr + 4));
            break;
        case OPC_EnableChainCancel:
            if (Actor->IsPlayer)
                Actor->Player->EnableChainCancel(*reinterpret_cast<bool*>(Addr + 4));
            break;
        case OPC_EnableWhiffCancel:
            if (Actor->IsPlayer)
                Actor->Player->EnableWhiffCancel(*reinterpret_cast<bool*>(Addr + 4));
            break;
        case OPC_EnableCancelIntoSelf:
            if (Actor->IsPlayer)
                Actor->Player->EnableCancelIntoSelf(*reinterpret_cast<bool*>(Addr + 4));
            break;
        case OPC_SetAttackLevel:
            Actor->NormalHitEffect.AttackLevel = *reinterpret_cast<int32_t*>(Addr + 4);
            Actor->CounterHitEffect.AttackLevel = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetCounterAttackLevel:
            Actor->CounterHitEffect.AttackLevel = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetHitstun:
            Actor->NormalHitEffect.Hitstun = *reinterpret_cast<int32_t*>(Addr + 4);
            Actor->CounterHitEffect.Hitstun = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetCounterHitstun:
            Actor->CounterHitEffect.Hitstun = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetUntech:
            Actor->NormalHitEffect.Untech = *reinterpret_cast<int32_t*>(Addr + 4);
            Actor->CounterHitEffect.Untech = *reinterpret_cast<int32_t*>(Addr + 4) * 2;
            break;
        case OPC_SetCounterUntech:
            Actor->CounterHitEffect.Untech = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetDamage:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->NormalHitEffect.HitDamage = Operand;
                Actor->CounterHitEffect.HitDamage = Operand * 11 / 10;
            }
            break;
        case OPC_SetCounterDamage:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->CounterHitEffect.HitDamage = Operand;
            }
            break;
        case OPC_SetMinimumDamagePercent:
            Actor->NormalHitEffect.MinimumDamagePercent = *reinterpret_cast<int32_t*>(Addr + 4);
            Actor->CounterHitEffect.MinimumDamagePercent = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetCounterMinimumDamagePercent:
            Actor->CounterHitEffect.MinimumDamagePercent = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetInitialProration:
            Actor->NormalHitEffect.InitialProration = *reinterpret_cast<int32_t*>(Addr + 4);
            Actor->CounterHitEffect.InitialProration = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetCounterInitialProration:
            Actor->CounterHitEffect.InitialProration = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetForcedProration:
            Actor->NormalHitEffect.ForcedProration = *reinterpret_cast<int32_t*>(Addr + 4);
            Actor->CounterHitEffect.ForcedProration = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetCounterForcedProration:
            Actor->CounterHitEffect.ForcedProration = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetHitPushbackX:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->NormalHitEffect.HitPushbackX = Operand;
                Actor->CounterHitEffect.HitPushbackX = Operand;
            }
            break;
        case OPC_SetCounterHitPushbackX:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->CounterHitEffect.HitPushbackX = Operand;
            }
            break;
        case OPC_SetAirHitPushbackX:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->NormalHitEffect.AirHitPushbackX = Operand;
                Actor->CounterHitEffect.AirHitPushbackX = Operand;
            }
            break;
        case OPC_SetCounterAirHitPushbackX:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->CounterHitEffect.AirHitPushbackX = Operand;
            }
            break;
        case OPC_SetAirHitPushbackY:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->NormalHitEffect.AirHitPushbackY = Operand;
                Actor->CounterHitEffect.AirHitPushbackY = Operand;
            }
            break;
        case OPC_SetCounterAirHitPushbackY:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->CounterHitEffect.AirHitPushbackY = Operand;
            }
            break;
        case OPC_SetHitGravity:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->NormalHitEffect.HitGravity = Operand;
                Actor->CounterHitEffect.HitGravity = Operand;
            }
            break;
        case OPC_SetCounterHitGravity:
            {
                int32_t Operand = *reinterpret_cast<int32_t *>(Addr + 8);
                if (*reinterpret_cast<int32_t *>(Addr + 4) > 0)
                {
                    Operand = Actor->GetInternalValue(static_cast<InternalValue>(Operand));
                }
                Actor->CounterHitEffect.HitGravity = Operand;
            }
            break;
        case OPC_SetGroundHitAction:
            Actor->NormalHitEffect.GroundHitAction = *reinterpret_cast<HitAction*>(Addr + 4);
            Actor->CounterHitEffect.GroundHitAction = *reinterpret_cast<HitAction*>(Addr + 4);
            break;
        case OPC_SetCounterGroundHitAction:
            Actor->CounterHitEffect.GroundHitAction = *reinterpret_cast<HitAction*>(Addr + 4);
            break;
        case OPC_SetAirHitAction:
            Actor->NormalHitEffect.AirHitAction = *reinterpret_cast<HitAction*>(Addr + 4);
            Actor->CounterHitEffect.AirHitAction = *reinterpret_cast<HitAction*>(Addr + 4);
            break;
        case OPC_SetCounterAirHitAction:
            Actor->CounterHitEffect.AirHitAction = *reinterpret_cast<HitAction*>(Addr + 4);
            break;
        case OPC_SetKnockdownTime:
            Actor->NormalHitEffect.KnockdownTime = *reinterpret_cast<int32_t*>(Addr + 4);
            Actor->CounterHitEffect.KnockdownTime = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetCounterKnockdownTime:
            Actor->CounterHitEffect.KnockdownTime = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetHitstop:
            Actor->NormalHitEffect.Hitstop = *reinterpret_cast<int32_t*>(Addr + 4);
            Actor->CounterHitEffect.Hitstop = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetCounterHitstop:
            Actor->CounterHitEffect.Hitstop = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetBlockstun:
            Actor->NormalHitEffect.Blockstun = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetBlockstopModifier:
            Actor->NormalHitEffect.BlockstopModifier = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetChipDamagePercent:
            Actor->NormalHitEffect.ChipDamagePercent = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_SetHitAngle:
            Actor->NormalHitEffect.HitAngle = *reinterpret_cast<int32_t*>(Addr + 4);
            Actor->CounterHitEffect.HitAngle = *reinterpret_cast<int32_t*>(Addr + 4);
            break;
        case OPC_AddChainCancelOption:
            {
                if (Actor->IsPlayer)
                {
                    CString<64> StateName;
                    StateName.SetString(Addr + 4);
                    Actor->Player->AddChainCancelOption(StateName);
                }
            }
        case OPC_AddWhiffCancelOption:
            {
                if (Actor->IsPlayer)
                {
                    CString<64> StateName;
                    StateName.SetString(Addr + 4);
                    Actor->Player->AddWhiffCancelOption(StateName);
                }
            }
        default:
            break;
        }
        Addr += OpCodeSizes[code];
    }
}

bool ScriptAnalyzer::FindNextCel(char **Addr, int AnimTime)
{
    while (true)
    {
        OpCodes code = *reinterpret_cast<OpCodes *>(*Addr);
        switch (code)
        {
        case OPC_SetCel:
            return true;
        case OPC_EndLabel:
            {
                if (AnimTime > *reinterpret_cast<int*>(*Addr + 4))
                {
                    break;
                }
                return false;
            }
        case OPC_ExitState:
        case OPC_EndBlock:
        case OPC_EndState:
        case OPC_EndSubroutine:
            return false;
        default:
            break;
        }
        *Addr += OpCodeSizes[code];
    }
}

void ScriptAnalyzer::FindMatchingEnd(char **Addr, OpCodes EndCode)
{
    int32_t Depth = -1;
    while (true)
    {
        OpCodes code = *reinterpret_cast<OpCodes*>(*Addr);
        if (EndCode == OPC_EndIf)
        {
            if (code == OPC_If || code == OPC_IfNot || code == OPC_IfOperation)
                Depth++;
        }
        else if (EndCode == OPC_EndElse)
        {
            if (code == OPC_Else)
                Depth++;
        }
        
        if (code == EndCode)
        {
            if (Depth > 0)
                Depth--;
            else
                return;
        }
        if (code == OPC_EndBlock || code == OPC_ExitState || code == OPC_EndSubroutine || code == OPC_EndLabel)
            return;
        *Addr += OpCodeSizes[code];
    }
}

void ScriptAnalyzer::FindElse(char **Addr)
{
    int32_t Depth = -1;
    while (true)
    {
        OpCodes code = *reinterpret_cast<OpCodes*>(*Addr);
        if (code == OPC_If || code == OPC_IfNot || code == OPC_IfOperation)
            Depth++;
        if (code == OPC_Else)
        {
            if (Depth > 0)
                Depth--;
            else
                return;                
        }
        if (code == OPC_EndBlock || code == OPC_ExitState || code == OPC_EndSubroutine || code == OPC_EndLabel)
        {
            *Addr = 0;
            return;
        }
        *Addr += OpCodeSizes[code];
    }
}

void ScriptAnalyzer::GetAllLabels(char *Addr, std::vector<StateAddress> *Labels)
{
    while (true)
    {
        OpCodes code = *reinterpret_cast<OpCodes *>(Addr);
        if (code == OPC_BeginLabel)
        {
            CString<64> LabelName;
            LabelName.SetString(Addr + 4);
            StateAddress Label;
            Label.Name = LabelName;
            Label.OffsetAddress = Addr - ScriptAddress;
            Labels->push_back(Label);
        }
        if (code == OPC_EndBlock || code == OPC_EndSubroutine || code == OPC_EndState)
            return;
        Addr += OpCodeSizes[code];
    }
}

void ScriptAnalyzer::CheckOperation(Operation Op, int32_t Operand1, int32_t Operand2, int32_t *Return)
{
    switch (Op)
    {
    case OP_Add:
    {
        *Return = Operand1 + Operand2;
        break;
    }
    case OP_Sub:
    {
        *Return = Operand1 - Operand2;
        break;
    }
    case OP_Mul:
    {
        *Return = Operand1 * Operand2;
        break;
    }
    case OP_Div:
    {
        *Return = Operand1 / Operand2;
        break;
    }
    case OP_Mod:
    {
        *Return = Operand1 % Operand2;
        break;
    }
    case OP_And:
    {
        *Return = Operand1 && Operand2;
        break;
    }
    case OP_Or:
    {
        *Return = Operand1 || Operand2;
        break;
    }
    case OP_BitwiseAnd:
    {
        *Return = Operand1 & Operand2;
        break;
    }
    case OP_BitwiseOr:
    {
        *Return = Operand1 | Operand2;
        break;
    }
    case OP_IsEqual:
    {
        *Return = Operand1 == Operand2;
        break;
    }
    case OP_IsGreater:
    {
        *Return = Operand1 > Operand2;
        break;
    }
    case OP_IsLesser:
    {
        *Return = Operand1 < Operand2;
        break;
    }
    case OP_IsGreaterOrEqual:
    {
        *Return = Operand1 >= Operand2;
        break;
    }
    case OP_IsLesserOrEqual:
    {
        *Return = Operand1 <= Operand2;
        break;
    }
    case OP_BitDelete:
    {
        *Return = Operand1 & ~Operand2;
        break;
    }
    case OP_IsNotEqual:
    {
        *Return = Operand1 != Operand2;
        break;
    }
    }
}
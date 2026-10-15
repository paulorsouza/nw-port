#include "NWBehaviorTreeComponent.h"
#include "NWPlayerGrid.h"

#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NWCharacter.h"
#include "NWCombatComponent.h"
#include "NWCombatTypes.h"
#include "NWCreature.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	// Tree from the JSON to the node list (index = node id in the execution state).
	int32 Flatten(const TSharedPtr<FJsonObject>& J, TArray<FNWBTNode>& Out)
	{
		const int32 Index = Out.AddDefaulted();
		FNWBTNode N;
		N.Type = FName(J->GetStringField(TEXT("t")));
		J->TryGetStringField(TEXT("n"), N.Name);
		double U = 0.0;
		if (J->TryGetNumberField(TEXT("u"), U))
		{
			N.Utility = float(U);
		}
		J->TryGetBoolField(TEXT("ni"), N.bNonInterruptable);
		const TSharedPtr<FJsonObject>* P = nullptr;
		N.P = J->TryGetObjectField(TEXT("p"), P) ? *P : MakeShared<FJsonObject>();
		const TArray<TSharedPtr<FJsonValue>>* Kids = nullptr;
		if (J->TryGetArrayField(TEXT("c"), Kids))
		{
			for (const TSharedPtr<FJsonValue>& K : *Kids)
			{
				N.Children.Add(Flatten(K->AsObject(), Out));
			}
		}
		Out[Index] = MoveTemp(N);
		return Index;
	}

	bool Compare(double L, int32 Op, double R)
	{
		switch (Op)
		{
		case 1: return FMath::IsNearlyEqual(L, R, 1e-4);
		case 2: return !FMath::IsNearlyEqual(L, R, 1e-4);
		case 3: return L < R;
		case 4: return L > R;
		case 5: return L <= R;
		case 6: return L >= R;
		default: return false;
		}
	}

	double JsonNumber(const TSharedPtr<FJsonValue>& V)
	{
		if (!V.IsValid())
		{
			return 0.0;
		}
		switch (V->Type)
		{
		case EJson::Number: return V->AsNumber();
		case EJson::Boolean: return V->AsBool() ? 1.0 : 0.0;
		case EJson::String:
		{
			const FString S = V->AsString();
			if (S.Equals(TEXT("True"), ESearchCase::IgnoreCase)) return 1.0;
			if (S.Equals(TEXT("False"), ESearchCase::IgnoreCase)) return 0.0;
			return FCString::Atod(*S);
		}
		default: return 0.0;
		}
	}
}

const FNWBTAsset* FNWBTAsset::Get(FName Set)
{
	static TMap<FName, TUniquePtr<FNWBTAsset>> Cache;
	if (const TUniquePtr<FNWBTAsset>* Found = Cache.Find(Set))
	{
		return Found->Get();
	}
	TUniquePtr<FNWBTAsset>& Slot = Cache.Add(Set);
	FString Raw;
	TSharedPtr<FJsonObject> Root;
	const FString File = FPaths::ProjectDir() / TEXT("Data/AI") / (Set.ToString() + TEXT(".json"));
	if (!FFileHelper::LoadFileToString(Raw, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
	{
		return nullptr;
	}
	Slot = MakeUnique<FNWBTAsset>();
	Slot->Profile = Root->GetObjectField(TEXT("profile"));
	Slot->Tasks = Root->GetObjectField(TEXT("tasks"));
	Flatten(Root->GetObjectField(TEXT("tree")), Slot->Nodes);
	return Slot.Get();
}

UNWBehaviorTreeComponent::UNWBehaviorTreeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = false;
}

bool UNWBehaviorTreeComponent::Start(FName Set, const FVector& Home, const TArray<FVector>& Waypoints)
{
	Asset = FNWBTAsset::Get(Set);
	if (!Asset || Asset->Nodes.Num() == 0)
	{
		return false;
	}
	SetName = Set;
	HomePos = Home;
	Path = Waypoints;
	// Initial blackboard: m_configparams from the slice + what the game server would fill in on spawn.
	for (const auto& Pair : Asset->Profile->GetObjectField(TEXT("blackboard"))->Values)
	{
		FNWBBValue& B = BB.FindOrAdd(FName(*Pair.Key));
		if (Pair.Value->Type == EJson::String)
		{
			B.Str = Pair.Value->AsString();
		}
		B.Num = JsonNumber(Pair.Value);
	}
	SetVec(TEXT("Spawn_Pos"), Home);
	// Character tags (slice TagComponent): creature variant (Grey...), which the tree tests.
	const TArray<TSharedPtr<FJsonValue>>* InitTags = nullptr;
	if (Asset->Profile->TryGetArrayField(TEXT("tags"), InitTags))
	{
		for (const TSharedPtr<FJsonValue>& V : *InitTags) Tags.Add(FName(V->AsString()));
	}
	if (Path.Num())
	{
		SetVec(TEXT("Waypoint_Pos"), Path[0]);
	}
	SetNum(TEXT("Difficulty_Tier"), 2.0);   // no per-mob source for the value: 2 = medium (enables pounce and combos)
	SetNum(TEXT("DoesLeash"), 1.0);
	SetNum(TEXT("Leash_Opt_Out"), 0.0);
	SetNum(TEXT("Enable_Wander"), Path.Num() ? 0.0 : 1.0);
	SetNum(TEXT("Target_Vitals_State"), 0.0);
	if (ANWCharacter* Me = Cast<ANWCharacter>(GetOwner()))
	{
		Me->Combat->OnHitTaken.AddDynamic(this, &UNWBehaviorTreeComponent::OnHit);
	}
	Activate(true);
	return true;
}

// ---------------------------------------------------------------- parameters and conditions

double UNWBehaviorTreeComponent::Operand(const TSharedPtr<FJsonObject>& O, bool bDistance) const
{
	FString Key;
	if (O->TryGetStringField(TEXT("k"), Key))
	{
		const FNWBBValue* B = BB.Find(FName(*Key));
		if (!B)
		{
			return 0.0;
		}
		if (B->bVec)
		{
			// Position key in a condition: distance (m) to it.
			return FVector::Dist(GetOwner()->GetActorLocation(), B->Vec) / 100.0;
		}
		return B->Num;
	}
	return JsonNumber(O->TryGetField(TEXT("v")));
}

bool UNWBehaviorTreeComponent::HasGroup(const FNWBTNode& N, const TCHAR* Group) const
{
	return N.P->HasTypedField<EJson::Object>(Group);
}

bool UNWBehaviorTreeComponent::CheckGroup(const FNWBTNode& N, const TCHAR* Group, bool bDefault) const
{
	const TSharedPtr<FJsonObject>* G = nullptr;
	if (!N.P->TryGetObjectField(Group, G))
	{
		return bDefault;
	}
	bool bOr = false;
	(*G)->TryGetBoolField(TEXT("or"), bOr);
	const TArray<TSharedPtr<FJsonValue>>* Conds = nullptr;
	if (!(*G)->TryGetArrayField(TEXT("c"), Conds) || Conds->Num() == 0)
	{
		return true;   // empty group passes
	}
	const bool bDist = N.Type == TEXT("DistanceCondition");
	for (const TSharedPtr<FJsonValue>& C : *Conds)
	{
		const TArray<TSharedPtr<FJsonValue>>& T = C->AsArray();
		double L;
		FString LeftKey;
		const TSharedPtr<FJsonValue> RV = T[2]->AsObject()->TryGetField(TEXT("v"));
		if (RV.IsValid() && RV->Type == EJson::Boolean && T[0]->AsObject()->TryGetStringField(TEXT("k"), LeftKey))
		{
			// "Last_Damage_Pos == True": the key exists (position) or is true (number).
			const FNWBBValue* B = BB.Find(FName(*LeftKey));
			L = B && (B->bVec || B->Num != 0.0) ? 1.0 : 0.0;
		}
		else
		{
			L = Operand(T[0]->AsObject(), bDist);
		}
		const bool bOk = Compare(L, int32(T[1]->AsNumber()), Operand(T[2]->AsObject(), false));
		if (bOr && bOk) return true;
		if (!bOr && !bOk) return false;
	}
	return !bOr;
}

double UNWBehaviorTreeComponent::Num(const FNWBTNode& N, const TCHAR* Key, double Default) const
{
	const TSharedPtr<FJsonValue> V = N.P->TryGetField(Key);
	if (!V.IsValid())
	{
		return Default;
	}
	if (V->Type == EJson::String && V->AsString().StartsWith(TEXT("$")))
	{
		const FNWBBValue* B = BB.Find(FName(*V->AsString().Mid(1)));
		return B ? B->Num : Default;
	}
	return JsonNumber(V);
}

FString UNWBehaviorTreeComponent::Str(const FNWBTNode& N, const TCHAR* Key) const
{
	FString S;
	if (!N.P->TryGetStringField(Key, S))
	{
		return S;
	}
	if (S.StartsWith(TEXT("$")))
	{
		const FNWBBValue* B = BB.Find(FName(*S.Mid(1)));
		return B ? B->Str : FString();
	}
	return S;
}

bool UNWBehaviorTreeComponent::Bool(const FNWBTNode& N, const TCHAR* Key, bool Default) const
{
	return Num(N, Key, Default ? 1.0 : 0.0) != 0.0;
}

FVector UNWBehaviorTreeComponent::PositionOf(int32 Type) const
{
	auto Vec = [this](const TCHAR* K, const FVector& Def) { const FNWBBValue* B = BB.Find(K); return B && B->bVec ? B->Vec : Def; };
	const FVector Self = GetOwner()->GetActorLocation();
	switch (Type)
	{
	case 1: case 2: case 12: return Target.IsValid() ? Target->GetActorLocation() : Vec(TEXT("Target_Pos"), Self);
	case 3: return Vec(TEXT("Selected_Position"), Self);
	case 4: return Vec(TEXT("Last_Damage_Pos"), Self);
	case 5: return HomePos;
	case 6: return Vec(TEXT("Waypoint_Pos"), HomePos);
	default: return Self;
	}
}

double UNWBehaviorTreeComponent::NumIn(const TSharedPtr<FJsonObject>& Obj, const FString& Key, double Default) const
{
	const TSharedPtr<FJsonValue> V = Obj.IsValid() ? Obj->TryGetField(Key) : nullptr;
	if (!V.IsValid())
	{
		return Default;
	}
	if (V->Type == EJson::String && V->AsString().StartsWith(TEXT("$")))
	{
		const FNWBBValue* B = BB.Find(FName(*V->AsString().Mid(1)));
		return B ? B->Num : Default;
	}
	return JsonNumber(V);
}

bool UNWBehaviorTreeComponent::SelectPosition(const TSharedPtr<FJsonObject>& Params, const FString& Px, FVector& Out) const
{
	// Game's SelectPositionParams: point at [rangemin, rangemax] m from the origin, in an arc [startangle, stopangle]
	// around the direction origin→anchor (0), anchor→origin (1) or the origin's facing (2).
	auto F = [&](const TCHAR* Key, double Def) { return NumIn(Params, Px + Key, Def); };
	const int32 OriginType = int32(F(TEXT("origin.positiontype"), 0));
	const int32 AnchorType = int32(F(TEXT("anchor.positiontype"), 0));
	const FVector Origin = PositionOf(OriginType);
	const FVector Anchor = PositionOf(AnchorType);
	const int32 DirType = int32(F(TEXT("direction"), 0));
	FVector Base = DirType == 1 ? (Origin - Anchor) : (Anchor - Origin);
	if (DirType == 2 || Base.IsNearlyZero())
	{
		Base = OriginType == 0 ? GetOwner()->GetActorForwardVector() : (Target.IsValid() ? Target->GetActorForwardVector() : GetOwner()->GetActorForwardVector());
	}
	Base = Base.GetSafeNormal2D();
	double A0 = F(TEXT("startangleparam"), F(TEXT("startangle"), 0.0));
	double A1 = F(TEXT("stopangleparam"), F(TEXT("stopangle"), 0.0));
	if (A1 < A0)
	{
		A1 += 360.0;
	}
	const double R0 = F(TEXT("rangeminparam"), F(TEXT("rangemin"), 0.0));
	const double R1 = F(TEXT("rangemaxparam"), F(TEXT("rangemax"), R0));
	double T = FMath::FRand();
	if (F(TEXT("favormiddleangle"), 0.0) != 0.0)
	{
		T = (T + FMath::FRand()) * 0.5;
	}
	const double Angle = A0 + (A1 - A0) * T;
	const double Range = FMath::Lerp(R0, FMath::Max(R0, R1), FMath::FRand()) * 100.0;
	Out = Origin + Base.RotateAngleAxis(float(Angle), FVector::UpVector) * float(Range);
	Out.Z = Origin.Z;
	return true;
}

// ---------------------------------------------------------------- cooldowns

bool UNWBehaviorTreeComponent::CooldownActive(FName Name) const
{
	const double* End = Cooldowns.Find(Name);
	return End && GetWorld()->GetTimeSeconds() < *End;
}

void UNWBehaviorTreeComponent::StartCooldown(const FNWBTNode& N)
{
	const FName Name(*Str(N, TEXT("cooldownname")));
	// No time (SetCooldownAction "ResetAttackCooldown"): clears the cooldown.
	const double T = FMath::FRandRange(Num(N, TEXT("timemin"), 0.0), FMath::Max(Num(N, TEXT("timemin"), 0.0), Num(N, TEXT("timemax"), 0.0)));
	const double End = GetWorld()->GetTimeSeconds() + T;
	double& Cur = Cooldowns.FindOrAdd(Name);
	const bool bActive = Cur > GetWorld()->GetTimeSeconds();
	// Game override: 0 always, 1 never (keeps the active one), 2 the longest, 3 the shortest.
	switch (int32(Num(N, TEXT("override"), 0)))
	{
	case 1: if (!bActive) Cur = End; break;
	case 2: Cur = bActive ? FMath::Max(Cur, End) : End; break;
	case 3: Cur = bActive ? FMath::Min(Cur, End) : End; break;
	default: Cur = End; break;
	}
}

// ---------------------------------------------------------------- plan

TSharedPtr<UNWBehaviorTreeComponent::FPlan> UNWBehaviorTreeComponent::Evaluate(int32 Index)
{
	const FNWBTNode& N = Asset->Nodes[Index];
	TSharedPtr<FPlan> P = MakeShared<FPlan>();
	P->Node = Index;
	P->Utility = N.Utility;
	const FName T = N.Type;
	auto WithChild = [&](int32 Child) -> TSharedPtr<FPlan>
	{
		TSharedPtr<FPlan> K = Evaluate(Child);
		if (!K)
		{
			return nullptr;
		}
		P->Kids.Add(K);
		P->Utility = N.Utility > 1e-5f ? N.Utility : K->Utility;
		return P;
	};
	auto FirstChild = [&]() -> int32 { return N.Children.Num() ? N.Children[0] : INDEX_NONE; };

	if (T == TEXT("Selector") || T == TEXT("FallbackSelector"))
	{
		for (const int32 C : N.Children)
		{
			if (TSharedPtr<FPlan> K = Evaluate(C))
			{
				P->Kids.Add(K);
				P->Utility = FMath::Max(N.Utility, K->Utility);
				return P;
			}
		}
		return nullptr;
	}
	if (T == TEXT("Sequencer") || T == TEXT("Parallel") || T == TEXT("PermissiveSequencer"))
	{
		const bool bPermissive = T == TEXT("PermissiveSequencer");
		float Best = N.Utility;
		for (const int32 C : N.Children)
		{
			TSharedPtr<FPlan> K = Evaluate(C);
			if (!K)
			{
				if (bPermissive) continue;
				return nullptr;
			}
			Best = FMath::Max(Best, K->Utility);
			P->Kids.Add(K);
		}
		if (P->Kids.Num() == 0)
		{
			return nullptr;
		}
		P->Utility = Best;
		return P;
	}
	if (T == TEXT("UtilitySelector"))
	{
		TArray<TSharedPtr<FPlan>> Cands;
		for (const int32 C : N.Children)
		{
			if (TSharedPtr<FPlan> K = Evaluate(C))
			{
				Cands.Add(K);
			}
		}
		if (Cands.Num() == 0)
		{
			return nullptr;
		}
		// Keeps the previous choice while it still evaluates (AlwaysPreserveExecutingPlan).
		TSharedPtr<FPlan> Pick;
		if (Bool(N, TEXT("alwayspreserveexecutingplan"), false) || Bool(N, TEXT("preserveexecutingplanifnochange"), false))
		{
			if (const int32* Prev = PreservedChoice.Find(Index))
			{
				for (const TSharedPtr<FPlan>& K : Cands)
				{
					if (K->Node == *Prev) Pick = K;
				}
			}
		}
		if (!Pick)
		{
			if (int32(Num(N, TEXT("selectionmethod"), 0)) == 1)
			{
				// Draw weighted by utility among the candidates (the wolf uses N = all).
				float Total = 0.f;
				for (const TSharedPtr<FPlan>& K : Cands) Total += FMath::Max(K->Utility, 1e-4f);
				float Roll = FMath::FRandRange(0.f, Total);
				for (const TSharedPtr<FPlan>& K : Cands)
				{
					Roll -= FMath::Max(K->Utility, 1e-4f);
					if (Roll <= 0.f) { Pick = K; break; }
				}
				if (!Pick) Pick = Cands.Last();
			}
			else
			{
				Pick = Cands[0];
				for (const TSharedPtr<FPlan>& K : Cands) if (K->Utility > Pick->Utility) Pick = K;
			}
		}
		PreservedChoice.Add(Index, Pick->Node);
		P->Kids.Add(Pick);
		P->Utility = FMath::Max(N.Utility, Pick->Utility);
		return P;
	}
	// Conditions (decorators with an entry group).
	if (T == TEXT("BlackboardCondition") || T == TEXT("DistanceCondition"))
	{
		return CheckGroup(N, TEXT("entry"), true) && FirstChild() != INDEX_NONE ? WithChild(FirstChild()) : nullptr;
	}
	if (T == TEXT("VariableExistsCondition"))
	{
		const bool bHas = BB.Contains(FName(*Str(N, TEXT("variablename"))));
		return (bHas != Bool(N, TEXT("invertcheck"), false)) && FirstChild() != INDEX_NONE ? WithChild(FirstChild()) : nullptr;
	}
	if (T == TEXT("IsCooldownActiveCondition"))
	{
		const bool bActive = CooldownActive(FName(*Str(N, TEXT("cooldownname"))));
		return (bActive != Bool(N, TEXT("invertcheck"), false)) && FirstChild() != INDEX_NONE ? WithChild(FirstChild()) : nullptr;
	}
	if (T == TEXT("MannequinTagCondition"))
	{
		// Tags: stunned comes from combat; the others from tasks and SetMannequinTag (Alerted, ChainAttack...).
		const ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
		const FName Tag(*Str(N, TEXT("tag")));
		const bool bTag = Tag == TEXT("Stunned") ? Me && Me->Combat->State == ENWCombatState::Stunned : Tags.Contains(Tag);
		return (bTag != Bool(N, TEXT("invertcheck"), false)) && FirstChild() != INDEX_NONE ? WithChild(FirstChild()) : nullptr;
	}
	if (T == TEXT("CooldownDecorator"))
	{
		return !CooldownActive(FName(*Str(N, TEXT("cooldownname")))) && FirstChild() != INDEX_NONE ? WithChild(FirstChild()) : nullptr;
	}
	if (T == TEXT("FrequencyLimiter"))
	{
		const double* Ready = FrequencyReady.Find(Index);
		return (!Ready || GetWorld()->GetTimeSeconds() >= *Ready) && FirstChild() != INDEX_NONE ? WithChild(FirstChild()) : nullptr;
	}
	if (T == TEXT("AlwaysSucceed"))
	{
		if (FirstChild() != INDEX_NONE)
		{
			if (TSharedPtr<FPlan> K = Evaluate(FirstChild()))
			{
				P->Kids.Add(K);
			}
		}
		return P;
	}
	if (N.Children.Num() > 0)
	{
		// Decorators without a condition: Timer, StatusEffectDecorator, SetMannequinTag, TreeReference, Decorator.
		return WithChild(FirstChild());
	}
	// Leaves.
	if (T == TEXT("CAGEAction"))
	{
		const FString Task = Str(N, TEXT("actionname"));
		const TSharedPtr<FJsonObject>* Map = nullptr;
		if (Asset->Tasks->TryGetObjectField(Task, Map) && (*Map)->HasField(TEXT("action")))
		{
			// Attack needs a target (action with an action target in the game).
			return Target.IsValid() ? P : nullptr;
		}
		return P;
	}
	if (T == TEXT("NavigationAction"))
	{
		const int32 Origin = int32(Num(N, TEXT("selectpositionparams.origin.positiontype"), 1));
		return (Origin != 1 && Origin != 2 && Origin != 12) || Target.IsValid() || BB.Contains(TEXT("Target_Pos")) ? P : nullptr;
	}
	if (T == TEXT("SelectWaypointAction"))
	{
		return Path.Num() ? P : nullptr;
	}
	return P;   // SelectPositionAction, SetBlackboardFactAction, SetCooldownAction, LockTargetSelection...
}

void UNWBehaviorTreeComponent::Signature(const FPlan& Plan, TArray<int32>& Out) const
{
	Out.Add(Plan.Node);
	for (const TSharedPtr<FPlan>& K : Plan.Kids)
	{
		Signature(*K, Out);
	}
}

bool UNWBehaviorTreeComponent::RunningPathNonInterruptable(const FPlan& Plan) const
{
	// Is any started (running) node on the current path non-interruptible?
	if (!Plan.bStarted)
	{
		return false;
	}
	if (Asset->Nodes[Plan.Node].bNonInterruptable)
	{
		return true;
	}
	for (const TSharedPtr<FPlan>& K : Plan.Kids)
	{
		if (RunningPathNonInterruptable(*K))
		{
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------- execute

void UNWBehaviorTreeComponent::Abort(FPlan& Plan)
{
	if (!Plan.bStarted)
	{
		return;
	}
	for (const TSharedPtr<FPlan>& K : Plan.Kids)
	{
		Abort(*K);
	}
	const FNWBTNode& N = Asset->Nodes[Plan.Node];
	// Cooldown also starts when the child is aborted.
	if (N.Type == TEXT("CooldownDecorator"))
	{
		StartCooldown(N);
	}
	if (N.Type == TEXT("StatusEffectDecorator"))
	{
		bResetting = false;
	}
	Plan.bStarted = false;
}

UNWBehaviorTreeComponent::EStatus UNWBehaviorTreeComponent::Execute(FPlan& Plan, float Dt)
{
	const FNWBTNode& N = Asset->Nodes[Plan.Node];
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bFirst = !Plan.bStarted;
	if (bFirst)
	{
		Plan.bStarted = true;
		Plan.StartTime = Now;
	}
	const FName T = N.Type;
	auto Child = [&]() -> EStatus { return Plan.Kids.Num() ? Execute(*Plan.Kids[0], Dt) : EStatus::Success; };

	if (T == TEXT("Sequencer") || T == TEXT("Parallel") || T == TEXT("PermissiveSequencer"))
	{
		while (Plan.Cursor < Plan.Kids.Num())
		{
			const EStatus S = Execute(*Plan.Kids[Plan.Cursor], Dt);
			if (S == EStatus::Running)
			{
				return S;
			}
			if (S == EStatus::Failure && T != TEXT("PermissiveSequencer"))
			{
				return S;
			}
			++Plan.Cursor;
		}
		return EStatus::Success;
	}
	if (T == TEXT("BlackboardCondition") || T == TEXT("DistanceCondition"))
	{
		// Continuation failed: ends with success; interruption failed: fails.
		if (!bFirst && HasGroup(N, TEXT("continuation")) && !CheckGroup(N, TEXT("continuation"), true))
		{
			for (const TSharedPtr<FPlan>& K : Plan.Kids) Abort(*K);
			return EStatus::Success;
		}
		if (!bFirst && HasGroup(N, TEXT("interruption")) && !CheckGroup(N, TEXT("interruption"), true))
		{
			for (const TSharedPtr<FPlan>& K : Plan.Kids) Abort(*K);
			return EStatus::Failure;
		}
		return Child();
	}
	if (T == TEXT("CooldownDecorator"))
	{
		const EStatus S = Child();
		if (S == EStatus::Success || (S == EStatus::Failure && !Bool(N, TEXT("omitonfailure"), false)))
		{
			StartCooldown(N);
		}
		return S;
	}
	if (T == TEXT("FrequencyLimiter"))
	{
		const EStatus S = Child();
		if ((S == EStatus::Success && Bool(N, TEXT("stamponsuccess"), true)) || (S == EStatus::Failure && Bool(N, TEXT("stamponfailure"), false)))
		{
			FrequencyReady.Add(Plan.Node, Now + FMath::FRandRange(Num(N, TEXT("intervalmin"), 1.0), FMath::Max(Num(N, TEXT("intervalmin"), 1.0), Num(N, TEXT("intervalmax"), 1.0))));
		}
		return S;
	}
	if (T == TEXT("Timer"))
	{
		if (bFirst)
		{
			Plan.Duration = FMath::FRandRange(Num(N, TEXT("timemin"), 1.0), FMath::Max(Num(N, TEXT("timemin"), 1.0), Num(N, TEXT("timemax"), 1.0)));
		}
		if (Now - Plan.StartTime >= Plan.Duration)
		{
			for (const TSharedPtr<FPlan>& K : Plan.Kids) Abort(*K);
			return Bool(N, TEXT("succeedontimeout"), true) ? EStatus::Success : EStatus::Failure;
		}
		const EStatus S = Child();
		// Leaf that finished early (e.g. a short wait) repeats until the Timer ends.
		if (S == EStatus::Success && Plan.Kids.Num() && Asset->Nodes[Plan.Kids[0]->Node].Children.Num() == 0)
		{
			Plan.Kids[0]->bStarted = false;
			return EStatus::Running;
		}
		return S;
	}
	if (T == TEXT("StatusEffectDecorator"))
	{
		const FString Id = Str(N, TEXT("statuseffectid"));
		const bool bReset = Id.StartsWith(TEXT("ResetAI"));
		if (bFirst && bReset)
		{
			// Return home: drops the target; ResetAI restores health when it ends.
			bResetting = true;
			Target.Reset();
		}
		const EStatus S = Child();
		if (S != EStatus::Running && bReset)
		{
			bResetting = false;
			if (Id == TEXT("ResetAI"))
			{
				if (ANWCharacter* Me = Cast<ANWCharacter>(GetOwner())) Me->Combat->HealFull();
			}
		}
		return S;
	}
	if (T == TEXT("AlwaysSucceed"))
	{
		const EStatus S = Child();
		return S == EStatus::Running ? S : EStatus::Success;
	}
	if (T == TEXT("SetMannequinTag"))
	{
		// Tag on while the child runs (ChainAttack chains the bites in the game).
		const FName Tag(*Str(N, TEXT("tag")));
		Tags.Add(Tag);
		const EStatus S = Child();
		if (S != EStatus::Running)
		{
			Tags.Remove(Tag);
		}
		return S;
	}
	if (N.Children.Num() > 0)
	{
		// Selector, UtilitySelector, TreeReference, SetMannequinTag, conditions without an execution group...
		return Child();
	}
	return ExecuteLeaf(Plan, N, Dt);
}

UNWBehaviorTreeComponent::EStatus UNWBehaviorTreeComponent::ExecuteLeaf(FPlan& Plan, const FNWBTNode& N, float Dt)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	const double Now = GetWorld()->GetTimeSeconds();
	const double Elapsed = Now - Plan.StartTime;
	const FName T = N.Type;
	const TSharedPtr<FJsonObject> Nav = Asset->Profile->GetObjectField(TEXT("nav"));

	if (T == TEXT("SetCooldownAction"))
	{
		StartCooldown(N);
		return EStatus::Success;
	}
	if (T == TEXT("SetBlackboardFactAction"))
	{
		const FName Fact(*Str(N, TEXT("factname")));
		const double V = Num(N, TEXT("rightoperand"), 0.0);
		const int32 Op = int32(Num(N, TEXT("operator"), 1));
		FNWBBValue& B = BB.FindOrAdd(Fact);
		B.bVec = false;
		B.Num = Op == 2 ? B.Num + V : Op == 3 ? B.Num - V : V;
		return EStatus::Success;
	}
	if (T == TEXT("SelectPositionAction"))
	{
		FVector Pos;
		SelectPosition(N.P, TEXT("params."), Pos);
		const FString Fact = Str(N, TEXT("storagefact"));
		SetVec(FName(*(Fact.IsEmpty() ? FString(TEXT("Selected_Position")) : Fact)), Pos);
		return EStatus::Success;
	}
	if (T == TEXT("SelectWaypointAction"))
	{
		PathIndex = (PathIndex + 1) % FMath::Max(1, Path.Num());
		SetVec(TEXT("Waypoint_Pos"), Path[PathIndex]);
		return EStatus::Success;
	}
	if (T == TEXT("NavigationAction"))
	{
		const FString Config = Str(N, TEXT("navconfigtype"));
		const TSharedPtr<FJsonObject>* Cfg = nullptr;
		Nav->TryGetObjectField(Config, Cfg);
		auto CfgNum = [&](const TCHAR* K, double Def) { double V = Def; if (Cfg) (*Cfg)->TryGetNumberField(K, V); return V; };
		const double Speed = Num(N, TEXT("maxspeed"), CfgNum(TEXT("max"), 5.0)) * 100.0;
		const double Turn = Num(N, TEXT("turnrate"), CfgNum(TEXT("turn"), 360.0));
		const double Arrive = Num(N, TEXT("arrivalthreshold"), CfgNum(TEXT("arrive"), 0.5)) * 100.0;
		const double Repath = Num(N, TEXT("targetrepathdistance"), CfgNum(TEXT("repath"), 0.5)) * 100.0;
		const bool bSel = Bool(N, TEXT("hasselectpositionparams"), false);
		const int32 Origin = int32(Num(N, TEXT("selectpositionparams.origin.positiontype"), 1));
		auto Pick = [&]()
		{
			// Node parameters or, without them, the slice's navigation type ones (e.g. 1–2 m from the target, 120° arc).
			const TSharedPtr<FJsonObject>* Default = nullptr;
			if (bSel)
			{
				SelectPosition(N.P, TEXT("selectpositionparams."), Plan.Dest);
			}
			else if (Cfg && (*Cfg)->TryGetObjectField(TEXT("select"), Default))
			{
				SelectPosition(*Default, TEXT(""), Plan.Dest);
			}
			else
			{
				Plan.Dest = PositionOf(Origin);
			}
			Plan.LastTargetPos = PositionOf(1);
		};
		if (Elapsed <= 0.0)
		{
			Pick();
			// Gives up if it does not arrive in time (GiveUpBehavior 2 = failure).
			Plan.Duration = 2.0 + 2.0 * FVector::Dist2D(Me->GetActorLocation(), Plan.Dest) / FMath::Max(Speed, 50.0);
		}
		else if ((Origin == 1 || Origin == 12) && Target.IsValid() && FVector::Dist2D(Target->GetActorLocation(), Plan.LastTargetPos) > Repath)
		{
			Pick();   // the target moved more than TargetRepathDistance: recomputes the destination
		}
		if (FVector::Dist2D(Me->GetActorLocation(), Plan.Dest) <= FMath::Max(Arrive, 30.0))
		{
			return EStatus::Success;
		}
		if (Elapsed > Plan.Duration)
		{
			return EStatus::Failure;
		}
		const bool bFaceTarget = Config.StartsWith(TEXT("Strafe")) || int32(Num(N, TEXT("facingmode"), 0)) != 0;
		MoveToward(Plan.Dest, float(Speed), float(Turn), bFaceTarget, Dt);
		return EStatus::Running;
	}
	if (T == TEXT("CAGEAction"))
	{
		const FString Task = Str(N, TEXT("actionname"));
		if (Elapsed <= 0.0)
		{
			SetNum(FName(*(TEXT("CAGE_") + Task)), 1.0);   // the tree tests this with VariableExistsCondition
		}
		const TSharedPtr<FJsonObject>* MapPtr = nullptr;
		const TSharedPtr<FJsonObject> Map = Asset->Tasks->TryGetObjectField(Task, MapPtr) ? *MapPtr : nullptr;
		FString Action;
		if (Map && Map->TryGetStringField(TEXT("action"), Action))
		{
			UNWCombatComponent* C = Me->Combat;
			if (!Plan.bActionRequested)
			{
				// Turns to the target and requests the hit; if combat is busy, retries for up to 1 s.
				FaceTarget(1080.f, Dt * 4.f);
				const int32 Before = C->GetActionSerial();
				C->RequestAction(FName(*Action));
				if (C->GetActionSerial() != Before)
				{
					Plan.bActionRequested = true;
				}
				else if (Elapsed > 1.0)
				{
					return EStatus::Failure;
				}
				return EStatus::Running;
			}
			bool bAdvance = false;
			Map->TryGetBoolField(TEXT("advance"), bAdvance);
			if (bAdvance && C->IsBusy())
			{
				// Charge: keeps moving forward while the montage plays.
				Me->RunSpeed = 700.f;
				Me->AddMovementInput(Me->GetActorForwardVector(), 1.f);
			}
			if (C->IsBusy())
			{
				return EStatus::Running;
			}
			FString Tag;
			if (Map->TryGetStringField(TEXT("tag"), Tag))
			{
				Tags.Add(FName(*Tag));
			}
			return EStatus::Success;
		}
		if (Map && Map->HasField(TEXT("turn")))
		{
			FaceTarget(float(BB.FindRef(TEXT("CombatWalkMaxTurnSpeed")).Num > 0 ? BB.FindRef(TEXT("CombatWalkMaxTurnSpeed")).Num : 360.0), Dt);
			const double Angle = BB.FindRef(TEXT("Target_Angle")).Num;
			return FMath::Abs(Angle) < 10.0 || Elapsed > 1.5 || !Target.IsValid() ? EStatus::Success : EStatus::Running;
		}
		const TArray<TSharedPtr<FJsonValue>>* Step = nullptr;
		if (Map && Map->TryGetArrayField(TEXT("step"), Step))
		{
			// Dodge without its own animation in the set: quick step (0.35 s) back/sideways.
			const FVector Local((*Step)[0]->AsNumber(), (*Step)[1]->AsNumber(), 0.0);
			Me->RunSpeed = 600.f;
			Me->AddMovementInput(Me->GetActorTransform().TransformVectorNoScale(Local), 1.f);
			return Elapsed > 0.35 ? EStatus::Success : EStatus::Running;
		}
		if (Map && Map->HasField(TEXT("stun")))
		{
			return Me->Combat->State == ENWCombatState::Stunned || Elapsed < 0.2 ? EStatus::Running : EStatus::Success;
		}
		const TArray<TSharedPtr<FJsonValue>>* Wait = nullptr;
		if (Map && Map->TryGetArrayField(TEXT("wait"), Wait))
		{
			if (Elapsed <= 0.0)
			{
				Plan.Duration = FMath::FRandRange((*Wait)[0]->AsNumber(), (*Wait)[1]->AsNumber());
			}
			if (Target.IsValid())
			{
				FaceTarget(180.f, Dt);
			}
			return Elapsed >= Plan.Duration ? EStatus::Success : EStatus::Running;
		}
		// Task with no equivalent in the port (e.g. loose CAGE movement): short wait.
		return Elapsed > 0.3 ? EStatus::Success : EStatus::Running;
	}
	return EStatus::Success;   // LockTargetSelection and other leaves with no effect in the port
}

// ---------------------------------------------------------------- movement

void UNWBehaviorTreeComponent::FaceTarget(float TurnRate, float Dt)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!Me || !Target.IsValid())
	{
		return;
	}
	const FVector To = (Target->GetActorLocation() - Me->GetActorLocation()).GetSafeNormal2D();
	Me->SetActorRotation(FMath::RInterpConstantTo(Me->GetActorRotation(), FRotator(0.f, To.Rotation().Yaw, 0.f), Dt, TurnRate));
}

void UNWBehaviorTreeComponent::MoveToward(const FVector& Dest, float Speed, float TurnRate, bool bFaceTarget, float Dt)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	const FVector Dir = (Dest - Me->GetActorLocation()).GetSafeNormal2D();
	Me->bWalking = false;
	Me->RunSpeed = Speed;
	Me->GetCharacterMovement()->MaxWalkSpeed = Speed;
	Me->AddMovementInput(Dir, 1.f);
	if (bFaceTarget && Target.IsValid())
	{
		FaceTarget(TurnRate, Dt);
	}
	else
	{
		Me->SetActorRotation(FMath::RInterpConstantTo(Me->GetActorRotation(), FRotator(0.f, Dir.Rotation().Yaw, 0.f), Dt, TurnRate));
	}
}

// ---------------------------------------------------------------- sensor

void UNWBehaviorTreeComponent::OnHit(float Damage, ENWHitResult Result, AActor* Attacker)
{
	if (ANWCharacter* By = Cast<ANWCharacter>(Attacker); By && By->IsPlayerLike())
	{
		SetVec(TEXT("Last_Damage_Pos"), By->GetActorLocation());
		Aggro(By);
	}
}

void UNWBehaviorTreeComponent::Aggro(ANWCharacter* Who)
{
	if (!Who || bResetting || Target.Get() == Who)
	{
		return;
	}
	const bool bWasIdle = !Target.IsValid();
	Target = Who;
	TargetLostAt = -1.0;
	if (!bWasIdle || !Asset)
	{
		return;
	}
	// Group aggro (PerceptionComponent: distance and limit on how many).
	const TArray<TSharedPtr<FJsonValue>>* G = nullptr;
	if (!Asset->Profile->TryGetArrayField(TEXT("group_aggro"), G) || G->Num() < 2)
	{
		return;
	}
	const float Radius = float((*G)[0]->AsNumber()) * 100.f;
	int32 Cap = int32((*G)[1]->AsNumber());
	for (TActorIterator<ANWCreature> It(GetWorld()); It && Cap > 0; ++It)
	{
		if (*It == GetOwner() || FVector::Dist(It->GetActorLocation(), GetOwner()->GetActorLocation()) > Radius)
		{
			continue;
		}
		if (UNWBehaviorTreeComponent* Other = It->FindComponentByClass<UNWBehaviorTreeComponent>(); Other && Other->IsActive() && !Other->Target.IsValid())
		{
			Other->Aggro(Who);
			--Cap;
		}
	}
}

ANWCharacter* UNWBehaviorTreeComponent::LookForTarget() const
{
	// Passive profile (boar): only fights if hit or if the group fights.
	FString Targeting;
	Asset->Profile->TryGetStringField(TEXT("targeting"), Targeting);
	if (Targeting == TEXT("passive"))
	{
		return nullptr;
	}
	const TArray<TSharedPtr<FJsonValue>>* V = nullptr;
	if (!Asset->Profile->TryGetArrayField(TEXT("ambient_vision"), V) || V->Num() < 2)
	{
		return nullptr;
	}
	const float HalfAngle = float((*V)[0]->AsNumber()) * 0.5f;
	const float Dist = float((*V)[1]->AsNumber()) * 100.f;
	const AActor* Self = GetOwner();
	ANWCharacter* Best = nullptr;
	float BestD = Dist;
	UNWPlayerGrid* Grid = UNWPlayerGrid::Get(GetWorld());
	if (!Grid)
	{
		return nullptr;
	}
	// Only nearby players (grid), not the whole world (docs/tech/architecture.md, rule 1).
	Grid->ForEachNear(Self->GetActorLocation(), Dist, [&](ANWCharacter* It)
	{
		if (It->Combat->State == ENWCombatState::Dead)
		{
			return;
		}
		const FVector To = It->GetActorLocation() - Self->GetActorLocation();
		const float D = To.Size();
		if (D > BestD || FVector::DotProduct(Self->GetActorForwardVector(), To.GetSafeNormal()) < FMath::Cos(FMath::DegreesToRadians(HalfAngle)))
		{
			return;
		}
		FHitResult Hit;
		FCollisionQueryParams P(SCENE_QUERY_STAT(NWSight), false, Self);
		P.AddIgnoredActor(It);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Self->GetActorLocation() + FVector(0, 0, 50), It->GetActorLocation() + FVector(0, 0, 50), ECC_Visibility, P))
		{
			return;   // no line of sight
		}
		BestD = D;
		Best = It;
	});
	return Best;
}

void UNWBehaviorTreeComponent::UpdateSensor(float Dt)
{
	const AActor* Self = GetOwner();
	const double Now = GetWorld()->GetTimeSeconds();
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	const bool bLeashing = BB.FindRef(TEXT("SpawnLeashActive")).Num > 0.5 || BB.FindRef(TEXT("WaypointLeashActive")).Num > 0.5;
	if (bResetting || bLeashing)
	{
		Target.Reset();
	}
	if (Target.IsValid() && Target->Combat->State == ENWCombatState::Dead)
	{
		SetNum(TEXT("Target_Vitals_State"), 3.0);
		Target.Reset();
	}
	if (!Target.IsValid() && !bResetting && !bLeashing && Now >= NextLookAt)
	{
		NextLookAt = Now + LookEvery * FMath::FRandRange(0.8, 1.2);
		if (ANWCharacter* Seen = LookForTarget())
		{
			Aggro(Seen);
		}
	}
	ANWCharacter* T = Target.Get();
	if (!T)
	{
		Tags.Remove(TEXT("Alerted"));
	}
	SetNum(TEXT("Has_Target"), T ? 1.0 : 0.0);
	SetNum(TEXT("Full_Body_Reaction"), Me && Me->Combat->State == ENWCombatState::Stunned ? 1.0 : 0.0);
	if (!T)
	{
		BB.Remove(TEXT("Target_Pos"));
		SetNum(TEXT("Target_Visible"), 0.0);
		return;
	}
	const FVector To = T->GetActorLocation() - Self->GetActorLocation();
	SetVec(TEXT("Target_Pos"), T->GetActorLocation());
	SetNum(TEXT("Target_Dist"), To.Size() / 100.0);
	// Angle to the target in degrees, positive with the target on the left (game's Target_Angle).
	const float Yaw = FRotator::NormalizeAxis(To.Rotation().Yaw - Self->GetActorRotation().Yaw);
	SetNum(TEXT("Target_Angle"), -Yaw);
	// Difference between the target's front and mine (target with its back to me = near 0).
	SetNum(TEXT("Orientation_Delta"), FRotator::NormalizeAxis(T->GetActorRotation().Yaw - Self->GetActorRotation().Yaw));
	SetNum(TEXT("Target_Vitals_State"), 4.0);
	SetNum(TEXT("Target_Reachable"), 1.0);
	SetNum(TEXT("Target_Directly_Reachable"), 1.0);
	SetNum(TEXT("Time_Since_Target_Reachable"), 0.0);
	// Combat vision: loses the target after 15 s outside it (game's LastKnownPosition).
	const TArray<TSharedPtr<FJsonValue>>* V = nullptr;
	const float CombatDist = Asset->Profile->TryGetArrayField(TEXT("combat_vision"), V) && V->Num() > 1 ? float((*V)[1]->AsNumber()) * 100.f : 3000.f;
	const bool bVisible = To.Size() <= CombatDist;
	SetNum(TEXT("Target_Visible"), bVisible ? 1.0 : 0.0);
	if (!bVisible)
	{
		if (TargetLostAt < 0.0) TargetLostAt = Now;
		if (Now - TargetLostAt > 15.0) Target.Reset();
	}
	else
	{
		TargetLostAt = -1.0;
	}
}

// ---------------------------------------------------------------- loop

void UNWBehaviorTreeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!IsActive() || !Asset || !Me || !Me->HasAuthority() || Me->Combat->State == ENWCombatState::Dead)
	{
		return;
	}
	UpdateSensor(DeltaTime);
	const double Now = GetWorld()->GetTimeSeconds();
	// Reacting to a hit (stunned): the plan waits; the reaction belongs to combat.
	if (Me->Combat->State == ENWCombatState::Stunned)
	{
		return;
	}

	// Replan: every replan_seconds, when an observed key changes or when the plan has ended.
	double ReplanEvery = 1.0;
	Asset->Profile->TryGetNumberField(TEXT("replan_seconds"), ReplanEvery);
	bool bChanged = false;
	const TArray<TSharedPtr<FJsonValue>>* Keys = nullptr;
	if (Asset->Profile->TryGetArrayField(TEXT("replan_keys"), Keys))
	{
		WatchedValues.SetNumZeroed(Keys->Num());
		for (int32 i = 0; i < Keys->Num(); ++i)
		{
			const FNWBBValue* B = BB.Find(FName((*Keys)[i]->AsString()));
			const double V = B ? (B->bVec ? B->Vec.X + B->Vec.Y * 7.0 : B->Num) : -1.0;
			bChanged |= !FMath::IsNearlyEqual(V, WatchedValues[i], 1.0);
			WatchedValues[i] = V;
		}
	}
	if (!Current || bChanged || Now - LastEval >= FMath::Max(ReplanEvery, 0.1))
	{
		LastEval = Now;
		if (!Current || !RunningPathNonInterruptable(*Current))
		{
			if (TSharedPtr<FPlan> Plan = Evaluate(0))
			{
				TArray<int32> Sig;
				Signature(*Plan, Sig);
				if (!Current || Sig != CurrentSig)
				{
					if (Current)
					{
						Abort(*Current);
					}
					Current = Plan;
					CurrentSig = MoveTemp(Sig);
				}
			}
		}
	}
	if (Current && Execute(*Current, DeltaTime) != EStatus::Running)
	{
		Current.Reset();
		CurrentSig.Reset();
	}
}

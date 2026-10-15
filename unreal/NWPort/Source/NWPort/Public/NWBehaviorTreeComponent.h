#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWCombatTypes.h"
#include "NWBehaviorTreeComponent.generated.h"

class ANWCharacter;
class FJsonObject;

/** Node of the game's tree (Data/AI/<set>.json, tools/make_ai.py). */
struct FNWBTNode
{
	FName Type;
	FString Name;
	float Utility = 0.f;
	bool bNonInterruptable = false;
	TSharedPtr<FJsonObject> P;
	TArray<int32> Children;
};

/** Tree and profile of a creature, loaded once per set. */
struct FNWBTAsset
{
	TArray<FNWBTNode> Nodes;
	TSharedPtr<FJsonObject> Profile;
	TSharedPtr<FJsonObject> Tasks;
	static const FNWBTAsset* Get(FName Set);
};

/** Blackboard value: number, vector or text. */
struct FNWBBValue
{
	double Num = 0.0;
	FVector Vec = FVector::ZeroVector;
	FString Str;
	bool bVec = false;
};

/**
 * Creature AI identical to the game's: interprets the behavior tree from Server.pak.
 * Server only.
 *
 * Plan (Evaluate): selector takes the first child that evaluates; sequence requires all; UtilitySelector picks by
 * utility (highest or weighted draw, keeping the choice while it evaluates); conditions test the entry
 * group; cooldown and rate limiter fail while active. Execute: leaves run every tick until success
 * or failure; conditions check continuation (failed = ends with success) and interruption (failed = fails); cooldown
 * starts when the child finishes. Replans every `replan_seconds` or when an observed key changes, except
 * while a non-interruptible node is on the running path.
 *
 * What in the game belongs to the server and was cut from the client (target sensor, movement) is done here: target by
 * the slice vision (ambient/combat), damage taken and group aggro; straight-line movement with the speed and turn of
 * the slice navigation config.
 */
UCLASS(ClassGroup = (NW))
class NWPORT_API UNWBehaviorTreeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWBehaviorTreeComponent();

	/** Turns on the set's tree (wolf, boar). False if there is no Data/AI/<set>.json. */
	bool Start(FName Set, const FVector& Home, const TArray<FVector>& Waypoints);
	ANWCharacter* GetTarget() const { return Target.Get(); }
	/** Aggro coming from outside (group, damage). */
	void Aggro(ANWCharacter* Who);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	enum class EStatus : uint8 { Running, Success, Failure };

	// Plan: tree of the chosen nodes, with the execution state of each one.
	struct FPlan
	{
		int32 Node = INDEX_NONE;
		TArray<TSharedPtr<FPlan>> Kids;
		int32 Cursor = 0;
		bool bStarted = false;
		double StartTime = 0.0;
		double Duration = 0.0;
		FVector Dest = FVector::ZeroVector;
		FVector LastTargetPos = FVector::ZeroVector;
		bool bActionRequested = false;
		float Utility = 0.f;
	};

	// Plan and execute.
	TSharedPtr<FPlan> Evaluate(int32 Node);
	EStatus Execute(FPlan& Plan, float Dt);
	EStatus ExecuteLeaf(FPlan& Plan, const FNWBTNode& N, float Dt);
	void Abort(FPlan& Plan);
	void Signature(const FPlan& Plan, TArray<int32>& Out) const;
	bool RunningPathNonInterruptable(const FPlan& Plan) const;

	// Conditions and parameters.
	bool CheckGroup(const FNWBTNode& N, const TCHAR* Group, bool bDefault) const;
	bool HasGroup(const FNWBTNode& N, const TCHAR* Group) const;
	double Operand(const TSharedPtr<FJsonObject>& O, bool bDistance) const;
	double Num(const FNWBTNode& N, const TCHAR* Key, double Default) const;
	FString Str(const FNWBTNode& N, const TCHAR* Key) const;
	bool Bool(const FNWBTNode& N, const TCHAR* Key, bool Default) const;
	FVector PositionOf(int32 PositionType) const;
	bool SelectPosition(const TSharedPtr<FJsonObject>& Params, const FString& Prefix, FVector& Out) const;
	double NumIn(const TSharedPtr<FJsonObject>& Obj, const FString& Key, double Default) const;

	// Blackboard and sensor.
	void SetNum(FName Key, double V) { FNWBBValue& B = BB.FindOrAdd(Key); B.Num = V; B.bVec = false; }
	void SetVec(FName Key, const FVector& V) { FNWBBValue& B = BB.FindOrAdd(Key); B.Vec = V; B.bVec = true; }
	void UpdateSensor(float Dt);
	ANWCharacter* LookForTarget() const;

	// Named cooldowns (CooldownDecorator, SetCooldownAction, IsCooldownActiveCondition).
	void StartCooldown(const FNWBTNode& N);
	bool CooldownActive(FName Name) const;

	// Movement.
	void MoveToward(const FVector& Dest, float Speed, float TurnRate, bool bFaceTarget, float Dt);
	void FaceTarget(float TurnRate, float Dt);

	UFUNCTION() void OnHit(float Damage, ENWHitResult Result, AActor* Attacker);

	const FNWBTAsset* Asset = nullptr;
	FName SetName;
	TMap<FName, FNWBBValue> BB;
	TMap<FName, double> Cooldowns;
	TMap<int32, double> FrequencyReady;
	TMap<int32, int32> PreservedChoice;
	TSharedPtr<FPlan> Current;
	TArray<int32> CurrentSig;
	double LastEval = -100.0;
	TArray<double> WatchedValues;
	TWeakObjectPtr<ANWCharacter> Target;
	/** Next target search (vision): every LookEvery s, offset per creature, not every cycle. */
	double NextLookAt = 0.0;
	static constexpr double LookEvery = 0.25;
	double TargetLostAt = -1.0;
	FVector HomePos = FVector::ZeroVector;
	TArray<FVector> Path;
	int32 PathIndex = 0;
	bool bResetting = false;
	// Mannequin tags the tree tests (Alerted, ChainAttack...): set by tasks and by SetMannequinTag.
	TSet<FName> Tags;
};

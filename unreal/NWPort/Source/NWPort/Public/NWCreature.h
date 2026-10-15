#pragma once

#include "CoreMinimal.h"
#include "NWCharacter.h"
#include "NWCreature.generated.h"

/** Game creature (javelindata_vitals + converted set), from Data/World/<region>_spawns.json (tools/make_spawns.py). */
struct FNWCreatureDef
{
	FName Id;                 // the game's VitalsID (Boar_StarterBeach)
	FString NameKey;          // game text
	FName Set;                // set (boar, wolf); empty = not converted yet
	int32 Level = 1;
	float Health = 500.f;     // port scale (make_spawns.py)
	float Damage = 100.f;
	float Mitigation = 0.f;
	float Scale = 1.f;
	bool bHostile = true;
	// Carcass (skinning): loot, time, trade skill, trade skill XP, name.
	FName SkinLoot;
	float SkinTime = 0.f;
	FName SkinTradeskill;
	int32 SkinTradeXp = 0;
	FString SkinNameKey;
};

class NWPORT_API FNWCreatureCatalog
{
public:
	static FNWCreatureCatalog& Get();
	const FNWCreatureDef* Find(FName Id) const { return Defs.Find(Id); }
	/** Reads the "creatures" from the spawns file (once per file). */
	void Load(const FString& File);

private:
	TMap<FName, FNWCreatureDef> Defs;
	TSet<FString> Loaded;
};

/**
 * Creature (G3): character with the set's mesh, animations and action table, and health and damage from the game.
 * The id is replicated: clients build the assets from the same catalog. Without a target, follows the spawner's waypoints
 * (game tasks: graze, patrol) walking; when hostile, chases and attacks whoever comes close (ANWCharacter::TickDummy)
 * and goes back home if it gets too far. When dead, the carcass stays to be skinned (knife) until it disappears.
 */
UCLASS()
class NWPORT_API ANWCreature : public ANWCharacter
{
	GENERATED_BODY()

public:
	ANWCreature(const FObjectInitializer& ObjectInitializer);

	/** Server, before FinishSpawning. */
	void Setup(const FNWCreatureDef& Def, const TArray<FVector>& InPath, const TArray<float>& InWaits);
	/** Definition by id (the catalog loads the region file the first time, also on the client). */
	const FNWCreatureDef* Def() const;
	/** Definition of any creature in the catalog (valid on any map, e.g. the arena). */
	static const FNWCreatureDef* FindDef(FName Id);

	UPROPERTY(ReplicatedUsing = OnRep_Creature) FName CreatureId;
	/** Carcass already skinned (cannot be skinned again). */
	UPROPERTY(Replicated) bool bSkinned = false;
	/** Who is skinning (server). */
	TWeakObjectPtr<AActor> SkinnedBy;

	virtual void Tick(float DeltaSeconds) override;
	// The game's AI (the set's behavior tree, Data/AI); without a tree, uses UNWBrainComponent.
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UNWBehaviorTreeComponent> Behavior;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void OnRep_Creature();
	void ApplyAssets();
	void TickPatrol(float DeltaSeconds);
	bool HasTarget() const;

	TArray<FVector> Path;
	TArray<float> Waits;
	int32 PathIndex = 0;
	float WaitLeft = 0.f;
	FVector Home = FVector::ZeroVector;
	bool bReturning = false;
	bool bAssetsApplied = false;
	bool bUseBehaviorTree = false;
	float WalkSpeed = 150.f;
	float ChaseSpeed = 450.f;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "NWArmaManopla.generated.h"

class ANWCharacter;
class UNWCombatComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
struct FNWWeaponSet;

/**
 * Ice Gauntlet (sub-area 1a, docs/game/weapons/ice-gauntlet.md): weapon registration and the skinned glove, which follows
 * the body pose. Combat uses UNWCombatComponent through the DT_IceGauntletActions table; mana, UNWManaComponent; the
 * abilities, UNWIceGauntletComponent (created on the server when the gauntlet is equipped).
 */
namespace NWArmaManopla
{
	void AddWeaponSet(TArray<FNWWeaponSet>& Sets);
	// Shows the skinned mesh of the active weapon (FNWWeaponSet::SkinMesh), or hides it if the weapon has none.
	void ApplySkin(ANWCharacter* Character, const FNWWeaponSet& Set);
}

/** Shape of an ice area (sphere or box), in cm, in world space. */
USTRUCT()
struct FNWIceShape
{
	GENERATED_BODY()
	UPROPERTY() FVector Center = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY() FVector Extent = FVector::ZeroVector;   // box: half-extent; sphere: X = radius
	UPROPERTY() bool bBox = false;
};

/**
 * Ice area on the ground (Ice Storm, Ice Shower): lasts Seconds; each Tick applies the damage row to whoever is
 * inside and keeps the slow status; whoever enters may be rooted (with immunity afterwards) and whoever leaves gets the
 * lingering slow. The server decides; everyone draws.
 */
UCLASS()
class NWPORT_API ANWIceArea : public AActor
{
	GENERATED_BODY()

public:
	ANWIceArea();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) TArray<FNWIceShape> Shapes;
	UPROPERTY(Replicated) float Seconds = 5.f;
	// Game effect converted to Niagara (tools/ue/import_fx.py); empty = plain blue discs/boxes.
	UPROPERTY(Replicated) FString FxPath;

	TWeakObjectPtr<UNWCombatComponent> Source;
	FName DamageID;            // damage per tick (empty = no damage)
	float TickSeconds = 0.33f;
	FName SlowID;              // slow while inside
	float SlowMove = 0.f;
	FName RootID;              // root on entering (Shower)
	float RootSeconds = 0.f;
	float RootImmunity = 0.f;
	FName LingerID;            // slow after leaving
	float LingerSeconds = 0.f;

protected:
	virtual void BeginPlay() override;

private:
	void Build();
	bool Inside(const FVector& P) const;
	float Age = 0.f;
	float NextTick = 0.f;
	TSet<TWeakObjectPtr<UNWCombatComponent>> InsideNow;
	TMap<TWeakObjectPtr<UNWCombatComponent>, float> RootImmuneUntil;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Visuals;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Mat;
};

/** Ice mesh for a while (spikes, tomb): replicated, no collision. */
UCLASS()
class NWPORT_API ANWIceProp : public AActor
{
	GENERATED_BODY()

public:
	ANWIceProp();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static ANWIceProp* Spawn(UWorld* World, const TCHAR* MeshPath, const FTransform& Where, float Seconds,
		const TArray<FString>& Fx = {}, bool bHideMesh = false);

	UPROPERTY(ReplicatedUsing = OnRep_Mesh) FString MeshPath;
	// Game effects attached to the mesh; bHideMesh when the effect itself already brings the mesh (spikes).
	UPROPERTY(ReplicatedUsing = OnRep_Fx) TArray<FString> FxPaths;
	UPROPERTY(Replicated) bool bHideMesh = false;
	UFUNCTION() void OnRep_Fx();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
	UFUNCTION() void OnRep_Mesh();
};

/** Ice Pylon: shoots IceMagic_Pylon projectiles at the nearest enemy within 20 m; disappears when its life ends. */
UCLASS()
class NWPORT_API ANWIcePylon : public ANWIceProp
{
	GENERATED_BODY()

public:
	ANWIcePylon();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	TWeakObjectPtr<UNWCombatComponent> Source;
	float FireAt = 0.f;
};

/**
 * Ice Gauntlet abilities on the server: follows the UNWCombatComponent's current action and does what the action
 * asks at the game's timing (spikes in a line, wind with damage ticks, areas, pylon, tomb), switches phase by time
 * (ForceAction) and starts the cooldowns the game starts at the end (StartCooldown).
 */
UCLASS(ClassGroup = (NW))
class NWPORT_API UNWIceGauntletComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWIceGauntletComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void Enter(FName Action);
	void Exit(FName Action, FName Next);
	void Step(FName Action, float T);
	FVector Forward() const;
	FVector GroundAhead(float MaxCm) const;
	void CapsuleHit(const FVector& Start, const FVector& Dir, float FromCm, float ToCm, float RadiusCm, FName DamageID, TSet<AActor*>& Done);
	void SpawnSpike(float DistanceCm, bool bEnd);
	UNWCombatComponent* Combat() const;

	FName Current;
	float StartedAt = 0.f;       // start of the current action
	float PhaseStartedAt = 0.f;  // start of the looping phase (wind, tomb)
	int32 Done = 0;              // events already done in the action (spikes, ticks, boxes)
	FVector Origin;              // position and direction fixed on entry (spikes, shower)
	FVector Dir;
	FVector GroundTarget;
	TWeakObjectPtr<ANWIcePylon> Pylon;
	TWeakObjectPtr<ANWIceProp> Tomb;
	bool bPylonAlive = false;
};

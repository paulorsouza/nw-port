#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWProjectile.generated.h"

class UNWCombatComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Flight parameters (action row). Zero = default of the Hatchet throw. */
struct FNWProjectileParams
{
	float Speed = 0.f;          // cm/s
	float Radius = 0.f;         // cm
	float GravityDelay = 0.f;   // cm in a straight line before gravity (arrow: 5 m)
	float Range = 2500.f;       // cm; disappears afterwards
	bool bPierce = false;       // passes through targets (one hit per target)
	float ExplodeRadius = 0.f;  // cm; explodes on hit or impact
	FName ExplodeDamageID;
	float CloudRadius = 0.f;    // cm; cloud where it lands (poison)
	float CloudSeconds = 0.f;
	FName StatusID;             // cloud status
	float StatusSeconds = 0.f;
	float StatusTick = 1.f;
	float StatusDamage = 0.f;   // absolute per tick
	FString FxPath;             // projectile's Niagara system (replaces the mesh and the trail)
};

/**
 * Projectile: thrown weapon (Hatchet, spins and drops a bit) or arrow (Bow: 100 m/s, straight for 5 m and then
 * gravity of -9.8 m/s², the game's radb Bow and basic_arrow slice). On the first character it touches it applies the damage
 * of the DamageID row through the shooter's combat; an arrow passing near the head bone counts as a headshot.
 */
UCLASS()
class NWPORT_API ANWProjectile : public AActor
{
	GENERATED_BODY()

public:
	ANWProjectile();

	// Before FinishSpawning.
	void Setup(UNWCombatComponent* InOwnerCombat, FName InDamageID, UStaticMesh* Mesh, const FNWProjectileParams& Params);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;
	// Arrow trail (at 100 m/s it vanishes in a few frames; the game draws a trail).
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Trail;

	UPROPERTY(EditAnywhere) float Speed = 3000.f;
	UPROPERTY(EditAnywhere) float SpinDegreesPerSecond = 1080.f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void OnOverlap(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
	UFUNCTION() void OnStop(const FHitResult& Hit);
	bool IsHeadHit(const AActor* Target) const;
	void Impact(const FVector& At);

	TWeakObjectPtr<UNWCombatComponent> OwnerCombat;
	FName DamageID;
	FNWProjectileParams P;
	FVector StartLocation = FVector::ZeroVector;
	FVector PrevLocation = FVector::ZeroVector;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	UPROPERTY(ReplicatedUsing = OnRep_Mesh) TObjectPtr<UStaticMesh> ReplicatedMesh;
	// Arrow: points where it flies and does not spin (replicated so clients draw it the same).
	UPROPERTY(Replicated) bool bArrow = false;
	UPROPERTY(Replicated) FString FxPath;
	bool bDone = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION() void OnRep_Mesh();
};

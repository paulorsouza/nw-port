#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWFxActor.generated.h"

class UNWCombatComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UENUM()
enum class ENWFx : uint8
{
	Explosion,   // flash that grows up to the radius (Explosive Arrow)
	PoisonCloud, // green cloud; on the server it poisons whoever is inside (Poison Shot: 3 m, 6 s)
	ArrowRain,   // arrows falling in the area (Rain of Arrows)
};

/**
 * Simple combat effect, made with basic meshes and the M_NW_FX material (the game particles are not
 * converted yet). Replicated: the server spawns, everyone draws. The poison cloud also applies the status on the server.
 */
UCLASS()
class NWPORT_API ANWFxActor : public AActor
{
	GENERATED_BODY()

public:
	ANWFxActor();

	static ANWFxActor* Spawn(UWorld* World, ENWFx Type, const FVector& At, float Radius, float Seconds, UNWCombatComponent* Source = nullptr);

	// Cloud: status applied to whoever enters (server).
	FName StatusID;
	float StatusSeconds = 0.f;
	float StatusTick = 1.f;
	float StatusDamage = 0.f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated) ENWFx Type = ENWFx::Explosion;
	UPROPERTY(Replicated) float Radius = 100.f;
	UPROPERTY(Replicated) float Seconds = 1.f;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Shape;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Arrows;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Mat;
	TArray<FVector> ArrowStart;
	TWeakObjectPtr<UNWCombatComponent> Source;
	float Age = 0.f;
	float NextApply = 0.f;
	void Build();
};

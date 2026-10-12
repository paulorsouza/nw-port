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
	Explosion,   // clarão que cresce até o raio (Flecha Explosiva)
	PoisonCloud, // nuvem verde; no servidor envenena quem está dentro (Disparo Venenoso: 3 m, 6 s)
	ArrowRain,   // flechas caindo na área (Chuva de Flechas)
};

/**
 * Efeito simples de combate, feito com malhas básicas e o material M_NW_FX (as partículas do jogo ainda não são
 * convertidas). Replicado: o servidor cria, todos desenham. A nuvem de veneno também aplica o status no servidor.
 */
UCLASS()
class NWPORT_API ANWFxActor : public AActor
{
	GENERATED_BODY()

public:
	ANWFxActor();

	static ANWFxActor* Spawn(UWorld* World, ENWFx Type, const FVector& At, float Radius, float Seconds, UNWCombatComponent* Source = nullptr);

	// Nuvem: status aplicado a quem entra (servidor).
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

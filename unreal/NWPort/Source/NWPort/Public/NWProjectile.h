#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWProjectile.generated.h"

class UNWCombatComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Parâmetros do voo (linha da ação). Zero = padrão do arremesso da machadinha. */
struct FNWProjectileParams
{
	float Speed = 0.f;          // cm/s
	float Radius = 0.f;         // cm
	float GravityDelay = 0.f;   // cm em linha reta antes da gravidade (flecha: 5 m)
	float Range = 2500.f;       // cm; some depois
	bool bPierce = false;       // atravessa alvos (um acerto por alvo)
	float ExplodeRadius = 0.f;  // cm; explode ao acertar ou bater
	FName ExplodeDamageID;
	float CloudRadius = 0.f;    // cm; nuvem onde cai (veneno)
	float CloudSeconds = 0.f;
	FName StatusID;             // status da nuvem
	float StatusSeconds = 0.f;
	float StatusTick = 1.f;
	float StatusDamage = 0.f;   // absoluto por tique
	FString FxPath;             // sistema Niagara do projétil (troca a malha e o rastro)
};

/**
 * Projétil: arma arremessada (machadinha, gira e cai um pouco) ou flecha (arco: 100 m/s, reta por 5 m e depois
 * gravidade de -9,8 m/s², radb Bow e slice basic_arrow do jogo). No primeiro personagem que tocar aplica o dano da
 * linha DamageID pelo combate de quem atirou; flecha que passa perto do osso da cabeça conta como acerto na cabeça.
 */
UCLASS()
class NWPORT_API ANWProjectile : public AActor
{
	GENERATED_BODY()

public:
	ANWProjectile();

	// Antes de FinishSpawning.
	void Setup(UNWCombatComponent* InOwnerCombat, FName InDamageID, UStaticMesh* Mesh, const FNWProjectileParams& Params);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;
	// Rastro da flecha (a 100 m/s ela some em poucos quadros; o jogo desenha um rastro).
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
	// Flecha: aponta para onde voa e não gira (replicado para os clientes desenharem igual).
	UPROPERTY(Replicated) bool bArrow = false;
	UPROPERTY(Replicated) FString FxPath;
	bool bDone = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION() void OnRep_Mesh();
};

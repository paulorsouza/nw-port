#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWProjectile.generated.h"

class UNWCombatComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Arma arremessada (habilidades de arremesso da machadinha). Voa reto com um pouco de gravidade, girando;
 * no primeiro personagem que tocar aplica o dano da linha DamageID pelo combate de quem arremessou.
 */
UCLASS()
class NWPORT_API ANWProjectile : public AActor
{
	GENERATED_BODY()

public:
	ANWProjectile();

	// Antes de FinishSpawning.
	void Setup(UNWCombatComponent* InOwnerCombat, FName InDamageID, UStaticMesh* Mesh, float Range);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(EditAnywhere) float Speed = 3000.f;
	UPROPERTY(EditAnywhere) float SpinDegreesPerSecond = 1080.f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void OnOverlap(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
	UFUNCTION() void OnStop(const FHitResult& Hit);

	TWeakObjectPtr<UNWCombatComponent> OwnerCombat;
	FName DamageID;
	UPROPERTY(ReplicatedUsing = OnRep_Mesh) TObjectPtr<UStaticMesh> ReplicatedMesh;
	bool bDone = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION() void OnRep_Mesh();
};

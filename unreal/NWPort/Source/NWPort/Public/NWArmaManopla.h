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
 * Manopla de gelo (sub-esteira 1a, docs/game/weapons/ice-gauntlet.md): registro da arma e a luva com pele, que segue a pose
 * do corpo. O combate usa o UNWCombatComponent pela tabela DT_IceGauntletActions; a mana, o UNWManaComponent; as
 * habilidades, o UNWIceGauntletComponent (criado no servidor quando a manopla é equipada).
 */
namespace NWArmaManopla
{
	void AddWeaponSet(TArray<FNWWeaponSet>& Sets);
	// Mostra a malha com pele da arma ativa (FNWWeaponSet::SkinMesh) ou esconde, se a arma não tiver.
	void ApplySkin(ANWCharacter* Character, const FNWWeaponSet& Set);
}

/** Forma de uma área de gelo (esfera ou caixa), em cm, no espaço do mundo. */
USTRUCT()
struct FNWIceShape
{
	GENERATED_BODY()
	UPROPERTY() FVector Center = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY() FVector Extent = FVector::ZeroVector;   // caixa: meia-extensão; esfera: X = raio
	UPROPERTY() bool bBox = false;
};

/**
 * Área de gelo no chão (Tempestade de Gelo, Chuva de Gelo): dura Seconds; a cada Tick aplica a linha de dano em quem
 * está dentro e mantém o status de lentidão; quem entra pode ser enraizado (com imunidade depois) e quem sai leva a
 * lentidão residual. Servidor decide; todos desenham.
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
	// Efeito do jogo convertido para Niagara (tools/ue/import_fx.py); vazio = discos/caixas azuis simples.
	UPROPERTY(Replicated) FString FxPath;

	TWeakObjectPtr<UNWCombatComponent> Source;
	FName DamageID;            // dano por tique (vazio = sem dano)
	float TickSeconds = 0.33f;
	FName SlowID;              // lentidão enquanto dentro
	float SlowMove = 0.f;
	FName RootID;              // enraizar ao entrar (Chuva)
	float RootSeconds = 0.f;
	float RootImmunity = 0.f;
	FName LingerID;            // lentidão depois de sair
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

/** Malha de gelo por um tempo (estacas, tumba): replicada, sem colisão. */
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
	// Efeitos do jogo presos à malha; bHideMesh quando o próprio efeito já traz a malha (estacas).
	UPROPERTY(ReplicatedUsing = OnRep_Fx) TArray<FString> FxPaths;
	UPROPERTY(Replicated) bool bHideMesh = false;
	UFUNCTION() void OnRep_Fx();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
	UFUNCTION() void OnRep_Mesh();
};

/** Torre de Gelo: atira projéteis IceMagic_Pylon no inimigo mais perto até 20 m; some ao fim da vida. */
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
 * Habilidades da manopla de gelo no servidor: acompanha a ação em curso do UNWCombatComponent e faz o que a ação
 * pede no tempo do jogo (estacas em linha, vento com tiques de dano, áreas, torre, tumba), troca de fase pelo tempo
 * (ForceAction) e inicia as recargas que o jogo começa no fim (StartCooldown).
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
	float StartedAt = 0.f;       // começo da ação atual
	float PhaseStartedAt = 0.f;  // começo da fase em loop (vento, tumba)
	int32 Done = 0;              // eventos já feitos na ação (estacas, tiques, caixas)
	FVector Origin;              // posição e direção fixadas na entrada (estacas, chuva)
	FVector Dir;
	FVector GroundTarget;
	TWeakObjectPtr<ANWIcePylon> Pylon;
	TWeakObjectPtr<ANWIceProp> Tomb;
	bool bPylonAlive = false;
};

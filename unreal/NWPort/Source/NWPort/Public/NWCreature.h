#pragma once

#include "CoreMinimal.h"
#include "NWCharacter.h"
#include "NWCreature.generated.h"

/** Criatura do jogo (javelindata_vitals + conjunto convertido), de Data/World/<região>_spawns.json (tools/make_spawns.py). */
struct FNWCreatureDef
{
	FName Id;                 // VitalsID do jogo (Boar_StarterBeach)
	FString NameKey;          // texto do jogo
	FName Set;                // conjunto (boar, wolf); vazio = ainda não convertido
	int32 Level = 1;
	float Health = 500.f;     // escala do port (make_spawns.py)
	float Damage = 100.f;
	float Mitigation = 0.f;
	float Scale = 1.f;
	bool bHostile = true;
	// Carcaça (esfolar): saque, tempo, ofício, XP de ofício, nome.
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
	/** Lê os "creatures" do arquivo de spawns (uma vez por arquivo). */
	void Load(const FString& File);

private:
	TMap<FName, FNWCreatureDef> Defs;
	TSet<FString> Loaded;
};

/**
 * Criatura (G3): personagem com a malha, as animações e a tabela de ações do conjunto, vida e dano do jogo.
 * O id é replicado: os clientes montam os assets pelo mesmo catálogo. Sem alvo, segue os waypoints do spawner
 * (tarefas do jogo: pastar, patrulhar) andando; hostil, persegue e ataca quem chegar perto (ANWCharacter::TickDummy)
 * e volta para casa se for longe demais. Morta, a carcaça fica para esfolar (faca) até sumir.
 */
UCLASS()
class NWPORT_API ANWCreature : public ANWCharacter
{
	GENERATED_BODY()

public:
	ANWCreature(const FObjectInitializer& ObjectInitializer);

	/** Servidor, antes de FinishSpawning. */
	void Setup(const FNWCreatureDef& Def, const TArray<FVector>& InPath, const TArray<float>& InWaits);
	/** Definição pelo id (o catálogo carrega o arquivo da região na primeira vez, também no cliente). */
	const FNWCreatureDef* Def() const;
	/** Definição de qualquer criatura do catálogo (vale em qualquer mapa, ex.: a arena). */
	static const FNWCreatureDef* FindDef(FName Id);

	UPROPERTY(ReplicatedUsing = OnRep_Creature) FName CreatureId;
	/** Carcaça já esfolada (não dá para esfolar de novo). */
	UPROPERTY(Replicated) bool bSkinned = false;
	/** Quem está esfolando (servidor). */
	TWeakObjectPtr<AActor> SkinnedBy;

	virtual void Tick(float DeltaSeconds) override;
	// IA do jogo (árvore de comportamento do conjunto, Data/AI); sem árvore, usa o UNWBrainComponent.
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

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "NWGatherField.generated.h"

class ANWGatherField;
class UStaticMesh;
class UInstancedStaticMeshComponent;

/** Coletável esgotado e quando volta (tempo do servidor). */
USTRUCT()
struct FNWDepletedNode : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY() int32 Index = INDEX_NONE;
	UPROPERTY() float RespawnAt = 0.f;

	void PostReplicatedAdd(const struct FNWDepletedList& List);
	void PreReplicatedRemove(const struct FNWDepletedList& List);
};

USTRUCT()
struct FNWDepletedList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY() TArray<FNWDepletedNode> Items;
	UPROPERTY(NotReplicated) TObjectPtr<ANWGatherField> Owner;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& Params)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FNWDepletedNode, FNWDepletedList>(Items, Params, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FNWDepletedList> : public TStructOpsTypeTraitsBase2<FNWDepletedList>
{
	enum { WithNetDeltaSerializer = true };
};

/**
 * Coletáveis de uma região (Data/World/<região>_gatherables.json, tools/make_gatherables.py): os ~100 mil
 * arbustos, árvores, pedras, minérios, plantas e alquimia da paisagem do jogo. Servidor e cliente leem o mesmo
 * arquivo, então o índice do coletável é o mesmo dos dois lados; só o que está esgotado é replicado.
 * Uma malha instanciada por variante (cheia) e outra para a esgotada; esgotar troca a instância de lugar
 * (a cheia desce para baixo do chão, a esgotada sobe). Malhas em /Game/NW/Gatherables/<nome do .cgf>
 * (tools/ue/import_gatherables.py: /Game/NW/Gatherables/<nome>/<nome>); sem a malha o coletável funciona sem aparecer. Colisão só em minérios,
 * pedras e troncos; o servidor dedicado só cria esses (não desenha nada).
 */
UCLASS()
class NWPORT_API ANWGatherField : public AActor
{
	GENERATED_BODY()

public:
	ANWGatherField();

	struct FVariant
	{
		FName Id;
		FName Gatherable;
		FString Mesh, DepletedMesh;
		float Radius = 300.f;
		FString NameKey;
		FName Tradeskill, Action, Restriction, Loot;
		float Time = 0.f;
		int32 RequiredLevel = 0;   // RequiredTradeskillLevel (ainda não exigido; docs/backlog.md)
		int32 TradeXp = 0;         // XP de ofício por coleta (evento do coletável)
		float RespawnMin = 0.f, RespawnMax = 0.f;
		bool bCollides = false;
		UInstancedStaticMeshComponent* Full = nullptr;
		UInstancedStaticMeshComponent* Depleted = nullptr;
	};
	struct FNode
	{
		int32 Variant = 0;
		FTransform Transform;
		int32 FullInstance = INDEX_NONE;
		int32 DepletedInstance = INDEX_NONE;
		bool bDepleted = false;
	};

	/** Campo da região (há no máximo um por mapa). */
	static ANWGatherField* Get(const UWorld* World);
	/** Arquivo de coletáveis do mapa atual ou vazio (mapa sem coleta). */
	static FString DataFileFor(const UWorld* World);

	int32 Num() const { return Nodes.Num(); }
	const FNode& Node(int32 Index) const { return Nodes[Index]; }
	const FVariant& VariantOf(int32 Index) const { return Variants[Nodes[Index].Variant]; }
	bool IsDepleted(int32 Index) const { return Nodes.IsValidIndex(Index) && Nodes[Index].bDepleted; }
	/** Coletável disponível mais perto de Location dentro do raio de interação (ou INDEX_NONE). */
	int32 FindNear(const FVector& Location, const FVector& Facing) const;
	/** Distância horizontal até o coletável (cm) cabe no raio de interação + folga? */
	bool InReach(int32 Index, const FVector& Location, float Slack = 0.f) const;

	// ---- servidor
	/** Esgota o coletável e marca a volta no respawn do jogo. */
	void Deplete(int32 Index);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetDepletedVisual(int32 Index, bool bDepleted);

private:
	bool Load(const FString& File);
	float ServerNow() const;

	UPROPERTY(Replicated) FNWDepletedList Depleted;

	TArray<FVariant> Variants;
	TArray<FNode> Nodes;
	// Grade de 20 m para a busca por proximidade (índices de Nodes por célula).
	TMap<FIntPoint, TArray<int32>> Grid;
	static constexpr float CellSize = 2000.f;
	float NextRespawnCheck = 0.f;
};

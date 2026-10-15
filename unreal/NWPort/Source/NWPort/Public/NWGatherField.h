#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "NWGatherField.generated.h"

class ANWGatherField;
class UStaticMesh;
class UInstancedStaticMeshComponent;

/** Depleted gatherable and when it comes back (server time). */
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
 * Gatherables of a region (Data/World/<region>_gatherables.json, tools/make_gatherables.py): the ~100k
 * bushes, trees, rocks, ores, plants and alchemy of the game landscape. Server and client read the same
 * file, so the gatherable index is the same on both sides; only what is depleted is replicated.
 * One instanced mesh per variant (full) and another for the depleted one; depleting moves the instance
 * (the full one goes below the ground, the depleted one comes up). Meshes in /Game/NW/Gatherables/<.cgf name>
 * (tools/ue/import_gatherables.py: /Game/NW/Gatherables/<name>/<name>); without a mesh the gatherable works unseen. Collision only on ores,
 * rocks and trunks; the dedicated server only creates those (draws nothing).
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
		int32 RequiredLevel = 0;   // RequiredTradeskillLevel (not enforced yet; docs/backlog.md)
		int32 TradeXp = 0;         // trade skill XP per gather (gatherable event)
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

	/** Region field (at most one per map). */
	static ANWGatherField* Get(const UWorld* World);
	/** Gatherables file of the current map or empty (map without gathering). */
	static FString DataFileFor(const UWorld* World);

	int32 Num() const { return Nodes.Num(); }
	const FNode& Node(int32 Index) const { return Nodes[Index]; }
	const FVariant& VariantOf(int32 Index) const { return Variants[Nodes[Index].Variant]; }
	bool IsDepleted(int32 Index) const { return Nodes.IsValidIndex(Index) && Nodes[Index].bDepleted; }
	/** Closest available gatherable to Location within the interaction radius (or INDEX_NONE). */
	int32 FindNear(const FVector& Location, const FVector& Facing) const;
	/** Does the horizontal distance to the gatherable (cm) fit in the interaction radius + margin? */
	bool InReach(int32 Index, const FVector& Location, float Slack = 0.f) const;

	// ---- server
	/** Depletes the gatherable and schedules its return at the game respawn. */
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
	// 20 m grid for the proximity search (indices of Nodes per cell).
	TMap<FIntPoint, TArray<int32>> Grid;
	static constexpr float CellSize = 2000.f;
	float NextRespawnCheck = 0.f;
};

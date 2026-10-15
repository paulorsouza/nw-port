#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWNpc.generated.h"

class UCapsuleComponent;
class UNWInteractableComponent;
class USkeletalMeshComponent;

/**
 * Standing city NPC (area 4, docs/game/city.md): appearance from the .cdf, idle animation from the IdleTimeline, name.
 * Created by ANWNpcField on each machine (not replicated: it is fixed and comes from the same data file).
 * Interacting turns the NPC to the player and, if it is the week's drawn one for the trade skill, delivers the gift
 * (FNWNpcRewards, server); no dialogue, because the game quests are not converted.
 */
UCLASS()
class NWPORT_API ANWNpc : public AActor
{
	GENERATED_BODY()

public:
	ANWNpc();
	void Setup(const FString& InVariant, const FText& InName, const FString& Mesh, const FString& Anim);

	UPROPERTY(VisibleAnywhere, Category = "NW|Npc") TObjectPtr<UCapsuleComponent> Capsule;
	UPROPERTY(VisibleAnywhere, Category = "NW|Npc") TObjectPtr<USkeletalMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "NW|Npc") TObjectPtr<UNWInteractableComponent> Interact;
	/** Game VariantID (javelindata_variations_npcs). */
	UPROPERTY(VisibleAnywhere, Category = "NW|Npc") FString Variant;
	UPROPERTY(VisibleAnywhere, Category = "NW|Npc") FText DisplayName;
	/** Trade skill (gift group, tools/npc_rewards.json) and map city. */
	UPROPERTY(VisibleAnywhere, Category = "NW|Npc") FString Type;
	UPROPERTY(VisibleAnywhere, Category = "NW|Npc") FString City;
};

/**
 * Places the map NPCs (Data/World/<city>_npcs.json, tools/make_npcs.py). Saved in the city map
 * (tools/ue/import_npcs.py) and runs on the server and on the clients.
 */
UCLASS()
class NWPORT_API ANWNpcField : public AActor
{
	GENERATED_BODY()

public:
	ANWNpcField();
	virtual void BeginPlay() override;

	/** File in Data/World. */
	UPROPERTY(EditAnywhere, Category = "NW|Npc") FString DataFile = TEXT("queda_eterna_npcs.json");
	/** Folder of the animation set (tools/sets/npc_queda_eterna.json) and of the meshes (NPCs/<SK_name>). */
	UPROPERTY(EditAnywhere, Category = "NW|Npc") FString AssetRoot = TEXT("/Game/NW/NpcQuedaEterna");

	UPROPERTY(Transient) TArray<TObjectPtr<ANWNpc>> Npcs;

	/** City area (center and radius around the NPCs and stations + CityMargin): whoever enters gets to know the city
	 *  (shared storage, docs/game/city.md "Storage"). */
	UPROPERTY(EditAnywhere, Category = "NW|Npc") float CityMargin = 4000.f;

private:
	FString City;
	FVector CityCenter = FVector::ZeroVector;
	float CityRadius = 0.f;
	FTimerHandle DiscoverTimer;
	void CheckDiscover();
};

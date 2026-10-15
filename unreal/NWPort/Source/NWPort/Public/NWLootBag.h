#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWItems.h"
#include "NWLootBag.generated.h"

class ANWCharacter;
class UNWInteractableComponent;
class UStaticMeshComponent;

/**
 * Bag on the ground (area 4; like the game's, catcontainer in loot bag mode): items dropped from the bag or that did not
 * fit in it (crafting, gathering, F10). Only the dropper sees and takes it: [E] opens the bag screen (inventory with the
 * "Dropped Items" window), item by item or Take All; what does not fit stays. Disappears after LifeSeconds (3 min, the
 * game's inv_discard_life_warning). Each player can have up to MaxGroundWeight kg dropped (full-bag overflow does not count toward the limit).
 * With nw-core, dropping and taking are recorded operations (NWOps; sack in ground_bags/ground_items with BagId): the sack comes
 * back to the same place when the player logs in (RestoreFor) and leaves the world when they log out (RemoveFor), until it expires.
 */
UCLASS()
class NWPORT_API ANWLootBag : public AActor
{
	GENERATED_BODY()

public:
	ANWLootBag();

	static constexpr float LifeSeconds = 180.f;   // "destroyed after 3 minutes" (inv_discard_life_warning)
	static constexpr float MaxGroundWeight = 1000.f;

	UPROPERTY(ReplicatedUsing = OnRep_Items) TArray<FNWItem> Items;
	UPROPERTY(VisibleAnywhere, Category = "NW") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "NW") TObjectPtr<UNWInteractableComponent> Interact;

	/** Sack id in nw-core (empty = only on this server, without nw-core). Server only. */
	FString BagId;

	/** Server, without nw-core: drops the items in a new bag at the character's feet. bLimit checks the player's ground
	 *  weight; on error returns null and the text key in OutError. With nw-core, dropping is NWOps::Overflow. */
	static ANWLootBag* Drop(ANWCharacter* Who, const TArray<FNWItem>& InItems, bool bLimit, FString* OutError = nullptr);
	/** Creates the sack actor (server). */
	static ANWLootBag* Spawn(AController* Owner, const FVector& At, const TArray<FNWItem>& InItems, const FString& InBagId, float Life);
	/** Point at the character's feet, a little ahead. */
	static FVector DropPoint(const ANWCharacter* Who);
	/** The player can drop more InItems without exceeding MaxGroundWeight on the ground. */
	static bool CheckLimit(const ANWCharacter* Who, const TArray<FNWItem>& InItems, FString* OutError);
	/** The player's sacks still alive in nw-core come back to the world (on this map). */
	static void RestoreFor(class ANWPlayerController* PC);
	/** Removes the player's sacks from the world (they stay in nw-core until they expire). */
	static void RemoveFor(const AController* Owner);
	/** Weight (kg) of the player's bags on the ground. */
	static float GroundWeight(const AController* Owner);
	static float ItemsWeight(const TArray<FNWItem>& InItems);
	float Weight() const { return ItemsWeight(Items); }
	/** Server: moves what fits into the character's bag (recorded operation); when empty, it disappears. */
	void TakeAll(ANWCharacter* Who) { Take(Who, INDEX_NONE, MAX_int32); }
	/** Server: moves Quantity of the item in slot Slot (INDEX_NONE = all), as much as fits; when empty, it disappears. */
	void Take(ANWCharacter* Who, int32 Slot, int32 Quantity);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION() void OnRep_Items();
	void UpdateLabel();
};

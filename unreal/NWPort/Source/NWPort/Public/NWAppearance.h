#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWAppearance.generated.h"

class USkeletalMeshComponent;
class UNWInventoryComponent;

/** What the other players see of a character's equipment: item ids per visible slot (replicated to everyone). */
USTRUCT()
struct FNWLook
{
	GENERATED_BODY()

	UPROPERTY() FName Head;
	UPROPERTY() FName Chest;
	UPROPERTY() FName Hands;
	UPROPERTY() FName Legs;
	UPROPERTY() FName Feet;
	UPROPERTY() FName Weapon1;
	UPROPERTY() FName Weapon2;
	UPROPERTY() FName Shield;
	UPROPERTY() bool bFromInventory = false;   // player: the look follows the equipment (creatures and NPCs: false)

	bool operator==(const FNWLook& O) const
	{
		return Head == O.Head && Chest == O.Chest && Hands == O.Hands && Legs == O.Legs && Feet == O.Feet
			&& Weapon1 == O.Weapon1 && Weapon2 == O.Weapon2 && Shield == O.Shield && bFromInventory == O.bFromInventory;
	}
};

/**
 * Equipped items change the player's look (docs/game/items.md "Look"). The character's own mesh (SK_player_male) stays
 * as the hidden leader that animates; the body is drawn by one skinned part per slot following it
 * (SetLeaderPoseComponent): the base parts (head, shirt, gloves, pants, boots) or the armor piece that covers them.
 * Weapons and shield in hand use the equipped item's mesh; the combat reach stays the weapon class's (ApplyWeapon
 * measures the blade before the mesh is swapped). Meshes from tools/make_appearances.py (Data/Items/appearances.json);
 * without that file or the imported meshes, the character keeps the old look.
 */
UCLASS(ClassGroup = (NW))
class NWPORT_API UNWAppearanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWAppearanceComponent();

	UPROPERTY(ReplicatedUsing = OnRep_Look) FNWLook Look;

	/** Server: the look from the inventory's equipment (Weapon 1/2, shield, armor). */
	void RefreshFrom(const UNWInventoryComponent* Inventory);
	/** Hand, back and shield meshes of the equipped items; called at the end of ANWCharacter::ApplyWeapon. */
	void ApplyWeaponLook();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

private:
	UFUNCTION() void OnRep_Look();
	void ApplyBody();
	USkeletalMeshComponent* Part(FName Name);
	UPROPERTY() TMap<FName, TObjectPtr<USkeletalMeshComponent>> Parts;
};

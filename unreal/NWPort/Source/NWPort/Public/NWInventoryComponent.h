#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWItems.h"
#include "NWInventoryComponent.generated.h"

/**
 * Player inventory: bag of BagSize slots (stacks from the catalog), equipment (weapons 1/2, shield, tools, armor,
 * jewelry, 4 quick consumables and 3 bags; NWEquip) and coins. The equipped weapons decide the character's weapon pair
 * (ANWCharacter::SyncWeaponsFromInventory). The server is authoritative; the owner receives the replicated copy and requests changes via RPC.
 * Saved in nw-core together with the character state (FNWCharacterRecord).
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWInventoryComponent();

	// Bag slots (single stack per slot): 500, the nw-core ceiling (MAX_BAG_SLOTS). When full, what arrives goes to the ground
	// (ANWLootBag); weight still causes overload.
	static constexpr int32 BagSize = 500;
	// Max weight without overload (the game's base inventory); above it, no running, dodging or jumping.
	static constexpr float BaseMaxWeight = 1000.f;
	// This character's max weight: the base one plus the equipped bags, or 10,000,000 with the dev bag (F10, not saved).
	UPROPERTY(Replicated) float MaxWeight = BaseMaxWeight;
	static constexpr float DevMaxWeight = 10000000.f;
	bool bDevBag = false;
	// The game's max health at level 60, to scale potion heals to ours (MaxHealth).
	static constexpr float GameMaxHealth = 7000.f;

	UPROPERTY(Replicated) TArray<FNWItem> Items;
	UPROPERTY(Replicated) int64 Coins = 0;
	// Potion cooldown (server time).
	UPROPERTY(Replicated) float PotionReadyAt = 0.f;

	const FNWItem* At(ENWItemContainer Container, int32 Slot) const;
	int32 CountOf(FName ItemId) const;
	/** Server: removes up to Quantity of the item from the bag stacks (refining, NWCrafting); returns how much it removed. */
	int32 Take(FName ItemId, int32 Quantity);
	/** Server: removes up to Quantity from the stack in bag slot BagSlot (storage); returns what came out (Quantity 0 = nothing). */
	FNWItem TakeAt(int32 BagSlot, int32 Quantity);
	/**
	 * Equipment slot for the item (the game's EquipItemToBestSlot): a quick slot with the same stack, an empty slot that
	 * takes it, or the first one that takes it (swap). INDEX_NONE if it is not equipment.
	 */
	int32 BestEquipSlot(const FNWItem& Item) const;
	/** Weapon set (WeaponSet.Id) of the weapon in Weapon1/Weapon2, or None. */
	FName WeaponSetAt(int32 WeaponSlot) const;
	/** Damage multiplier of the equipped weapon of the set by gear score; 1 without a weapon. */
	float DamageMultiplier(FName WeaponSet) const;
	float GetPotionCooldown() const;
	/** Total weight (bag + equipped), kg, from the catalog (the game's Weight). Server and owner have the items. */
	float TotalWeight() const;
	bool IsEncumbered() const { return TotalWeight() > MaxWeight; }

	// ---- server
	/** Puts into the bag (merges stacks, then free slots). Returns what did not fit. */
	int32 AddItem(const FNWItem& Item);
	void AddCoins(int64 Amount);
	/** Coins in cents as in the game (common.lua GetLocalizedCurrency: ÷100, 2 decimals, language separators). */
	static FString FormatCoins(int64 Cents);
	/** Delivers loot: puts it in the bag; what does not fit goes to the ground (loot bag) with a notice; shows the loot to the owner. */
	void Grant(const TArray<FNWItem>& Gained, int64 Coins);
	/** Grant without an operation (inside one, or without nw-core). */
	void GrantNow(const TArray<FNWItem>& Gained, int64 Coins);
	void Load(const TArray<FNWItem>& InItems, int64 InCoins);
	/** Marks the bag as changed (replicates at the end of the cycle). */
	void Changed();
	/** Server: max weight = base + equipped bags (or the dev bag). */
	void RefreshMaxWeight();
	void GiveStarterKit();
	/** T1 gathering tools in the empty slots (starter kit and dev menu). */
	void GiveTools();
	/** Equipped tool for the game's gathering type (Mining, Cutting, Chopping, Dressing). */
	const FNWItemDef* ToolFor(FName GatherType) const;

	// ---- owner -> server
	UFUNCTION(Server, Reliable) void ServerMove(ENWItemContainer FromContainer, int32 FromSlot, ENWItemContainer ToContainer, int32 ToSlot);
	UFUNCTION(Server, Reliable) void ServerUse(int32 BagSlot);
	/** Uses the consumable in quick slot Index (0..3, keys 3-6); refills it from the bag when the stack ends. */
	UFUNCTION(Server, Reliable) void ServerUseQuick(int32 Index);
	/** Destroys Quantity from the stack. */
	UFUNCTION(Server, Reliable) void ServerDestroy(int32 BagSlot, int32 Quantity);
	/** Drops Quantity from the stack on the ground, in a bag (ANWLootBag; up to 1000 kg on the ground per player). */
	UFUNCTION(Server, Reliable) void ServerDrop(int32 BagSlot, int32 Quantity);
	/**
	 * Sockets the gem or runeglass from bag slot GemBagSlot into the item (bag or equipped; docs/game/items.md "Slot and gems").
	 * Only into an empty slot: with a gem, refuses and warns (remove it first, paying).
	 */
	UFUNCTION(Server, Reliable) void ServerSocket(int32 GemBagSlot, ENWItemContainer Container, int32 Slot);
	/** Removes the gem from the item into the bag, paying the cost (FNWItemRules::UnsocketCost). */
	UFUNCTION(Server, Reliable) void ServerUnsocket(ENWItemContainer Container, int32 Slot);
	/** Loot received (for the on-screen notice). */
	UFUNCTION(Client, Reliable) void ClientLoot(const TArray<FNWItem>& Gained, int64 CoinsGained);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	FNWItem* Find(ENWItemContainer Container, int32 Slot);
	/** Server: returns the gem from the item to the bag, paying the cost; error key or empty (nothing changes on error). */
	FString Unsocket(ENWItemContainer Container, int32 Slot);
	void Warn(const TCHAR* Key) const;
	int32 FreeBagSlot() const;
	bool CanEquip(const FNWItem& Item, int32 EquipSlot) const;
	/** Server: uses one of the consumable stack (potion heal and cooldown); false if it could not. */
	bool UseStack(ENWItemContainer Container, int32 Slot);
	float ServerNow() const;
};

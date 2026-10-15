#pragma once

#include "CoreMinimal.h"
#include "NWItems.generated.h"

class FJsonObject;

/** Where the item is: bag or equipped. */
UENUM()
enum class ENWItemContainer : uint8
{
	Bag,
	Equip,
};

/** Equipment slots: weapons (sword, Hatchet, shield), gathering tools (F4), armor and jewelry. */
namespace NWEquip
{
	constexpr int32 Sword = 0;
	constexpr int32 Hatchet = 1;
	constexpr int32 Shield = 2;
	constexpr int32 Pick = 3;
	constexpr int32 Sickle = 4;
	constexpr int32 LoggingAxe = 5;
	constexpr int32 SkinningKnife = 6;
	// Armor and jewelry (docs/game/items.md); nw-core accepts slots 0..15.
	constexpr int32 Head = 7;
	constexpr int32 Chest = 8;
	constexpr int32 Hands = 9;
	constexpr int32 Legs = 10;
	constexpr int32 Feet = 11;
	constexpr int32 Amulet = 12;
	constexpr int32 Ring = 13;
	constexpr int32 Earring = 14;
	constexpr int32 Count = 15;
	constexpr int32 FirstTool = Pick;
	constexpr int32 LastTool = SkinningKnife;
	constexpr int32 FirstArmor = Head;
	inline bool IsTool(int32 Slot) { return Slot >= FirstTool && Slot <= LastTool; }

	/** Slot of the catalog set (Sword, Hatchet, Shield, Pick, ...) or INDEX_NONE. */
	NWPORT_API int32 SlotForSet(FName Set);
}

/** Character item (replicated to the owner and saved in nw-core). */
USTRUCT()
struct FNWItem
{
	GENERATED_BODY()

	UPROPERTY() FName ItemId;
	UPROPERTY() int32 Quantity = 0;
	UPROPERTY() ENWItemContainer Container = ENWItemContainer::Bag;
	UPROPERTY() int32 Slot = 0;
	UPROPERTY() int32 GearScore = 0;
	UPROPERTY() uint8 Rarity = 0;   // 0 common, 1 uncommon, 2 rare, 3 epic, 4 legendary
	// Crafted item (docs/game/items.md), stored in `props` in nw-core: attribute (index into FNWItemRules::Attributes; 255 =
	// none), quality 0..100 (< 0 = none), perks, gem, binding (0 none, 1 binds on equip, 2 bound) and the crafter
	// who signed it (quality >= assinatura_qualidade).
	UPROPERTY() uint8 Attr = 255;
	UPROPERTY() float Quality = -1.f;
	UPROPERTY() TArray<FName> Perks;
	UPROPERTY() FName Gem;
	UPROPERTY() uint8 Bind = 0;
	UPROPERTY() FString Signer;
	/** Serial number of the signed item (issued by nw-core; the database refuses the same number in two places). */
	UPROPERTY() FString Serial;
	/** Shared Storage tab (0..4); outside Storage, 0. Not part of SameKind. */
	UPROPERTY() uint8 Tab = 0;

	bool HasProps() const { return Attr != 255 || Quality >= 0.f || Perks.Num() > 0 || !Gem.IsNone() || Bind != 0 || !Signer.IsEmpty() || !Serial.IsEmpty(); }
	/** Same item for stacking or finding in Storage: same id, GS, rarity and props. */
	bool SameKind(const FNWItem& O) const
	{
		return ItemId == O.ItemId && GearScore == O.GearScore && Rarity == O.Rarity && Attr == O.Attr && Quality == O.Quality
			&& Perks == O.Perks && Gem == O.Gem && Bind == O.Bind && Signer == O.Signer && Serial == O.Serial;
	}
};

/** The item's `props` in nw-core (JSON object; empty = item without props). */
namespace NWItemProps
{
	NWPORT_API TSharedRef<FJsonObject> Write(const FNWItem& Item);
	NWPORT_API void Read(FNWItem& Item, const TSharedPtr<FJsonObject>& Props);
	/** Whole item (id, quantity, GS, rarity and props). */
	NWPORT_API TSharedRef<FJsonObject> WriteItem(const FNWItem& Item);
	NWPORT_API FNWItem ReadItem(const TSharedPtr<FJsonObject>& Json);
}

/** Catalog definition (tools/make_items.py). */
struct FNWItemDef
{
	FName Id;
	FString Type;       // weapon, consumable, resource
	FName Set;          // Sword, Hatchet, Shield, Pick, Sickle, LoggingAxe, SkinningKnife or empty
	int32 Tier = 0;
	int32 Stack = 1;
	FString Icon;       // lyshineui/images/icons/items/...dds
	int32 GearScoreMin = 0;
	int32 GearScoreMax = 0;
	float Damage = 0.f;
	float Heal = 0.f;
	float HealPercent = 0.f;
	float Cooldown = 0.f;
	FName Gather;       // tool: Mining, Cutting, Chopping, Dressing
	FName Armor;        // armor: Heavy, Medium, Light (jewelry: empty)
	float Efficiency = 1.f;
	float Weight = 0.f;  // kg (the game stores tenths)
	FString Name;
	FString Description;
	FString TypeName;   // The game's ItemTypeDisplayName ("Recurso", "Espada")
	FName UiClass;      // inventory section (UiItemClass without "UI_": Weapon, Tools, Smelting...)
	bool bRound = false; // round piece (resource, lore, dye, house item)
};

/** Item catalog and loot table, loaded once (Data/Items). */
class NWPORT_API FNWItemCatalog
{
public:
	static FNWItemCatalog& Get();
	const FNWItemDef* Find(FName Id) const { return Items.Find(Id); }
	const TMap<FName, FNWItemDef>& All() const { return Items; }

	struct FRoll
	{
		float Chance = 0.f;
		TArray<FName> Pick;
		int32 QtyMin = 1, QtyMax = 1;
	};
	struct FLootTable
	{
		int32 CoinsMin = 0, CoinsMax = 0;
		TArray<FRoll> Rolls;
	};
	const FLootTable* Loot(FName Table) const { return LootTables.Find(Table); }
	/**
	 * Rolls one of the game's tables (Data/Items/loottables.json): die 0..max; "and" gives every item with threshold <= die,
	 * "or" picks one among them. Entries with a table roll the subtable (quantity = number of rolls).
	 */
	void RollGame(FName Table, TArray<FNWItem>& OutItems, int32 RollBonus = 0) const;
	/** Rolls a table: items (with gear score and rarity) and coins. */
	void RollLoot(FName Table, TArray<FNWItem>& OutItems, int64& OutCoins) const;
	/** Gear score and rarity of a new item (rolled within the catalog range). */
	void RollStats(FNWItem& Item) const;
	/** Reloads the names in the current language (NWText::Lang). */
	void Reload();

private:
	void Load();
	TMap<FName, FNWItemDef> Items;
	TMap<FName, FLootTable> LootTables;

	struct FGameEntry
	{
		FName Item, Table;
		int32 QtyMin = 1, QtyMax = 1, Threshold = 0;
	};
	struct FGameTable
	{
		bool bAnd = true;
		int32 MaxRoll = 0;
		uint8 BonusMode = 0;   // RollBonusSetting: 0 ignores, 1 AddToRoll, 2 ClampMax
		TArray<FGameEntry> Entries;
	};
	TMap<FName, FGameTable> GameTables;
	void RollGameRec(FName Table, TArray<FNWItem>& OutItems, int32 Depth, int32 RollBonus) const;
	bool bLoaded = false;
};

/** Rarity color (the same color scale as the game). */
NWPORT_API FLinearColor NWRarityColor(uint8 Rarity);

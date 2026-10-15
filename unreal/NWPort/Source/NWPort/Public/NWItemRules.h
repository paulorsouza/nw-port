#pragma once

#include "CoreMinimal.h"
#include "NWItems.h"

/**
 * Item and trade skill rules of the port (docs/game/items.md, docs/game/crafting.md), in Data/Items/regras_itens.json
 * (tools/make_itens.py from tools/itens.json and tools/perks_decisoes.json).
 */
class NWPORT_API FNWItemRules
{
public:
	static const FNWItemRules& Get();

	struct FPerk
	{
		TMap<FString, FString> Name, Effect;   // per language; Effect with {v} in place of the value (port perks)
		float Min = 0.f, Max = 0.f;           // band (0 and 0: game perk, effect without band)
		FString Icon;
	};

	int32 CraftLevel = 50;
	float SignQuality = 80.f;
	float AttrPerGs = 0.05f;
	int32 PerksPerItem = 3;
	int32 PerksPerRing = 2;
	int32 AbilityPerksMax = 1;
	int32 GsMax = 800;
	float QualityMinCraftMax = 20.f;
	float QualityMinAllMax = 50.f;
	TArray<FName> Attributes;                  // strength, dexterity, intelligence, focus, constitution
	TMap<FName, FPerk> Perks;
	TMap<FString, TArray<FName>> Equipment;    // weapon, shield, armor, amulet, ring, earring, tool
	TMap<FName, TArray<FName>> Ability;        // weapon set (Sword, Hatchet...) -> ability perks
	TArray<FName> HealWeapons;                 // weapons that also roll Healing done

	/** Refine GS caps (docs/game/items.md "Attribute and gear score"): up to UpTo above the band min, success rises to Max and
	 *  failure drops to −Min. Without UpTo (MAX_int32), applies to the rest. */
	struct FGsStep { int32 UpTo = MAX_int32; int32 Min = 0; int32 Max = 0; };
	TArray<FGsStep> GsSteps;
	float GsSuccess = 51.f;                    // % success of refine GS
	/** Material paying for upgrades, chosen on screen: ingredient RecipeIndex of the recipe or the item Item. Empty Steps
	 *  uses GsSteps. */
	struct FImproveMaterial { int32 RecipeIndex = INDEX_NONE; FName Item; TArray<FGsStep> Steps; };
	TArray<FImproveMaterial> ImproveMaterials;
	int32 CostAttr = 2, CostGs = 2, CostUnbind = 2;
	float UnbindQualityPer = 10.f;             // unbind: +1 for every so many % of quality
	/** Market (docs/game/city.md "Trading Post"): open orders per character and what can go to the market. */
	int32 MarketMaxOrders = 50;
	TArray<FString> MarketPatterns;
	TArray<FName> MarketItems;
	/** The item can go to the market: crafted equipment, standard items (cut gem, runeglass) or the item list; never
	 *  bound. */
	bool CanTrade(const FNWItemDef& D, const FNWItem& Item) const;
	/** Shared storage (docs/game/city.md "Storage"): kg per known city and number of tabs. */
	float StoragePerCity = 1000.f;
	int32 StorageTabs = 5;

	/** Gem or runeglass (docs/game/items.md "Slot and gems"): class (armor, weapon), effects in %, raw gem for the removal cost. */
	struct FGem
	{
		FString Class;
		FName Raw;
		bool bGlass = false;
		TArray<TPair<FName, float>> Effects;
	};
	TMap<FName, FGem> Gems;
	TMap<FString, TArray<FString>> GemClasses;           // gem class -> equipment classes
	TMap<FName, TMap<FString, FString>> GemEffectText;   // effect -> text per language, with {v}

	/** Chest resistance per weight class (min, max) and fraction of each piece (docs/game/items.md "Armor and jewelry"). */
	TMap<FName, FVector2f> ChestResistance;
	TMap<FName, float> SlotResistance;
	/** Physical (= magic) resistance of the armor piece with the item quality; 0 outside armor. */
	float Resistance(const FNWItemDef& D, float Quality) const;

	/** Equipment with a slot: crafted weapon, shield or armor (jewelry and tools have none). */
	static bool HasSocket(const FNWItemDef& D, const FNWItem& Item);
	/** The gem (or runeglass) fits this item. */
	bool CanSocket(const FNWItemDef& D, const FNWItem& Item, FName GemId) const;
	/** Gem effect lines in the current language ("+2,5% de resistência física"). */
	TArray<FString> GemLines(FName GemId) const;
	/** Cost to remove the gem (provisional: 1 raw gem of the same type and tier). */
	FName UnsocketCost(FName GemId, int32& OutQty) const;

	/** Equipment class of the item ("arma", "escudo", "ferramenta"...; empty = takes no perk). */
	static FString EquipClass(const FNWItemDef& D);
	/** Perks that can drop on the item: equipment ones, the weapon's ability ones and Healing done on healing weapons. */
	TArray<FName> PerkPool(const FNWItemDef& D) const;
	/** Perk value with the item quality (one decimal place); 0 if the perk has no band. */
	float PerkValue(FName Perk, float Quality) const;
	/** Perk text in the current language, with the quality value. */
	FString PerkText(FName Perk, float Quality, bool bName) const;
	/** Attribute value by gear score (docs/game/items.md, provisional). */
	int32 AttrValue(int32 GearScore) const;
	/** Minimum roll quality by trade skill level (0 at level 0, QualityMinCraftMax at max). */
	float QualityMin(int32 SkillLevel, int32 MaxLevel) const;
	/**
	 * Builds the crafted item: GS rolled within the band (Min..Max), attribute, quality (from QualMin to 100), perks without
	 * repeats and bind on equip. Signer gets the crafter's name when the quality exceeds SignQuality.
	 */
	FNWItem Roll(FName ItemId, int32 RollMin, int32 RollMax, float QualMin, const FString& Crafter) const;

private:
	FNWItemRules();
};

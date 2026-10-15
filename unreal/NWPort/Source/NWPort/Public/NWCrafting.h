#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWItems.h"
#include "NWCrafting.generated.h"

class ANWCharacter;
class UNWInteractableComponent;
class UNWInventoryComponent;

/**
 * Refining at the city stations (area 4, docs/game/crafting.md "Refining"; data in
 * Data/Crafting/refining.json, tools/make_crafting.py). Game recipes (javelindata_crafting_refining): ingredients
 * by item or by category, minimum trade skill level, station tier, coin fee and trade skill XP. Instant: the game
 * has no refining time.
 */
/** Item upgrades at its trade skill station (docs/game/items.md): reroll the attribute, raise the GS, unbind. */
enum class ENWImprove : uint8 { Attribute, GearScore, Unbind, Num };

struct NWPORT_API FNWRecipe
{
	struct FIngredient { FName Id; int32 Qty = 0; bool bCategory = false; };
	FName Id;
	FName Skill;
	int32 Level = 0;
	FString Station;
	int32 Tier = 0;
	FName Out;
	int32 Qty = 1;
	TArray<FIngredient> Ingredients;
	int32 Fee = 0;
	int32 Xp = 0;
	FString Group;
	/** Screen tree: family (CraftingCategory) and group, with the game name per language. */
	FString Category;
	TMap<FString, FString> CategoryName, GroupName;
	/** Position in the tree (craftingsortinfo.lua CategorySortOrder/GroupSortOrder; 9999 = off the list). */
	int32 CategoryOrder = 9999, GroupOrder = 9999;
	/** Has FirstCraftAchievementId: the first craft gives extra XP. */
	bool bFirstCraft = false;
	/** Extra item chance: BonusItemChance and the adjustment by category reagent tier (Increase/Decrease). */
	float Bonus = 0.f;
	TArray<float> BonusInc, BonusDec;
	/** Craft weapon, shield or tool (item with GS, perks and quality; docs/game/crafting.md): item tier and base GS. */
	bool bGear = false;
	int32 ItemTier = 0;
	int32 GsBase = 0;
	/** Trade skill XP for Count crafts: CategoricalProgressionReward × Σ Qty of the ingredients × Count
	 *  (crafting.xp_oficio, crafting.xp_ingrediente_contagem; sistemas-005). bFirst: first craft of this recipe by the
	 *  character, + base × (mult − 1) (crafting.primeira_confeccao_xp_mult, crafting.primeira_confeccao_ativa). */
	int32 XpFor(int32 Count, bool bFirst = false) const;
};

class NWPORT_API FNWCrafting
{
public:
	static FNWCrafting& Get();

	struct FStation { FString Type; FName Skill; int32 Tier = 0; FVector Pos = FVector::ZeroVector; float Yaw = 0.f; TMap<FString, FString> Name; };
	const TArray<FStation>& Stations() const { return StationList; }
	const FNWRecipe* Find(FName Id) const;
	/** Station recipes (type and tier), in game order. */
	TArray<const FNWRecipe*> ForStation(const FString& Type, int32 Tier) const;
	const TArray<FName>& Category(FName Id) const;
	/** How much of the ingredient the character has (category: sum of its items). */
	int32 Have(const UNWInventoryComponent* Inv, const FNWRecipe::FIngredient& I) const;
	/** How many times refining is possible with what the character has (ingredients and coins). */
	int32 MaxCount(const UNWInventoryComponent* Inv, const FNWRecipe& R) const;
	/** Extra item chance (1 = 100%; can exceed 1). Chosen: item chosen per ingredient (empty = the recipe's ones). */
	float BonusChance(const FNWRecipe& R, int32 RollBonus, const TArray<FName>& Chosen) const;
	/** Category item the refining spends first (the one the character has most of). */
	FName ChosenFor(const UNWInventoryComponent* Inv, const FNWRecipe::FIngredient& I) const;
	/** Server: checks and refines Count times; returns the error text key or empty. */
	/** Tax: tax of the station's city (governance), to go into the operation save. */
	FString Refine(ANWCharacter* Who, const FString& StationType, FName RecipeId, int32 Count, struct FNWCityTaxNote* Tax = nullptr) const;
	/**
	 * Server (docs/game/crafting.md): weapon, shield or tool, instantly. Below the crafting level it is training (spends,
	 * gives XP, no item). From that level on it spends material and fee and rolls the item. With OutItem the item goes back
	 * to the caller (who requests the serial number and bags it); otherwise straight to the bag. Returns error key or empty.
	 */
	FString Craft(ANWCharacter* Who, const FString& StationType, FName RecipeId, FNWItem* OutItem = nullptr, struct FNWCityTaxNote* Tax = nullptr) const;
	/** Timed recipe of this station that crafts the item (null if the station does not make the item). */
	const FNWRecipe* RecipeFor(FName ItemId, const FString& StationType) const;
	/** Item GS band of the recipe for the character: catalog band + crafter bonus by level (cap gs_maximo). */
	FIntPoint GsRange(const ANWCharacter* Who, const FNWRecipe& R) const;
	/** Materials that pay for this item's upgrades, in FNWItemRules::ImproveMaterials order (None = the recipe lacks
	 *  that ingredient). */
	TArray<FName> ImproveMaterials(const FNWRecipe& R) const;
	/** Cost of the upgrade in material Mat (index into ImproveMaterials; provisional, tools/itens.json "_custos").
	 *  Qty 0 = cannot be done on this item. */
	FNWRecipe::FIngredient ImproveCost(const FNWRecipe& R, const FNWItem& Item, ENWImprove Kind, uint8 Mat = 0) const;
	/** Refine GS: how much it can go up (success) and down (failure), clamped to the crafting band, and the minimum
	 *  and maximum possible GS. */
	struct FGsOdds { FIntPoint Range = FIntPoint::ZeroValue; int32 UpMax = 0, DownMax = 0; };
	FGsOdds GsOdds(const ANWCharacter* Who, const FNWRecipe& R, const FNWItem& Item, uint8 Mat) const;
	/** Server: upgrade on the bag item (only its part changes) paid with material Mat; Before and After receive the item
	 *  before and after. Returns the error key or empty. */
	FString Improve(ANWCharacter* Who, const FString& StationType, int32 BagSlot, ENWImprove Kind, uint8 Mat,
		FNWItem* Before = nullptr, FNWItem* After = nullptr) const;

private:
	FNWCrafting();
	TArray<FStation> StationList;
	TArray<FNWRecipe> Recipes;
	const class ANWStation* NearStation(const ANWCharacter* Who, const FString& StationType) const;
	/** Spends the ingredients of Count recipes; returns what went out (to give back on cancel). */
	TArray<FNWItem> Spend(ANWCharacter* Who, const FNWRecipe& R, int32 Count) const;
	TMap<FName, TArray<FName>> Categories;
};

/** Interaction point of a refining station (the mesh is the converted city one). Created by ANWNpcField. */
UCLASS()
class NWPORT_API ANWStation : public AActor
{
	GENERATED_BODY()

public:
	ANWStation();
	void Setup(const FNWCrafting::FStation& Data);

	UPROPERTY(VisibleAnywhere, Category = "NW|Crafting") TObjectPtr<UNWInteractableComponent> Interact;
	UPROPERTY(VisibleAnywhere, Category = "NW|Crafting") FString Type;
	UPROPERTY(VisibleAnywhere, Category = "NW|Crafting") int32 Tier = 0;
	/** Map city (Data/World/<city>_npcs.json "city"): storage location "settlement:<city>". */
	UPROPERTY(VisibleAnywhere, Category = "NW|Crafting") FString City;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWProgressionComponent.generated.h"

/** The game's attributes, in screen order (FOR, DES, INT, FOC, CON). */
UENUM()
enum class ENWAttribute : uint8
{
	Strength,
	Dexterity,
	Intelligence,
	Focus,
	Constitution,
	Num UMETA(Hidden),
};

/** The game's gathering trade skills (CategoricalProgressionId). */
namespace NWTradeskill
{
	constexpr int32 Num = 14;
	/** Gathering trade skills (0..3); the refining ones come after (4..8). */
	constexpr int32 NumGathering = 4;
	/** Mining, Logging, Harvesting, Skinning -> 0..3; Smelting, Woodworking, Leatherworking, Weaving, Stonecutting
	 *  -> 4..8 (refining, area 4); Weaponsmithing, Engineering, Arcana -> 9..11 (crafting, docs/game/crafting.md); Jewelcrafting -> 12, Armoring -> 13; other ->
	 *  INDEX_NONE. */
	NWPORT_API int32 Index(FName Tradeskill);
	NWPORT_API FName Name(int32 Index);
	/** Key of the game's text (ui_mining...) and icon (icons/tradeskills/icon_tradeskill_<tradeskill>). */
	NWPORT_API FString TextKey(int32 Index);
	NWPORT_API FString Icon(int32 Index);
}

/**
 * Character level, XP and attributes, with the game's tables (Data/Progression/progression.json,
 * tools/make_progression.py): total XP per level (javelindata_xpamountsbylevel), attribute points gained at each
 * level and the effect of each attribute value (javelindata_attribute*). Each attribute starts at 5.
 * The server is authoritative and saves to nw-core (progression column). Character XP comes from the level tutorials
 * (whole levels per quest) and, after them, from kills; trade skill XP comes from gathering and refining.
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWProgressionComponent();

	UPROPERTY(Replicated) int32 Level = 1;
	/** Total accumulated XP. */
	UPROPERTY(Replicated) int64 Xp = 0;
	/** Points spent per attribute (on top of the base 5). */
	UPROPERTY(Replicated) TArray<int32> Spent;
	/** Total XP of each gathering trade skill (NWTradeskill). */
	UPROPERTY(Replicated) TArray<int32> TradeXp;
	/**
	 * Progression came from nw-core (saved character): the gatherables' minimum trade skill level applies
	 * (RequiredTradeskillLevel). In the editor's Play, without backend, it is free for testing.
	 */
	UPROPERTY(Replicated) bool bFromBackend = false;
	/** Trade skill level missing to gather (0 = can). */
	int32 MissingTradeLevel(FName Tradeskill, int32 Required) const;

	/** Trade skill level from the game's table (javelindata_tradeskill<tradeskill>, accumulated InfluenceCost). */
	int32 TradeLevel(int32 Skill) const;
	int32 TradeMaxLevel(int32 Skill) const;
	/** 0..1 of the trade skill level toward the next. */
	float TradeProgress(int32 Skill) const;
	/** Gathering speed bonus of the level (GatheringEfficiency: 0.05 per level at the start). */
	float TradeEfficiency(int32 Skill) const;
	/** Loot die bonus of the level (RollBonus: +10 per level). */
	int32 TradeRollBonus(int32 Skill) const;
	/** Server: gathering trade skill XP (CategoricalProgressionReward of the gatherable's event). */
	void AddTradeXp(FName Tradeskill, int32 Amount);
	/** XP missing to the next trade skill level (dev menu). */
	int32 TradeXpToNext(int32 Skill) const;
	UFUNCTION(Client, Reliable) void ClientTradeXpGained(uint8 Skill, int32 Amount, int32 NewLevel);

	int32 MaxLevel() const;
	/** Total XP to reach the level (1 = 0). */
	static int64 XpForLevel(int32 InLevel);
	/** 0..1 from the current level toward the next (1 at max level). */
	float LevelProgress() const;
	int32 Attribute(ENWAttribute A) const;
	/** Points gained up to the current level minus the spent ones. */
	int32 UnspentPoints() const;
	/** Extra health from constitution (Health column of the game's table). */
	float ConstitutionHealth() const;

	// ---- server
	/** State saved in nw-core (FNWCharacterRecord::Progression). */
	void Load(const TSharedPtr<class FJsonObject>& Json);
	TSharedRef<class FJsonObject> Save() const;
	/** Adds XP and levels up through the table; notifies the owner. */
	void AddXp(int64 Amount);

	// ---- owner -> server
	UFUNCTION(Server, Reliable) void ServerSpendPoint(ENWAttribute A);
	/** Several points at once (Attributes tab: Confirm), in the order FOR, DES, INT, FOC, CON. */
	UFUNCTION(Server, Reliable) void ServerSpendPoints(const TArray<int32>& Points);

	UFUNCTION(Client, Reliable) void ClientXpGained(int64 Amount, int32 NewLevel);

	// ---- level tutorials (area 4, tools/tutorials.json; docs/game/city.md)
	/** Progress of each quest (index into tools/tutorials.json); -1 = completed. Only the owner receives it. */
	UPROPERTY(Replicated) TArray<int32> TutorialCount;
	/** Recipes already crafted (first-craft bonus; saved in progression.crafted). Only the owner receives it. */
	UPROPERTY(Replicated) TArray<FName> Crafted;
	bool HasCrafted(FName Recipe) const { return Crafted.Contains(Recipe); }
	void MarkCrafted(FName Recipe);
	/** Known cities (city id, e.g. queda_eterna; saved in progression.cities): each one adds capacity to the
	 *  shared Storage. Only the owner receives it. */
	UPROPERTY(Replicated) TArray<FString> Cities;
	/** Server: marks the city as known; true if it is new. */
	bool DiscoverCity(const FString& City);
	/** Weekly shop purchases (weekly limit; saved in progression.shop). Period = FNWNpcRewards::Period. */
	UPROPERTY(Replicated) FString ShopPeriod;
	UPROPERTY(Replicated) TArray<FName> ShopBoughtIds;
	UPROPERTY(Replicated) TArray<int32> ShopBoughtQty;
	int32 ShopBought(FName Item, const FString& Period) const;
	/** Crafter GS bonus (min, max) at the current trade skill level for an item of the tier (2..5). */
	FIntPoint TradeGsBonus(int32 Skill, int32 ItemTier) const;
	void AddShopBought(FName Item, int32 Quantity, const FString& Period);
	int32 TutorialNum() const;
	FString TutorialName(int32 Index) const;
	int32 TutorialGoal(int32 Index) const;
	bool TutorialsDone() const;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void AdvanceTutorials(const FString& Kind, const FString& Target, int32 Amount);
	void HandleKill(AActor* Killer, AActor* Victim);
	void HandleTradeXp(class ANWCharacter* Who, FName Tradeskill, int32 Amount);
	void HandleRefine(class ANWCharacter* Who, const FString& Station, FName Recipe, int32 Count);
	FDelegateHandle KillHandle, TradeHandle, RefineHandle;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

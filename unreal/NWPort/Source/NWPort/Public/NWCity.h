#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "NWCity.generated.h"

/**
 * City governance (docs/game/economy.md; nw-core: backend/src/routes/city.rs). One city per region; nw-core
 * stores everything and the game server keeps a short copy per region (prices, tax, station level, buffs)
 * for crafting. Rules in Data/World/governanca.json, bands in precos_cidade.json, tax reference in
 * precos_referencia.json (copies from tools/, make_crafting.py).
 */
USTRUCT()
struct NWPORT_API FNWCityPrice
{
	GENERATED_BODY()
	UPROPERTY() FName Item;
	UPROPERTY() int64 Price = 0;
	UPROPERTY() int64 Min = 0;
	UPROPERTY() int64 Max = 0;
	UPROPERTY() int64 Padrao = 0;
	UPROPERTY() int32 Used = 0;    // how much of the week's limit the player has used
	UPROPERTY() int32 Limit = 0;
};

USTRUCT()
struct NWPORT_API FNWCityStation
{
	GENERATED_BODY()
	UPROPERTY() FString Station;
	UPROPERTY() int32 Tier = 2;          // station tier (2 to 5)
	UPROPERTY() int32 Level = 0;         // level after tier 5
	UPROPERTY() int32 Reached = 0;
	UPROPERTY() int32 Crafts = 0;
	UPROPERTY() FString Work;            // work: "tier", "level", "repair" or empty
	UPROPERTY() int32 WorkTarget = 0;
	UPROPERTY() float WorkEndsIn = 0.f;  // seconds
};

USTRUCT()
struct NWPORT_API FNWCityWeapon
{
	GENERATED_BODY()
	UPROPERTY() FString Weapon;          // ballista, repeater, explosive, firebarrel, hornofresilience
	UPROPERTY() int32 Tier = 1;
	UPROPERTY() int32 WorkTarget = 0;    // 0 = no work
	UPROPERTY() float WorkEndsIn = 0.f;
};

USTRUCT()
struct NWPORT_API FNWCityStock
{
	GENERATED_BODY()
	UPROPERTY() FName Item;
	UPROPERTY() int64 Quantity = 0;
};

USTRUCT()
struct NWPORT_API FNWCityBuff
{
	GENERATED_BODY()
	UPROPERTY() FString Station;
	UPROPERTY() FString Buff;
};

USTRUCT()
struct NWPORT_API FNWCityState
{
	GENERATED_BODY()
	UPROPERTY() bool bValid = false;
	UPROPERTY() FString Region;
	UPROPERTY() FString Name;
	UPROPERTY() bool bHasOwner = false;
	UPROPERTY() bool bMine = false;          // the recipient is the governor
	UPROPERTY() FString OwnerName;
	UPROPERTY() int64 Price = 0;             // starting price (fixed)
	// City sale auction (24 h): by the owner (with their starting bid) or forced (dark, starting price).
	UPROPERTY() bool bAuction = false;
	UPROPERTY() bool bAuctionForced = false;
	UPROPERTY() int64 AuctionStart = 0;
	UPROPERTY() int64 AuctionBid = -1;       // -1 = no bid
	UPROPERTY() FString AuctionBidder;
	UPROPERTY() int64 AuctionMinBid = 0;
	UPROPERTY() float AuctionEndsIn = 0.f;   // seconds
	UPROPERTY() bool bAuctionMine = false;   // the highest bid belongs to the recipient
	UPROPERTY() float SaleGraceDays = 0.f;   // days left before it can be sold (governor)
	UPROPERTY() int64 Treasury = 0;
	UPROPERTY() float Tax = 0.f;
	UPROPERTY() float TaxMin = 0.f;
	UPROPERTY() float TaxMax = 0.f;
	UPROPERTY() TArray<FNWCityPrice> Buy;     // raw material the city buys
	UPROPERTY() TArray<FNWCityPrice> Sell;    // list the city sells
	UPROPERTY() TArray<FNWCityStation> Stations;
	UPROPERTY() TArray<FNWCityStock> Stock;
	UPROPERTY() TArray<FNWCityBuff> Buffs;
	UPROPERTY() TArray<FNWCityWeapon> Weapons;
	UPROPERTY() int32 Defense = 0;       // sum of the weapon tiers
	UPROPERTY() float NextAttackIn = -1.f;   // seconds; -1 = no attack scheduled
	UPROPERTY() bool bHasLastAttack = false;
	UPROPERTY() bool bLastAttackWon = false;
	UPROPERTY() float LastAttackStrength = 0.f;
	UPROPERTY() TArray<FString> LastAttackLost;
	UPROPERTY() FString Week;
	UPROPERTY() int64 WeekRawUnits = 0;
	UPROPERTY() int64 WeekListUnits = 0;
	UPROPERTY() int32 WeekBuffs = 0;

	const FNWCityStation* StationOf(const FString& Station) const;
	bool HasBuff(const FString& Station, const FString& Buff) const;
	int64 StockOf(FName Item) const;
};

/** Crafting tax of an operation (goes in the character save, `city_tax`). */
struct NWPORT_API FNWCityTaxNote
{
	FString Region;
	FString Station;
	int64 Amount = 0;
	bool IsSet() const { return !Region.IsEmpty(); }
	TSharedPtr<FJsonObject> Json() const;
};

namespace NWCity
{
	/** One upgrade (tier, level or weapon): basket of raw material and coins. */
	struct FUp { int32 To = 0; TArray<TPair<FName, int64>> Materials; int64 Coins = 0; };
	struct FRules
	{
		TMap<FString, FString> CityToRegion;           // "queda_eterna" -> "1", "guardaventos" -> "2"
		TMap<FString, FString> CityTerritory;          // city -> territory name key ("everfall", "windsward")
		TArray<TPair<FString, FString>> CityMap;       // city -> its map ("guardaventos" -> "L_SouthWindsward"), file order
		int32 TierInitial = 2;
		TArray<FUp> TierUps;                           // To = new tier
		TMap<int32, TPair<int32, float>> TierEffect;   // tier -> (GS, minimum quality)
		int32 LevelMax = 0;
		TArray<FUp> LevelUps;                          // To = new level
		int32 GsPerLevel = 0;
		float QualityPerLevel = 0.f;
		TArray<FString> Weapons;
		TArray<FUp> WeaponUps;
		int32 WorkHours = 24;
		float RepairFraction = 0.3f;
		TMap<FString, int64> BuffCost;
		TMap<FString, float> BuffValue;
		float TaxDefault = 0.05f;
		int32 GraceDays = 7;                           // grace period before putting the city up for auction
		int64 CoinsMax = 0;                            // coin cap per character (cents)
		int64 ItemPriceMax = 0;                        // maximum unit price on the market (cents)
		int64 TenebMaterial = 0;                       // minimum weekly targets (below = dark)
		int64 TenebList = 0;
		TMap<FName, int64> Reference;                  // reference price (cents) for the tax
	};
	NWPORT_API const FRules& Rules();
	/** City region of a station (ANWStation::City); empty = city without governance. */
	NWPORT_API FString RegionOf(const FString& City);
	/** Map path (/Game/NW/Maps/...) of the first town other than City (town teleporter); empty if none. */
	NWPORT_API FString OtherTownMap(const FString& City);
	/** Territory name of the town other than City (teleporter card), in the current language. */
	NWPORT_API FString OtherTownName(const FString& City);
	/** Reads the nw-core response (city GET/POST) for ViewerId. */
	NWPORT_API bool Parse(const TSharedPtr<FJsonObject>& Json, const FString& ViewerId, FNWCityState& Out);
	/** Server copy per region (filled on every nw-core read). */
	NWPORT_API const FNWCityState* Cached(const FString& Region);
	NWPORT_API void Store(const FNWCityState& State);
	/** Reads again from nw-core if the copy is older than MaxAge seconds (does not wait; crafting uses the copy). */
	NWPORT_API void RefreshIfOld(UWorld* World, const FString& Region, double MaxAge = 30.0);
	/** City station bonus on crafting: GS and minimum quality points (tier + levels). */
	NWPORT_API void StationBonus(const FNWCityState* City, const FString& Station, int32& OutGs, float& OutQuality);
	/** Next upgrade of a station (tier, level or repair) or of a weapon; null = maxed. */
	NWPORT_API const FUp* NextStationUp(const FNWCityStation& S, bool& bOutRepair);
	NWPORT_API const FUp* NextWeaponUp(const FNWCityWeapon& W);
	/** Tax (cents) for spending these ingredients at a station in the region. */
	NWPORT_API int64 TaxFor(const FString& Region, const TMap<FName, int32>& Spent);
}

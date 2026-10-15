#pragma once

#include "CoreMinimal.h"

/**
 * Port shops (area 4; Data/World/shops.json, from tools/shops.json). The game has no vendor with prices in coins:
 * list, price (cents) and weekly limit per character are the maintainer's decision (docs/game/city.md "Shops and merchants").
 * The purchase is server-side (UNWInteractorComponent::ServerShopBuy); the limit resets with the gifts (FNWNpcRewards::Period).
 */
struct NWPORT_API FNWShopItem
{
	FName Id;
	int64 Price = 0;
	/** 0 = no limit. */
	int32 WeeklyLimit = 0;
};

struct NWPORT_API FNWShop
{
	FString Id;
	FString City;
	/** VariantID of the NPC that sells (ANWNpc::Variant). */
	FString Npc;
	TArray<FNWShopItem> Items;
	const FNWShopItem* Find(FName Item) const { return Items.FindByPredicate([Item](const FNWShopItem& I) { return I.Id == Item; }); }
};

class NWPORT_API FNWShops
{
public:
	static FNWShops& Get();
	const FNWShop* Find(const FString& Id) const { return Shops.FindByPredicate([&Id](const FNWShop& S) { return S.Id == Id; }); }
	const FNWShop* ForNpc(const FString& Variant) const { return Shops.FindByPredicate([&Variant](const FNWShop& S) { return S.Npc == Variant; }); }

private:
	void Load();
	TArray<FNWShop> Shops;
};

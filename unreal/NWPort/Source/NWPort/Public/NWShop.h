#pragma once

#include "CoreMinimal.h"

/**
 * Lojas do port (esteira 4; Data/World/shops.json, de tools/shops.json). O jogo não tem vendedor com preço em moedas:
 * lista, preço (centavos) e limite semanal por personagem são decisão do dono (docs/game/city.md "Lojas").
 * A compra é do servidor (UNWInteractorComponent::ServerShopBuy); o limite renova com os brindes (FNWNpcRewards::Period).
 */
struct NWPORT_API FNWShopItem
{
	FName Id;
	int64 Price = 0;
	/** 0 = sem limite. */
	int32 WeeklyLimit = 0;
};

struct NWPORT_API FNWShop
{
	FString Id;
	FString City;
	/** VariantID do NPC que vende (ANWNpc::Variant). */
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

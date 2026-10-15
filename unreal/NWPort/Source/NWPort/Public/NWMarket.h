#pragma once

#include "CoreMinimal.h"
#include "NWItems.h"
#include "NWMarket.generated.h"

/**
 * Market (Trading Post, docs/game/city.md §5; maintainer's rules 2026-10-14): a single market, no fees, up to
 * FNWItemRules::MarketMaxOrders open orders per character, only items that can go to the market (FNWItemRules::CanTrade).
 * The server reads the orders from nw-core (short cache) and sends the rows to the owner (ClientMarketOrders); buying, selling
 * and creating an order are written operations (NWOps): coins and items only change after nw-core writes. What is bought goes
 * to the shared Storage; what is sold through a sell order becomes credit, claimed when opening the Trading Post.
 */
USTRUCT()
struct NWPORT_API FNWMarketRow
{
	GENERATED_BODY()

	UPROPERTY() FString Id;
	UPROPERTY() bool bSell = true;
	UPROPERTY() bool bMine = false;
	UPROPERTY() FString Seller;
	UPROPERTY() FNWItem Item;          // Quantity = what is left to trade
	UPROPERTY() int64 UnitPrice = 0;
	UPROPERTY() float ExpiresIn = 0.f;  // seconds
};

namespace NWMarket
{
	/** Accepted durations (days), as in the game. */
	inline constexpr int32 Durations[] = {1, 3, 7, 14};
	inline bool ValidDays(int32 Days) { return Days == 1 || Days == 3 || Days == 7 || Days == 14; }
}

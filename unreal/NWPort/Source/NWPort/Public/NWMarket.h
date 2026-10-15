#pragma once

#include "CoreMinimal.h"
#include "NWItems.h"
#include "NWMarket.generated.h"

/**
 * Mercado (posto de trocas, docs/game/city.md §5; regras do dono 2026-10-14): um mercado só, sem taxas, até
 * FNWItemRules::MarketMaxOrders ordens abertas por personagem, só itens que podem ir ao mercado (FNWItemRules::CanTrade).
 * O servidor lê as ordens do nw-core (cache curto) e manda ao dono as linhas (ClientMarketOrders); comprar, vender e criar
 * ordem são operações gravadas (NWOps): moedas e itens só mudam depois do nw-core gravar. O que se compra vai para o
 * armazém compartilhado; o que se vende numa ordem de venda vira crédito, resgatado ao abrir o posto.
 */
USTRUCT()
struct NWPORT_API FNWMarketRow
{
	GENERATED_BODY()

	UPROPERTY() FString Id;
	UPROPERTY() bool bSell = true;
	UPROPERTY() bool bMine = false;
	UPROPERTY() FString Seller;
	UPROPERTY() FNWItem Item;          // Quantity = o que falta negociar
	UPROPERTY() int64 UnitPrice = 0;
	UPROPERTY() float ExpiresIn = 0.f;  // segundos
};

namespace NWMarket
{
	/** Durações aceitas (dias), como o jogo. */
	inline constexpr int32 Durations[] = {1, 3, 7, 14};
	inline bool ValidDays(int32 Days) { return Days == 1 || Days == 3 || Days == 7 || Days == 14; }
}

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "NWCity.generated.h"

/**
 * Governança das cidades (docs/plano-governanca.md; nw-core: backend/src/routes/city.rs). Uma cidade por região; o
 * nw-core guarda tudo e o servidor de jogo mantém uma cópia curta por região (preços, imposto, nível das mesas, buffs)
 * para a fabricação. Regras em Data/World/governanca.json, faixas em precos_cidade.json, referência do imposto em
 * precos_referencia.json (cópias de tools/, make_crafting.py).
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
	UPROPERTY() int32 Used = 0;    // quanto o jogador já usou do limite da semana
	UPROPERTY() int32 Limit = 0;
};

USTRUCT()
struct NWPORT_API FNWCityStation
{
	GENERATED_BODY()
	UPROPERTY() FString Station;
	UPROPERTY() int32 Level = 0;
	UPROPERTY() int32 Reached = 0;
	UPROPERTY() int32 Crafts = 0;
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
	UPROPERTY() bool bMine = false;          // quem recebe é o governador
	UPROPERTY() FString OwnerName;
	UPROPERTY() int64 Price = 0;             // preço inicial (fixo)
	// Leilão da venda da cidade (24 h): do dono (lance inicial dele) ou obrigatório (tenebroso, preço inicial).
	UPROPERTY() bool bAuction = false;
	UPROPERTY() bool bAuctionForced = false;
	UPROPERTY() int64 AuctionStart = 0;
	UPROPERTY() int64 AuctionBid = -1;       // -1 = sem lance
	UPROPERTY() FString AuctionBidder;
	UPROPERTY() int64 AuctionMinBid = 0;
	UPROPERTY() float AuctionEndsIn = 0.f;   // segundos
	UPROPERTY() bool bAuctionMine = false;   // o maior lance é de quem recebe
	UPROPERTY() float SaleGraceDays = 0.f;   // dias que faltam para poder vender (governador)
	UPROPERTY() int64 Treasury = 0;
	UPROPERTY() float Tax = 0.f;
	UPROPERTY() float TaxMin = 0.f;
	UPROPERTY() float TaxMax = 0.f;
	UPROPERTY() TArray<FNWCityPrice> Buy;     // matéria-prima que a cidade compra
	UPROPERTY() TArray<FNWCityPrice> Sell;    // lista que a cidade vende
	UPROPERTY() TArray<FNWCityStation> Stations;
	UPROPERTY() TArray<FNWCityStock> Stock;
	UPROPERTY() TArray<FNWCityBuff> Buffs;
	UPROPERTY() int32 Weapons = 0;
	UPROPERTY() float NextAttackIn = -1.f;   // segundos; -1 = sem ataque marcado
	UPROPERTY() bool bHasLastAttack = false;
	UPROPERTY() bool bLastAttackWon = false;
	UPROPERTY() float LastAttackStrength = 0.f;
	UPROPERTY() TArray<FString> LastAttackLost;
	UPROPERTY() FString Week;
	UPROPERTY() int64 WeekRawUnits = 0;
	UPROPERTY() int64 WeekListUnits = 0;
	UPROPERTY() int32 WeekBuffs = 0;

	int32 StationLevel(const FString& Station) const;
	bool HasBuff(const FString& Station, const FString& Buff) const;
	int64 StockOf(FName Item) const;
};

/** Imposto de fabricação de uma operação (vai no salvamento do personagem, `city_tax`). */
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
	struct FStep { int64 Material = 0; int64 Coins = 0; };
	struct FRules
	{
		TMap<FString, FString> CityToRegion;           // "queda_eterna" -> "1"
		TMap<FString, TArray<FName>> StationMaterials; // mesa -> material do nível 1, 2, 3
		TArray<FStep> Steps;
		float RepairFraction = 0.3f;
		TMap<FString, int64> BuffCost;
		TMap<FString, float> BuffValue;
		int64 WeaponCost = 0;
		int32 GsPerLevel = 0;
		int32 QualityPerLevel = 0;
		float TaxDefault = 0.05f;
		int64 TenebMaterial = 0;                       // metas mínimas da semana (abaixo = tenebroso)
		int64 TenebList = 0;
		TMap<FName, int64> Reference;                  // preço de referência (centavos) para o imposto
	};
	NWPORT_API const FRules& Rules();
	/** Região da cidade de uma estação (ANWStation::City); vazio = cidade sem governança. */
	NWPORT_API FString RegionOf(const FString& City);
	/** Lê a resposta do nw-core (GET/POST da cidade) para quem ViewerId. */
	NWPORT_API bool Parse(const TSharedPtr<FJsonObject>& Json, const FString& ViewerId, FNWCityState& Out);
	/** Cópia do servidor por região (preenchida a cada leitura do nw-core). */
	NWPORT_API const FNWCityState* Cached(const FString& Region);
	NWPORT_API void Store(const FNWCityState& State);
	/** Lê de novo do nw-core se a cópia tem mais de MaxAge segundos (não espera; a fabricação usa a cópia). */
	NWPORT_API void RefreshIfOld(UWorld* World, const FString& Region, double MaxAge = 30.0);
	/** Imposto (centavos) de gastar estes ingredientes numa mesa da região. */
	NWPORT_API int64 TaxFor(const FString& Region, const TMap<FName, int32>& Spent);
}

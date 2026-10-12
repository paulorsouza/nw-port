#pragma once

#include "CoreMinimal.h"
#include "NWItems.generated.h"

/** Onde o item está: bolsa ou equipado. */
UENUM()
enum class ENWItemContainer : uint8
{
	Bag,
	Equip,
};

/** Espaços de equipamento: armas (espada, machadinha, escudo) e ferramentas de coleta (F4). */
namespace NWEquip
{
	constexpr int32 Sword = 0;
	constexpr int32 Hatchet = 1;
	constexpr int32 Shield = 2;
	constexpr int32 Pick = 3;
	constexpr int32 Sickle = 4;
	constexpr int32 LoggingAxe = 5;
	constexpr int32 SkinningKnife = 6;
	constexpr int32 Count = 7;
	constexpr int32 FirstTool = Pick;

	/** Espaço do conjunto do catálogo (Sword, Hatchet, Shield, Pick, ...) ou INDEX_NONE. */
	NWPORT_API int32 SlotForSet(FName Set);
}

/** Item do personagem (replicado para o dono e salvo no nw-core). */
USTRUCT()
struct FNWItem
{
	GENERATED_BODY()

	UPROPERTY() FName ItemId;
	UPROPERTY() int32 Quantity = 0;
	UPROPERTY() ENWItemContainer Container = ENWItemContainer::Bag;
	UPROPERTY() int32 Slot = 0;
	UPROPERTY() int32 GearScore = 0;
	UPROPERTY() uint8 Rarity = 0;   // 0 comum, 1 incomum, 2 raro, 3 épico, 4 lendário
};

/** Definição do catálogo (tools/make_items.py). */
struct FNWItemDef
{
	FName Id;
	FString Type;       // weapon, consumable, resource
	FName Set;          // Sword, Hatchet, Shield, Pick, Sickle, LoggingAxe, SkinningKnife ou vazio
	int32 Tier = 0;
	int32 Stack = 1;
	FString Icon;       // lyshineui/images/icons/items/...dds
	int32 GearScoreMin = 0;
	int32 GearScoreMax = 0;
	float Damage = 0.f;
	float Heal = 0.f;
	float HealPercent = 0.f;
	float Cooldown = 0.f;
	FName Gather;       // ferramenta: Mining, Cutting, Chopping, Dressing
	float Efficiency = 1.f;
	float Weight = 0.f;  // kg (o jogo guarda em décimos)
	FString Name;
	FString Description;
	FString TypeName;   // ItemTypeDisplayName do jogo ("Recurso", "Espada")
};

/** Catálogo de itens e tabela de saque, carregados uma vez (Data/Items). */
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
	 * Sorteia uma tabela do jogo (Data/Items/loottables.json): dado 0..max; "and" dá todo item com limiar <= dado,
	 * "or" sorteia um entre eles. Entradas com tabela rolam a subtabela (quantidade = número de rolagens).
	 */
	void RollGame(FName Table, TArray<FNWItem>& OutItems) const;
	/** Sorteia uma tabela: itens (com gear score e raridade) e moedas. */
	void RollLoot(FName Table, TArray<FNWItem>& OutItems, int64& OutCoins) const;
	/** Gear score e raridade de um item novo (sorteados na faixa do catálogo). */
	void RollStats(FNWItem& Item) const;
	/** Recarrega os nomes no idioma atual (NWText::Lang). */
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
		TArray<FGameEntry> Entries;
	};
	TMap<FName, FGameTable> GameTables;
	void RollGameRec(FName Table, TArray<FNWItem>& OutItems, int32 Depth) const;
	bool bLoaded = false;
};

/** Cor da raridade (a mesma escala de cores do jogo). */
NWPORT_API FLinearColor NWRarityColor(uint8 Rarity);

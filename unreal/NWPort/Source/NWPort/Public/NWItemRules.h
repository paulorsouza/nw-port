#pragma once

#include "CoreMinimal.h"
#include "NWItems.h"

/**
 * Regras de item e de ofício do port (docs/itens.md, docs/oficio.md), em Data/Items/regras_itens.json
 * (tools/make_itens.py a partir de tools/itens.json e tools/perks_decisoes.json).
 */
class NWPORT_API FNWItemRules
{
public:
	static const FNWItemRules& Get();

	struct FPerk
	{
		TMap<FString, FString> Name, Effect;   // por idioma; Effect com {v} no lugar do valor (perks do port)
		float Min = 0.f, Max = 0.f;           // faixa (0 e 0: perk do jogo, efeito sem faixa)
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
	TMap<FString, TArray<FName>> Equipment;    // arma, escudo, armadura, amuleto, anel, brinco, ferramenta
	TMap<FName, TArray<FName>> Ability;        // conjunto da arma (Sword, Hatchet...) -> perks de habilidade
	TArray<FName> HealWeapons;                 // armas que também rolam Cura causada

	/** Tetos do refinar GS (docs/itens.md "Gear score"): até UpTo acima do mínimo da faixa, o sucesso sobe até Max e a
	 *  falha desce até −Min. Sem UpTo (MAX_int32), vale para o resto. */
	struct FGsStep { int32 UpTo = MAX_int32; int32 Min = 0; int32 Max = 0; };
	TArray<FGsStep> GsSteps;
	float GsSuccess = 51.f;                    // % de sucesso do refinar GS
	/** Material que paga as melhorias, à escolha na tela: ingrediente RecipeIndex da receita ou o item Item. Steps vazio
	 *  usa GsSteps. */
	struct FImproveMaterial { int32 RecipeIndex = INDEX_NONE; FName Item; TArray<FGsStep> Steps; };
	TArray<FImproveMaterial> ImproveMaterials;
	int32 CostAttr = 2, CostGs = 2, CostUnbind = 2;
	float UnbindQualityPer = 10.f;             // desvincular: +1 a cada tantos % de qualidade

	/** Gema ou vidro rúnico (docs/itens.md "Slot"): classe (armadura, arma), efeitos em %, gema bruta do custo de retirar. */
	struct FGem
	{
		FString Class;
		FName Raw;
		bool bGlass = false;
		TArray<TPair<FName, float>> Effects;
	};
	TMap<FName, FGem> Gems;
	TMap<FString, TArray<FString>> GemClasses;           // classe da gema -> classes de equipamento
	TMap<FName, TMap<FString, FString>> GemEffectText;   // efeito -> texto por idioma, com {v}

	/** Resistência do peito por classe de peso (mín, máx) e fração de cada peça (docs/itens.md "Armaduras e joias"). */
	TMap<FName, FVector2f> ChestResistance;
	TMap<FName, float> SlotResistance;
	/** Resistência física (= mágica) da peça de armadura com a qualidade do item; 0 fora de armadura. */
	float Resistance(const FNWItemDef& D, float Quality) const;

	/** Equipamento com slot: arma, escudo ou armadura fabricados (joias e ferramentas não têm). */
	static bool HasSocket(const FNWItemDef& D, const FNWItem& Item);
	/** A gema (ou vidro) encaixa neste item. */
	bool CanSocket(const FNWItemDef& D, const FNWItem& Item, FName GemId) const;
	/** Linhas de efeito da gema no idioma atual ("+2,5% de resistência física"). */
	TArray<FString> GemLines(FName GemId) const;
	/** Custo de retirar a gema (provisório: 1 gema bruta do mesmo tipo e tier). */
	FName UnsocketCost(FName GemId, int32& OutQty) const;

	/** Classe de equipamento do item ("arma", "escudo", "ferramenta"...; vazio = não leva perk). */
	static FString EquipClass(const FNWItemDef& D);
	/** Perks que podem cair no item: as do equipamento, as de habilidade da arma e Cura causada nas armas de cura. */
	TArray<FName> PerkPool(const FNWItemDef& D) const;
	/** Valor da perk com a qualidade do item (uma casa decimal); 0 se a perk não tem faixa. */
	float PerkValue(FName Perk, float Quality) const;
	/** Texto da perk no idioma atual, com o valor da qualidade. */
	FString PerkText(FName Perk, float Quality, bool bName) const;
	/** Valor do atributo pelo gear score (docs/itens.md, provisório). */
	int32 AttrValue(int32 GearScore) const;
	/** Qualidade mínima do sorteio pelo nível do ofício (0 no nível 0, QualityMinCraftMax no máximo). */
	float QualityMin(int32 SkillLevel, int32 MaxLevel) const;
	/**
	 * Monta o item fabricado: GS sorteado na faixa (Min..Max), atributo, qualidade (de QualMin a 100), perks sem repetir e
	 * vínculo ao equipar. Signer fica com o nome do artesão quando a qualidade passa de SignQuality.
	 */
	FNWItem Roll(FName ItemId, int32 RollMin, int32 RollMax, float QualMin, const FString& Crafter) const;

private:
	FNWItemRules();
};

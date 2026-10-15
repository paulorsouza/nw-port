#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWItems.h"
#include "NWCrafting.generated.h"

class ANWCharacter;
class UNWInteractableComponent;
class UNWInventoryComponent;

/**
 * Refino nas estações da cidade (esteira 4, docs/sistemas.md "Estações de refino"; dados em
 * Data/Crafting/refining.json, tools/make_crafting.py). Receitas do jogo (javelindata_crafting_refining): ingredientes
 * por item ou por categoria, nível mínimo do ofício, tier da estação, taxa em moedas e XP do ofício. Instantâneo: o jogo
 * não tem tempo de refino.
 */
/** Melhorias do item na mesa do ofício dele (docs/itens.md): sortear o atributo, subir o GS, desvincular. */
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
	/** Árvore da tela: família (CraftingCategory) e grupo, com o nome do jogo por idioma. */
	FString Category;
	TMap<FString, FString> CategoryName, GroupName;
	/** Posição na árvore (craftingsortinfo.lua CategorySortOrder/GroupSortOrder; 9999 = fora da lista). */
	int32 CategoryOrder = 9999, GroupOrder = 9999;
	/** Tem FirstCraftAchievementId: a primeira confecção dá XP extra. */
	bool bFirstCraft = false;
	/** Chance de item extra: BonusItemChance e o ajuste por tier do reagente de categoria (Increase/Decrease). */
	float Bonus = 0.f;
	TArray<float> BonusInc, BonusDec;
	/** Fabricar arma, escudo ou ferramenta (item com GS, perks e qualidade; docs/oficio.md): tier do item e GS base. */
	bool bGear = false;
	int32 ItemTier = 0;
	int32 GsBase = 0;
	/** XP de ofício de Count confecções: CategoricalProgressionReward × Σ Qty dos ingredientes × Count
	 *  (crafting.xp_oficio, crafting.xp_ingrediente_contagem; sistemas-005). bFirst: primeira confecção desta receita pelo
	 *  personagem, + base × (mult − 1) (crafting.primeira_confeccao_xp_mult, crafting.primeira_confeccao_ativa). */
	int32 XpFor(int32 Count, bool bFirst = false) const;
};

class NWPORT_API FNWCrafting
{
public:
	static FNWCrafting& Get();

	struct FStation { FString Type; FName Skill; int32 Tier = 0; FVector Pos = FVector::ZeroVector; float Yaw = 0.f; TMap<FString, FString> Name; };
	const TArray<FStation>& Stations() const { return StationList; }
	const FNWRecipe* Find(FName Id) const;
	/** Receitas da estação (tipo e tier), na ordem do jogo. */
	TArray<const FNWRecipe*> ForStation(const FString& Type, int32 Tier) const;
	const TArray<FName>& Category(FName Id) const;
	/** Quanto o personagem tem do ingrediente (categoria: soma dos itens dela). */
	int32 Have(const UNWInventoryComponent* Inv, const FNWRecipe::FIngredient& I) const;
	/** Quantas vezes dá para refinar com o que tem (ingredientes e moedas). */
	int32 MaxCount(const UNWInventoryComponent* Inv, const FNWRecipe& R) const;
	/** Chance de item extra (1 = 100%; pode passar de 1). Chosen: item escolhido por ingrediente (vazio = os da receita). */
	float BonusChance(const FNWRecipe& R, int32 RollBonus, const TArray<FName>& Chosen) const;
	/** Item de categoria que o refino gasta primeiro (o de que se tem mais). */
	FName ChosenFor(const UNWInventoryComponent* Inv, const FNWRecipe::FIngredient& I) const;
	/** Servidor: confere e refina Count vezes; devolve a chave do texto do erro ou vazio. */
	/** Tax: imposto da cidade da mesa (governança), para ir no salvamento da operação. */
	FString Refine(ANWCharacter* Who, const FString& StationType, FName RecipeId, int32 Count, struct FNWCityTaxNote* Tax = nullptr) const;
	/**
	 * Servidor (docs/oficio.md): arma, escudo ou ferramenta, na hora. Abaixo do nível de fabricar é treino (gasta, dá XP,
	 * sem item). Do nível em diante gasta material e taxa e sorteia o item. Com OutItem o item volta para quem chamou (que
	 * pede o número de série e põe na bolsa); sem OutItem vai direto para a bolsa. Devolve a chave do erro ou vazio.
	 */
	FString Craft(ANWCharacter* Who, const FString& StationType, FName RecipeId, FNWItem* OutItem = nullptr, struct FNWCityTaxNote* Tax = nullptr) const;
	/** Receita com tempo desta mesa que fabrica o item (nulo se a mesa não faz o item). */
	const FNWRecipe* RecipeFor(FName ItemId, const FString& StationType) const;
	/** Faixa de GS do item da receita para o personagem: faixa do catálogo + bônus do artesão pelo nível (teto gs_maximo). */
	FIntPoint GsRange(const ANWCharacter* Who, const FNWRecipe& R) const;
	/** Materiais que pagam as melhorias deste item, na ordem de FNWItemRules::ImproveMaterials (None = a receita não tem
	 *  aquele ingrediente). */
	TArray<FName> ImproveMaterials(const FNWRecipe& R) const;
	/** Custo da melhoria no material Mat (índice de ImproveMaterials; provisório, tools/itens.json "_custos").
	 *  Qty 0 = não dá para fazer neste item. */
	FNWRecipe::FIngredient ImproveCost(const FNWRecipe& R, const FNWItem& Item, ENWImprove Kind, uint8 Mat = 0) const;
	/** Refinar GS: quanto pode subir (sucesso) e descer (falha), já presos na faixa da fabricação, e o GS mínimo e
	 *  máximo possíveis. */
	struct FGsOdds { FIntPoint Range = FIntPoint::ZeroValue; int32 UpMax = 0, DownMax = 0; };
	FGsOdds GsOdds(const ANWCharacter* Who, const FNWRecipe& R, const FNWItem& Item, uint8 Mat) const;
	/** Servidor: melhoria no item da bolsa (só a parte dela muda) paga com o material Mat; Before e After recebem o item
	 *  antes e depois. Devolve a chave do erro ou vazio. */
	FString Improve(ANWCharacter* Who, const FString& StationType, int32 BagSlot, ENWImprove Kind, uint8 Mat,
		FNWItem* Before = nullptr, FNWItem* After = nullptr) const;

private:
	FNWCrafting();
	TArray<FStation> StationList;
	TArray<FNWRecipe> Recipes;
	const class ANWStation* NearStation(const ANWCharacter* Who, const FString& StationType) const;
	/** Gasta os ingredientes de Count receitas; devolve o que saiu (para devolver ao cancelar). */
	TArray<FNWItem> Spend(ANWCharacter* Who, const FNWRecipe& R, int32 Count) const;
	TMap<FName, TArray<FName>> Categories;
};

/** Ponto de interação de uma estação de refino (a malha é a da cidade convertida). Criado pelo ANWNpcField. */
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
	/** Cidade do mapa (Data/World/<cidade>_npcs.json "city"): local do armazém "settlement:<cidade>". */
	UPROPERTY(VisibleAnywhere, Category = "NW|Crafting") FString City;
};

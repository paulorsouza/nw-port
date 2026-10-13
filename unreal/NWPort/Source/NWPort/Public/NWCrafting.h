#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
	/** Chance de item extra: BonusItemChance e o ajuste por tier do reagente de categoria (Increase/Decrease). */
	float Bonus = 0.f;
	TArray<float> BonusInc, BonusDec;
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
	FString Refine(ANWCharacter* Who, const FString& StationType, FName RecipeId, int32 Count) const;

private:
	FNWCrafting();
	TArray<FStation> StationList;
	TArray<FNWRecipe> Recipes;
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

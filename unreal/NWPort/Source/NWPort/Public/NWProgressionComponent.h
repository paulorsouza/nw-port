#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWProgressionComponent.generated.h"

/** Atributos do jogo, na ordem da tela (FOR, DES, INT, FOC, CON). */
UENUM()
enum class ENWAttribute : uint8
{
	Strength,
	Dexterity,
	Intelligence,
	Focus,
	Constitution,
	Num UMETA(Hidden),
};

/** Ofícios de coleta do jogo (CategoricalProgressionId). */
namespace NWTradeskill
{
	constexpr int32 Num = 12;
	/** Ofícios de coleta (0..3); os de refino vêm depois (4..8). */
	constexpr int32 NumGathering = 4;
	/** Mining, Logging, Harvesting, Skinning -> 0..3; Smelting, Woodworking, Leatherworking, Weaving, Stonecutting
	 *  -> 4..8 (refino, esteira 4); Weaponsmithing, Engineering, Arcana -> 9..11 (fabricar, docs/oficio.md); outro ->
	 *  INDEX_NONE. */
	NWPORT_API int32 Index(FName Tradeskill);
	NWPORT_API FName Name(int32 Index);
	/** Chave do texto do jogo (ui_mining...) e ícone (icons/tradeskills/icon_tradeskill_<ofício>). */
	NWPORT_API FString TextKey(int32 Index);
	NWPORT_API FString Icon(int32 Index);
}

/**
 * Nível do personagem, XP e atributos, com as tabelas do jogo (Data/Progression/progression.json,
 * tools/make_progression.py): XP total por nível (javelindata_xpamountsbylevel), pontos de atributo ganhos a cada
 * nível e o efeito de cada valor de atributo (javelindata_attribute*). Cada atributo começa em 5.
 * O servidor é autoritativo e salva no nw-core (coluna progression). XP de personagem vem dos tutoriais de nível
 * (níveis inteiros por missão) e, depois deles, dos abates; XP de ofício vem da coleta e do refino.
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWProgressionComponent();

	UPROPERTY(Replicated) int32 Level = 1;
	/** XP total acumulado. */
	UPROPERTY(Replicated) int64 Xp = 0;
	/** Pontos distribuídos por atributo (além da base 5). */
	UPROPERTY(Replicated) TArray<int32> Spent;
	/** XP total de cada ofício de coleta (NWTradeskill). */
	UPROPERTY(Replicated) TArray<int32> TradeXp;
	/**
	 * Progressão veio do nw-core (personagem salvo): vale o nível mínimo de ofício dos coletáveis
	 * (RequiredTradeskillLevel). No Play do editor, sem backend, fica livre para testar.
	 */
	UPROPERTY(Replicated) bool bFromBackend = false;
	/** Nível de ofício que falta para coletar (0 = pode). */
	int32 MissingTradeLevel(FName Tradeskill, int32 Required) const;

	/** Nível do ofício pela tabela do jogo (javelindata_tradeskill<ofício>, InfluenceCost acumulado). */
	int32 TradeLevel(int32 Skill) const;
	int32 TradeMaxLevel(int32 Skill) const;
	/** 0..1 do nível do ofício até o próximo. */
	float TradeProgress(int32 Skill) const;
	/** Bônus de velocidade de coleta do nível (GatheringEfficiency: 0,05 por nível no começo). */
	float TradeEfficiency(int32 Skill) const;
	/** Bônus no dado do saque do nível (RollBonus: +10 por nível). */
	int32 TradeRollBonus(int32 Skill) const;
	/** Servidor: XP de ofício da coleta (CategoricalProgressionReward do evento do coletável). */
	void AddTradeXp(FName Tradeskill, int32 Amount);
	/** XP que falta para o próximo nível do ofício (menu de dev). */
	int32 TradeXpToNext(int32 Skill) const;
	UFUNCTION(Client, Reliable) void ClientTradeXpGained(uint8 Skill, int32 Amount, int32 NewLevel);

	int32 MaxLevel() const;
	/** XP total para chegar ao nível (1 = 0). */
	static int64 XpForLevel(int32 InLevel);
	/** 0..1 do nível atual até o próximo (1 no nível máximo). */
	float LevelProgress() const;
	int32 Attribute(ENWAttribute A) const;
	/** Pontos ganhos até o nível atual menos os distribuídos. */
	int32 UnspentPoints() const;
	/** Vida extra da constituição (coluna Health da tabela do jogo). */
	float ConstitutionHealth() const;

	// ---- servidor
	/** Estado salvo no nw-core (FNWCharacterRecord::Progression). */
	void Load(const TSharedPtr<class FJsonObject>& Json);
	TSharedRef<class FJsonObject> Save() const;
	/** Soma XP e sobe de nível pela tabela; avisa o dono. */
	void AddXp(int64 Amount);

	// ---- dono -> servidor
	UFUNCTION(Server, Reliable) void ServerSpendPoint(ENWAttribute A);
	/** Vários pontos de uma vez (aba Atributos: Confirmar), na ordem FOR, DES, INT, FOC, CON. */
	UFUNCTION(Server, Reliable) void ServerSpendPoints(const TArray<int32>& Points);

	UFUNCTION(Client, Reliable) void ClientXpGained(int64 Amount, int32 NewLevel);

	// ---- tutoriais de nível (esteira 4, tools/tutorials.json; docs/sistemas.md)
	/** Progresso de cada missão (índice do tools/tutorials.json); -1 = concluída. Só o dono recebe. */
	UPROPERTY(Replicated) TArray<int32> TutorialCount;
	/** Receitas já confeccionadas (bônus de primeira confecção; salvo em progression.crafted). Só o dono recebe. */
	UPROPERTY(Replicated) TArray<FName> Crafted;
	bool HasCrafted(FName Recipe) const { return Crafted.Contains(Recipe); }
	void MarkCrafted(FName Recipe);
	/** Compras da semana nas lojas (limite semanal; salvo em progression.shop). Período = FNWNpcRewards::Period. */
	UPROPERTY(Replicated) FString ShopPeriod;
	UPROPERTY(Replicated) TArray<FName> ShopBoughtIds;
	UPROPERTY(Replicated) TArray<int32> ShopBoughtQty;
	int32 ShopBought(FName Item, const FString& Period) const;
	/** Bônus de GS do artesão (mínimo, máximo) no nível atual do ofício para um item do tier (2..5). */
	FIntPoint TradeGsBonus(int32 Skill, int32 ItemTier) const;
	void AddShopBought(FName Item, int32 Quantity, const FString& Period);
	int32 TutorialNum() const;
	FString TutorialName(int32 Index) const;
	int32 TutorialGoal(int32 Index) const;
	bool TutorialsDone() const;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void AdvanceTutorials(const FString& Kind, const FString& Target, int32 Amount);
	void HandleKill(AActor* Killer, AActor* Victim);
	void HandleTradeXp(class ANWCharacter* Who, FName Tradeskill, int32 Amount);
	void HandleRefine(class ANWCharacter* Who, const FString& Station, FName Recipe, int32 Count);
	FDelegateHandle KillHandle, TradeHandle, RefineHandle;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

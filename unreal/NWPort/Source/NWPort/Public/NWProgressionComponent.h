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
	constexpr int32 Num = 4;
	/** Mining, Logging, Harvesting, Skinning -> 0..3; outro -> INDEX_NONE. */
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
 * O servidor é autoritativo e salva no nw-core (coluna progression). XP de personagem ainda não vem de coleta nem
 * de abate (docs/hud-original.md); XP de ofício vem da coleta.
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

	UFUNCTION(Client, Reliable) void ClientXpGained(int64 Amount, int32 NewLevel);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

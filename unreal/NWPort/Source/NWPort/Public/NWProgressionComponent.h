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

/**
 * Nível do personagem, XP e atributos, com as tabelas do jogo (Data/Progression/progression.json,
 * tools/make_progression.py): XP total por nível (javelindata_xpamountsbylevel), pontos de atributo ganhos a cada
 * nível e o efeito de cada valor de atributo (javelindata_attribute*). Cada atributo começa em 5.
 * O servidor é autoritativo. Ainda não é salvo no nw-core nem recebe XP de abates (docs/hud-original.md).
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
	/** Soma XP e sobe de nível pela tabela; avisa o dono. */
	void AddXp(int64 Amount);

	// ---- dono -> servidor
	UFUNCTION(Server, Reliable) void ServerSpendPoint(ENWAttribute A);

	UFUNCTION(Client, Reliable) void ClientXpGained(int64 Amount, int32 NewLevel);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWBrainComponent.generated.h"

class ANWCharacter;

/** Jeito de lutar. Os números de cada um ficam nos campos do componente (presets em UNWBrainComponent::ApplyStyle). */
UENUM(BlueprintType)
enum class ENWBrainStyle : uint8
{
	// Persegue e ataca no intervalo (o comportamento antigo do boneco atacante).
	Brawler,
	// Mantém distância, rodeia, entra com uma sequência de golpes e recua; bloqueia e esquiva quando pode.
	Duelist,
	// Matilha: no máximo AttackersPerTarget atacam juntos; os outros cercam pelos flancos esperando a vez.
	Pack,
	// Investida de longe (ataque de aproximação) e golpe curto de perto; depois da investida fica exposto.
	Charger,
	// Fica na faixa de distância preferida, recua se o alvo chega perto e atira.
	Archer,
};

/** Um ataque que o cérebro pode escolher: linha da tabela de ações, faixa de distância (cm), peso e recarga. */
USTRUCT(BlueprintType)
struct FNWBrainAttack
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Action;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinRange = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxRange = 250.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Weight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Cooldown = 0.f;
	// Ataque que avança (investida, salto): continua empurrando para a frente enquanto a montagem toca.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bGapCloser = false;
	// Golpes em sequência (combo leve): quantos pedidos do mesmo ataque, encaixados pelo buffer do combate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Chain = 1;
	// Ações seguintes, cada uma quando a anterior termina (ex.: preparar investida → investida → frear).
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> FollowUps;
};

/**
 * IA de combate dos inimigos (só no servidor). Escolhe alvo (jogador mais perto dentro de AggroRange do dono),
 * anda, rodeia, ataca e se defende pedindo as mesmas ações que o jogador pede ao UNWCombatComponent: nada de
 * atalho, a IA obedece janelas, recargas e stamina.
 *
 * Com o componente ativo, o ANWCharacter não roda o TickDummy antigo.
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWBrainComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro") ENWBrainStyle Style = ENWBrainStyle::Brawler;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro") TArray<FNWBrainAttack> Attacks;
	// Pausa entre investidas (s, sorteada entre os dois).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro") FVector2D AttackPause = FVector2D(1.2f, 2.4f);
	// Distância de espera: duelista e matilha rodeiam nela; arqueiro atira a partir dela.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro") float HoldDistance = 380.f;
	// Chance de levantar a guarda / esquivar quando o alvo ataca de perto (precisa das linhas BlockStart/Dodge_*).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro", meta = (ClampMin = 0, ClampMax = 1)) float BlockChance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro", meta = (ClampMin = 0, ClampMax = 1)) float DodgeChance = 0.f;
	// Tempo até reagir a um golpe do alvo (s).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro") float ReactionTime = 0.18f;
	// Quantos deste estilo podem atacar o mesmo alvo ao mesmo tempo (matilha e duelistas).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro") int32 AttackersPerTarget = 1;
	// Quadrúpede: anda virado para onde vai (rodeia em arco) e só encara o alvo para atacar.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro") bool bFaceMovement = false;

	/** Aplica os números padrão de um estilo (pode ajustar os campos depois). */
	void ApplyStyle(ENWBrainStyle NewStyle);

	ANWCharacter* GetTarget() const { return Target.Get(); }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	enum class EMode : uint8 { Approach, Hold, Attack, Recover };

	ANWCharacter* FindTarget(float& OutDist) const;
	const FNWBrainAttack* PickAttack(float Dist) const;
	void StartAttack(const FNWBrainAttack& A);
	void TickDefense(ANWCharacter* Me, ANWCharacter* T, float Dist, float DeltaTime);
	void Steer(ANWCharacter* Me, const FVector& Dir, bool bSlow);
	void Face(ANWCharacter* Me, const FVector& Dir, float DeltaTime, float Rate);
	bool TakeToken();
	void ReleaseToken();
	int32 PackIndex() const;

	TWeakObjectPtr<ANWCharacter> Target;
	EMode Mode = EMode::Approach;
	float ModeTime = 0.f;
	float ModeLength = 0.f;
	float NextAttackAt = 0.f;
	float StrafeSign = 1.f;
	bool bHasToken = false;
	const FNWBrainAttack* Current = nullptr;
	int32 ChainLeft = 0;
	int32 FollowIndex = 0;
	float ChainTimer = 0.f;
	TMap<FName, float> ReadyAt;
	// Defesa: último golpe do alvo já avaliado (número da ação) e quando reagir.
	int32 SeenTargetAction = -1;
	float ReactAt = -1.f;
	bool bWillBlock = false;
	bool bWillDodge = false;
	float BlockUntil = 0.f;
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NWCombatTypes.generated.h"

class UAnimMontage;

// Linha de javelindata_damagetable (só as colunas usadas). Chave da linha = DamageID do jogo.
USTRUCT(BlueprintType)
struct FNWDamageRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString WeaponCategory;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString DamageType;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DmgCoef = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HitStun = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BlockHitStun = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PowerLevel = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BlockPowerLevel = 0.f;
	// Empurrão para trás do alvo, em metros (ImpactDistanceY do jogo).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ImpactDistance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool Unblockable = false;
	// Regras completas: docs/combate-regras-jogo.md. Light/Heavy/Ability (limitador de reação da IA).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString AttackType;
	// Empurrão lateral (m, + = direita de quem bate) e fator de decaimento por quadro de 1/30 s (0 = instantâneo).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ImpactDistanceX = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ImpactDecayRate = 0.f;
	// Colunas _AI: valem quando o alvo é IA; 0 = usa a normal.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HitStun_AI = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PowerLevel_AI = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ImpactDistance_AI = 0.f;
	// Crítico: substitui as normais quando > 0 (DmgCoefCrit 0 = multiplicador da arma).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DmgCoefCrit = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CritHitStun = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CritPowerLevel = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CritImpactDistance = 0.f;
	// Acerto na cabeça (projéteis): multiplica o coeficiente.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DmgCoefHead = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool NoBackstab = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool NoHeadshot = false;
	// Stagger: dano de stagger do golpe = BaseStaggerDamage da arma × StaggerDmgModifier; no alvo que está atacando,
	// a resistência é multiplicada pelo StaggerResistModifier da linha do ataque dele.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StaggerDmgModifier = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StaggerResistModifier = 0.f;
	// Bloqueio: fator da stamina perdida por quem bloqueia.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BlockStaminaDmgMod = 0.f;
	// Tremidas (javelindata_camerashake): de quem bate / de quem apanha; no bloqueio; no crítico.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CameraShakeID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName TargetCameraShakeID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName BlockCameraShakeID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName BlockTargetCameraShakeID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CritCameraShakeID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CritTargetCameraShakeID;
};

// Linha de javelindata_camerashake (SShakeParams do CryEngine): deslocamento em m e ângulo em rad nos eixos da
// câmera (X direita, Y frente, Z cima); envelope em s; Frequency = período entre alvos aleatórios (s).
USTRUCT(BlueprintType)
struct FNWCameraShakeRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShiftX = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShiftY = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShiftZ = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AngleX = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AngleY = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AngleZ = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FadeIn = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Sustain = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FadeOut = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Frequency = 0.03f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Randomness = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool FlipVec = true;
};

// Como a ação acerta: varredura da lâmina, ou área (no jogo, CAGE-Damage acerta uma área, não a arma).
UENUM(BlueprintType)
enum class ENWHitShape : uint8
{
	Blade,   // lâmina da arma da mão principal
	Front,   // cone à frente: HitRange (cm) e HitAngle (graus, abertura total)
	Around,  // círculo em volta: HitRange (cm)
	Projectile, // arremesso: projétil sai na janela de dano (HitRange = alcance em cm)
};

// Uma ação de combate: montagem + linha de dano + custo de stamina. Chave = nome da ação (Light1, Heavy, Dodge_F...).
USTRUCT(BlueprintType)
struct FNWActionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UAnimMontage> Montage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName DamageID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StaminaCost = 0.f;
	// Próxima ação do combo (Light1 -> Light2 -> Light3).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ComboNext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ENWHitShape HitShape = ENWHitShape::Blade;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HitRange = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HitAngle = 0.f;
	// Recarga em segundos (javelindata_cooldowns_player); 0 = sem recarga.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Cooldown = 0.f;
	// Efeitos das descrições do jogo: atordoar o alvo atingido (s); buff em si mesmo (s e redução de dano 0..1).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TargetStun = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BuffSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BuffDamageReduction = 0.f;
	// Buff de dano causado (0..1), ex.: Berserk da machadinha.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BuffDamageBonus = 0.f;
	// Efeito de status no alvo atingido (javelindata_statuseffects_<arma>): sangramento, veneno, lentidão.
	// Dano por tique = fração do dano de arma de quem aplicou; MoveSpeed = modificador de velocidade (-0.3 = 30% mais lento).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName StatusID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StatusSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StatusTick = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StatusDamage = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 StatusMaxStacks = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StatusMoveSpeed = 0.f;
	// Efeito em si mesmo ao começar a ação (pressa do Tiro Evasivo, Evasão da rapieira): só velocidade, por StatusSeconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SelfMoveSpeed = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SelfMoveSeconds = 0.f;
	// Consome os acúmulos deste status no alvo: cada acúmulo vira ConsumeDamage × dano de arma na hora (Floreio e
	// Finalização da rapieira).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ConsumeStatusID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ConsumeDamage = 0.f;

	// ---- projéteis (arco; radb Bow e slice basic_arrow do jogo) ----
	// Momentos do disparo (s, separados por ";"; vazio = quando a janela de dano abre) e a linha de dano de cada um.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString FireTimes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString FireDamageIDs;
	// cm/s (0 = arremesso padrão), raio (cm), reto antes da gravidade (cm), atravessa alvos.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ProjectileSpeed = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ProjectileRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float GravityDelay = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bPierce = false;
	// Explosão ao acertar/bater (cm) com outra linha de dano (Flecha Explosiva).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ExplodeRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ExplodeDamageID;
	// Nuvem onde a flecha cai (Disparo Venenoso): raio (cm) e duração (s); envenena quem entra com o Status* da linha.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CloudRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CloudSeconds = 0.f;
	// Área no chão no ponto mirado (Chuva de Flechas): raio (cm), alcance máximo da mira (cm), atraso do acerto (s).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float GroundRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float GroundRange = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float GroundDelay = 0.f;
	// Ação que entra sozinha quando esta termina (tiro → recarga).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName NextAction;
	// Começa a montagem nesta fração do clipe (golpe carregado da rapieira entra em 74% da estocada).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StartAt = 0.f;
	// Reação: início do procclip HitStun no fragmento do .adb (s); a reação dura isso + HitStun/30.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HitStunStart = 0.f;
	// Recarga compartilhada (as quatro direções do Evadir usam "Evade").
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CooldownGroup;
	// Pode ser usada no meio de qualquer ação (Evadir: CanAttack_Evasion desde 0 s).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bCancelsAny = false;
	// Ripostar: segundos em postura (sem dano, sem reação); golpe recebido nesse tempo dispara CounterAction.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CounterSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CounterAction;
	// Atordoamento do alvo termina no primeiro dano que ele recebe (Rapier_Riposte_Stun).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bStunBreaksOnDamage = false;
	// Janela do jogo que libera esta ação no meio de outra (CAGE-TransitionInstant, ex.: CanAttack_ShieldBash,
	// Charge_Bow). A curva "Buffer_<nome>" é o trecho em que o pedido fica guardado. Vazio = CanTransition.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName TransitionCurve;
};

// Efeito de status em curso (servidor).
USTRUCT()
struct FNWStatus
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() float EndTime = 0.f;
	UPROPERTY() int32 Stacks = 1;
	float NextTick = 0.f;
	float Tick = 1.f;
	float DamagePerStack = 0.f;
	float MoveSpeed = 0.f;
	TWeakObjectPtr<class UNWCombatComponent> Source;
};

// Recarga em curso de uma ação (tempo do servidor).
USTRUCT(BlueprintType)
struct FNWCooldown
{
	GENERATED_BODY()
	UPROPERTY() FName Action;
	UPROPERTY() float EndTime = 0.f;
	UPROPERTY() float Duration = 0.f;
};

UENUM(BlueprintType)
enum class ENWCombatState : uint8
{
	Idle,
	Attacking,
	Blocking,
	Dodging,
	Stunned,
	Dead
};

UENUM(BlueprintType)
enum class ENWHitResult : uint8
{
	Hit,
	Blocked,
	BlockBroken,
	Dodged,
	Ignored
};

// Nomes das curvas gravadas nas sequências pelo import_nw.py (janelas do ProcLayer do jogo).
namespace NWCurves
{
	static const FName WeaponTrail(TEXT("WeaponTrail"));
	static const FName DamageWindow(TEXT("DamageWindow"));
	static const FName Invulnerability(TEXT("Invulnerability"));
	static const FName Grit(TEXT("ActivateGrit"));
	static const FName Homing(TEXT("Homing"));
	static const FName CanTransition(TEXT("CanTransition"));
	static const FName CanDodgeCancel(TEXT("CanDodgeCancel"));
	static const FName CanBlockCancel(TEXT("CanBlockCancel"));
	static const FName BlockDisabled(TEXT("BlockDisabled"));
}

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

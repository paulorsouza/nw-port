#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NWCombatTypes.generated.h"

class UAnimMontage;

// Row of javelindata_damagetable (only the columns used). Row key = the game's DamageID.
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
	// Push back of the target, in meters (the game's ImpactDistanceY).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ImpactDistance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool Unblockable = false;
	// Full rules: docs/game/combat.md. Light/Heavy/Ability (AI reaction limiter).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString AttackType;
	// Side push (m, + = attacker's right) and decay factor per 1/30 s frame (0 = instant).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ImpactDistanceX = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ImpactDecayRate = 0.f;
	// _AI columns: apply when the target is AI; 0 = uses the normal one.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HitStun_AI = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PowerLevel_AI = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ImpactDistance_AI = 0.f;
	// Crit: replaces the normal ones when > 0 (DmgCoefCrit 0 = weapon multiplier).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DmgCoefCrit = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CritHitStun = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CritPowerLevel = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CritImpactDistance = 0.f;
	// Headshot (projectiles): multiplies the coefficient.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DmgCoefHead = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool NoBackstab = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool NoHeadshot = false;
	// Stagger: the strike's stagger damage = weapon BaseStaggerDamage × StaggerDmgModifier; on a target that is attacking,
	// the resistance is multiplied by the StaggerResistModifier of its attack's row.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StaggerDmgModifier = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StaggerResistModifier = 0.f;
	// Block: factor of the stamina lost by the blocker.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BlockStaminaDmgMod = 0.f;
	// Shakes (javelindata_camerashake): of the attacker / of the one hit; on block; on crit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CameraShakeID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName TargetCameraShakeID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName BlockCameraShakeID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName BlockTargetCameraShakeID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CritCameraShakeID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CritTargetCameraShakeID;
};

// Row of javelindata_camerashake (CryEngine SShakeParams): displacement in m and angle in rad on the camera
// axes (X right, Y forward, Z up); envelope in s; Frequency = period between random targets (s).
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

// How the action hits: blade sweep, or area (in the game, CAGE-Damage hits an area, not the weapon).
UENUM(BlueprintType)
enum class ENWHitShape : uint8
{
	Blade,   // blade of the main-hand weapon
	Front,   // cone in front: HitRange (cm) and HitAngle (degrees, total opening)
	Around,  // circle around: HitRange (cm)
	Projectile, // throw: the projectile leaves in the damage window (HitRange = range in cm)
};

// A combat action: montage + damage row + stamina cost. Key = action name (Light1, Heavy, Dodge_F...).
USTRUCT(BlueprintType)
struct FNWActionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UAnimMontage> Montage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName DamageID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StaminaCost = 0.f;
	// Mana (javelindata_manacosts_player by the action's CostID; magic weapons): only starts with mana ≥ cost.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ManaCost = 0.f;
	// A basic attack during this action becomes this action (breaking the tomb, early final Ice Spike), if its window opens.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName AttackAction;
	// Niagara system of the projectile (converted game effect, docs/tech/particles.md); replaces the mesh and the trail.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString ProjectileFx;
	// Next combo action (Light1 -> Light2 -> Light3).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ComboNext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ENWHitShape HitShape = ENWHitShape::Blade;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HitRange = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HitAngle = 0.f;
	// Cooldown in seconds (javelindata_cooldowns_player); 0 = no cooldown.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Cooldown = 0.f;
	// Effects from the game descriptions: stun the hit target (s); buff on self (s and damage reduction 0..1).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TargetStun = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BuffSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BuffDamageReduction = 0.f;
	// Damage dealt buff (0..1), e.g. the Hatchet's Berserk.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BuffDamageBonus = 0.f;
	// Status effect on the hit target (javelindata_statuseffects_<weapon>): bleed, poison, slow.
	// Damage per tick = fraction of the applier's weapon damage; MoveSpeed = speed modifier (-0.3 = 30% slower).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName StatusID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StatusSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StatusTick = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StatusDamage = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 StatusMaxStacks = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StatusMoveSpeed = 0.f;
	// Effect on self when the action starts (Evade Shot haste, Rapier Evade): speed only, for StatusSeconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SelfMoveSpeed = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SelfMoveSeconds = 0.f;
	// Consumes this status's stacks on the target: each stack becomes ConsumeDamage × weapon damage immediately (the
	// Rapier's Flourish and Finish).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ConsumeStatusID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ConsumeDamage = 0.f;

	// ---- projectiles (bow; the game's Bow radb and basic_arrow slice) ----
	// Firing moments (s, separated by ";"; empty = when the damage window opens) and the damage row of each one.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString FireTimes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString FireDamageIDs;
	// cm/s (0 = default throw), radius (cm), straight flight before gravity (cm), pierces targets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ProjectileSpeed = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ProjectileRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float GravityDelay = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bPierce = false;
	// Explosion on hit/impact (cm) with another damage row (Explosive Arrow).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ExplodeRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ExplodeDamageID;
	// Cloud where the arrow lands (Poison Shot): radius (cm) and duration (s); poisons whoever enters with the row's Status*.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CloudRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CloudSeconds = 0.f;
	// Ground area at the aimed point (Rain of Arrows): radius (cm), max aim range (cm), hit delay (s).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float GroundRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float GroundRange = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float GroundDelay = 0.f;
	// Action that starts on its own when this one ends (shot → reload).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName NextAction;
	// Starts the montage at this fraction of the clip (the Rapier's charged strike starts at 74% of the thrust).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StartAt = 0.f;
	// Reaction: start of the HitStun procclip in the .adb fragment (s); the reaction lasts this + HitStun/30.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HitStunStart = 0.f;
	// Shared cooldown (the four Evade directions use "Evade").
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CooldownGroup;
	// Can be used in the middle of any action (Evade: CanAttack_Evasion from 0 s).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bCancelsAny = false;
	// Riposte: seconds in stance (no damage, no reaction); a hit received during that time triggers CounterAction.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CounterSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CounterAction;
	// The target's stun ends on the first damage it receives (Rapier_Riposte_Stun).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bStunBreaksOnDamage = false;
	// The game window that allows this action in the middle of another (CAGE-TransitionInstant, e.g. CanAttack_ShieldBash,
	// Charge_Bow). The "Buffer_<name>" curve is the span in which the request stays buffered. Empty = CanTransition.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName TransitionCurve;
};

// Status effect in progress (server).
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

// Cooldown in progress of an action (server time).
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

// Names of the curves written to the sequences by import_nw.py (the game's ProcLayer windows).
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

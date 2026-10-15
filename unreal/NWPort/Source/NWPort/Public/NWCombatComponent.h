#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWCombatTypes.h"
#include "NWCombatComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class UDataTable;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNWOnHitTaken, float, Damage, ENWHitResult, Result, AActor*, Attacker);

/**
 * Combat rules of a character: health, stamina, actions (attack, dodge, block),
 * hits by blade sweep, and reactions. The server decides everything; clients request via RPC
 * and receive the montages via multicast.
 *
 * Time windows come from the sequences' curves (WeaponTrail, CanTransition, Invulnerability...),
 * written from the ProcLayers of the game's .adb.
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWCombatComponent();

	// ---- data ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Dados") TObjectPtr<UDataTable> DamageTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Dados") TObjectPtr<UDataTable> ActionTable;

	// ---- tunable numbers (game simplification) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float MaxHealth = 1000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float WeaponBaseDamage = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números", meta = (ClampMin = 0, ClampMax = 0.95)) float ArmorMitigation = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float MaxStamina = 100.f;
	// Player StaminaComponent (docs/game/combat.md §7): 33.33/s; 10/s with the guard up; 1 s
	// wait after spending (dodge) or releasing the guard; exhausted at 0 until it fills again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float StaminaRegenPerSecond = 33.33f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float StaminaRegenBlocking = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float StaminaRegenDelay = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float BlockArcDegrees = 180.f;
	// Reaction duration, hit stop and push: reverse engineering facts (FNWRules).

	// ---- weapon in use and body (javelindata_itemdefinitions_weapons, game slices; §4–§6) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Arma") float CritChance = 0.03f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Arma") float CritDamageMultiplier = 1.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Arma") float BaseStaggerDamage = 36.f;
	// Stamina the strike takes from the blocker (attacker's weapon) and the blocker's stability (0..100).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Arma") float BlockStaminaDamage = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Arma") float BlockStability = 15.f;
	// Body stagger resistance (player 0, damned 20, boar and wolf 10) and scale of the push received.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Arma") float StaggerResistRating = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Arma") float ImpactDistanceScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float RespawnDelay = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float BladeRadius = 16.f;
	// Hit stop (combate.hit_stop_s, 0 in the game): how much attacker and target slow down while it lasts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float HitStopDilation = 0.05f;
	// Last hit received came from a creature (bite, gore): the reaction does not play the montage's weapon impact.
	bool bLastHitByCreature = false;
	// Heavy charge (the game's Attack_Start_Charge): release before HeavyMinHold = light; between the two = heavy;
	// on reaching HeavyFullCharge the charged strike goes off on its own.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float HeavyMinHold = 0.333f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float HeavyFullCharge = 0.667f;
	// Bow with aim: release before AimFullCharge = normal shot, after = charged (no automatic firing).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float AimFullCharge = 0.5f;

	// ---- replicated state ----
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") float Health = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") float Stamina = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") ENWCombatState State = ENWCombatState::Idle;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") bool bExhausted = false;
	UPROPERTY(Replicated) TArray<FNWCooldown> Cooldowns;
	// Dev menu options (the server decides; replicated to the HUD).
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Dev") bool bNoCooldowns = false;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Dev") bool bGodMode = false;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Dev") bool bInfiniteStamina = false;
	// Damage reduction buff in effect (Defender's Resolve).
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") float DamageReductionUntil = 0.f;
	float DamageReductionAmount = 0.f;
	// Damage dealt buff in effect (Berserk).
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") float DamageBonusUntil = 0.f;
	float DamageBonusAmount = 0.f;
	float GetDamageBonus() const { return ServerNow() < DamageBonusUntil ? DamageBonusAmount : 0.f; }

	// Thrown projectile hit a target (server). bHead: hit the head volume.
	void ProjectileHit(UNWCombatComponent* Target, FName DamageID, const FVector& Point, bool bHead = false, float Falloff = 1.f);
	// Exploding projectile (Explosive Arrow): area with another damage row.
	void ExplodeAt(const FVector& Point, float Radius, FName DamageID, AActor* Skip);

	// ---- status effects (server; Statuses replicates to the HUD) ----
	UPROPERTY(Replicated) TArray<FNWStatus> Statuses;
	void ApplyStatus(FName Id, float Seconds, float Tick, float DamagePerStack, int32 MaxStacks, float MoveSpeed, UNWCombatComponent* Source);
	int32 GetStatusStacks(FName Id) const;
	/** Removes the status and returns the damage it would still deal (stacks × tick × remaining ticks). */
	float ConsumeStatus(FName Id);
	/** Movement speed multiplier from statuses (slow, haste). */
	float GetMoveSpeedMultiplier() const;

	// Server: stuns (stunned loop) for Seconds; full heal; clears cooldowns.
	void ApplyStun(float Seconds, bool bBreakOnDamage = false);
	void HealFull();
	// Draw the weapon (Draw row of the current table), used on weapon swap.
	void PlayDraw();
	void ClearCooldowns() { Cooldowns.Reset(); }
	void Kill(AActor* Killer = nullptr) { if (State != ENWCombatState::Dead) { Health = 0.f; Die(Killer); } }
	// Dead player: who killed and when the Respawn button unlocks (server time). NPCs respawn on their own.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") FString KilledBy;
	UPROPERTY(Replicated) float RespawnAt = 0.f;
	float GetRespawnRemaining() const { return FMath::Max(0.f, RespawnAt - ServerNow()); }
	UFUNCTION(Server, Reliable) void ServerRequestRespawn();

	UPROPERTY(BlueprintAssignable) FNWOnHitTaken OnHitTaken;

	// Main-hand weapon: the sweep goes from base to tip, computed from the mesh bounds.
	UFUNCTION(BlueprintCallable, Category = "NW") void SetMainWeapon(UPrimitiveComponent* Weapon);

	// ---- requests (call on the owning client; they go to the server) ----
	UFUNCTION(BlueprintCallable, Category = "NW") void RequestAction(FName Action);
	UFUNCTION(BlueprintCallable, Category = "NW") void RequestLightAttack() { RequestAction(LightComboStart); }
	UFUNCTION(BlueprintCallable, Category = "NW") void RequestHeavyAttack() { RequestAction(TEXT("Heavy")); }
	// Player attack button: pressing starts the charge; releasing decides light/heavy.
	UFUNCTION(BlueprintCallable, Category = "NW") void AttackPressed();
	UFUNCTION(BlueprintCallable, Category = "NW") void AttackReleased();
	UFUNCTION(BlueprintPure, Category = "NW") bool IsCharging() const { return State == ENWCombatState::Attacking && CurrentAction == ChargeAction; }
	// 0..1 of the charge (for the HUD).
	UFUNCTION(BlueprintPure, Category = "NW") float GetChargeRatio() const;
	// Seconds of cooldown left (0 = ready); OutDuration receives the total cooldown.
	float GetCooldownRemaining(FName Action, float* OutDuration = nullptr) const;
	// Input direction in character space (X forward, Y right).
	UFUNCTION(BlueprintCallable, Category = "NW") void RequestDodge(FVector2D LocalInput);
	// Directional action from the input (X forward, Y right): Prefix + F/B/L/R (Evade_F...).
	void RequestDirectional(const FString& Prefix, FVector2D LocalInput);
	// Bow: aims with the right button (aim state, close camera, walks slowly).
	void RequestAim(bool bAim);
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") bool bAiming = false;
	// The weapon in use aims (bow): the right button aims instead of blocking and the left one shoots.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Arma") bool bAimWeapon = false;
	UFUNCTION(BlueprintCallable, Category = "NW") void RequestBlock(bool bBlock);

	UFUNCTION(BlueprintPure, Category = "NW") bool IsInvulnerable() const;
	UFUNCTION(BlueprintPure, Category = "NW") bool IsBusy() const { return State != ENWCombatState::Idle && State != ENWCombatState::Blocking; }
	UFUNCTION(BlueprintPure, Category = "NW") float GetCurve(FName Curve) const;
	// Curve only from the current action's montage (CAGE-Named window of the current fragment). GetCurve blends the montages
	// still blending out: the open window of the interrupted strike leaked for ~0.1 s and allowed the next click.
	float GetActionCurve(FName Curve) const;

	UPROPERTY(EditAnywhere, Category = "NW|Dados") FName LightComboStart = TEXT("Light1");
	FName GetCurrentAction() const { return CurrentAction; }
	// Server: starts the action without checking the window (timed phases of abilities, NWIceGauntlet.cpp).
	bool ForceAction(FName Action);
	// Server: cooldown that starts outside the start of the action (Ice Pylon on death, Entombed on exit).
	void StartCooldown(FName Key, float Seconds);
	// Counts the actions started on the server (the AI notices the target's "new strike" here).
	int32 GetActionSerial() const { return ActionSerial; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Prediction: the owning client plays the action immediately and sends its number; the server validates, returns the same
	// number in the multicast (the owner does not restart the montage) or rejects (the owner undoes). 0 = no prediction.
	UFUNCTION(Server, Reliable) void ServerRequestAction(FName Action, uint16 PredictionId);
	UFUNCTION(Server, Reliable) void ServerRequestBlock(bool bBlock);
	UFUNCTION(Server, Reliable) void ServerAttackInput(bool bPressed, uint16 PredictionId);
	UFUNCTION(NetMulticast, Reliable) void MulticastPlayMontage(UAnimMontage* Montage, float Rate, uint16 PredictionId, float StartAt);
	UFUNCTION(Client, Reliable) void ClientRejectPrediction(uint16 PredictionId);
	// Looping montage (section linked to itself), e.g. guard up while the button is held.
	UFUNCTION(NetMulticast, Reliable) void MulticastPlayLoop(UAnimMontage* Montage);
	UFUNCTION(NetMulticast, Reliable) void MulticastStopMontages(float BlendOut);
	/** Damage number and, on the local target, the hit direction indicator (From = attacker position). */
	/** Also plays the damage row's camera shakes: ByShake on By's camera, TargetShake on this one's. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastShowHit(float Damage, ENWHitResult Result, FVector_NetQuantize From, bool bByCreature,
		bool bCrit, AActor* By, FName ByShake, FName TargetShake);
	UFUNCTION(NetMulticast, Unreliable) void MulticastHitStop(AActor* Other, float Seconds);
	UFUNCTION(NetMulticast, Unreliable) void MulticastSetMontageRate(UAnimMontage* Montage, float Rate);

	UFUNCTION() void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	UFUNCTION() void HandleMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

private:
	// Pure validation (client and server): state, curve windows, cooldown, stamina; resolves the next in the combo.
	const FNWActionRow* CheckAction(FName& InOutAction) const;
	bool TryStartAction(FName Action);
	// Owning client: plays the predicted action. Returns the prediction number (0 = did not predict).
	uint16 PredictAction(FName Action);
	bool IsLocalOwner() const;
	uint16 NextPredictionId = 1;
	uint16 PendingPrediction = 0;
	double PredictedAt = 0.0;
	ENWCombatState StateBeforePrediction = ENWCombatState::Idle;
	FName ActionBeforePrediction;
	double LocalChargeStart = 0.0;
	FName LocalChargeComboFrom;
	uint16 ServerRequestId = 0;   // server: prediction number of the request being processed
	void StartAction(FName Action, const FNWActionRow& Row);
	void PlayReaction(FName Action, float StunSeconds);
	/** Hit reaction: lasts the HitStun procclip start + HitStun/30 and stretches the rest of the montage. Returns the duration. */
	float PlayHitReaction(FName Action, float HitStun, float ExtraSeconds);
	void PlayBlockIdle();
	void SweepWeapon();
	void AreaHit();
	void ResolveHit(UNWCombatComponent* Target, const FVector& ImpactPoint);
	ENWHitResult ReceiveHit(UNWCombatComponent* Attacker, const FNWDamageRow& Damage, float& OutDamage, bool bHead = false);
	void Push(const FVector& Dir, float Meters, float DecayRate, bool bNoDecay, float ReactionSeconds);
	void TickStatuses(float Now);
	// AI: after a full reaction, 2 s during which a light strike does not interrupt (AI_Reaction_Limiter).
	float ReactionLimiterUntil = 0.f;
	float LastBlockRelease = -100.f;
	void Die(AActor* Killer);
	void Respawn();
	void SpendStamina(float Amount);
	FName DirectionalAction(const TCHAR* Prefix, const FVector& WorldDir) const;
	UAnimInstance* GetAnim() const;
	float ServerNow() const;
	bool BladeEnds(FVector& OutBase, FVector& OutTip) const;

	UPROPERTY() TObjectPtr<UPrimitiveComponent> MainWeapon;
	UPROPERTY() TObjectPtr<UAnimMontage> CurrentMontage;
	UPROPERTY(Replicated) FName CurrentAction;
	UPROPERTY(Replicated) float ChargeStartedAt = 0.f;
	FName ChargeAction = TEXT("HeavyCharge");
	// Light action in progress when the charge started (releasing early continues its combo).
	FName ChargeComboFrom;
	bool bAttackHeld = false;
	bool TryStartCharge();
	void SpawnProjectile(FName DamageID);
	void FireGround();
	UFUNCTION(Server, Reliable) void ServerRequestAim(bool bAim);
	void PlayAimIdle();
	FName CooldownKey(FName Action, const FNWActionRow& Row) const { return Row.CooldownGroup.IsNone() ? Action : Row.CooldownGroup; }
	// Shots already fired in the action (FireTimes) and aimed ground point (Rain of Arrows).
	int32 ShotsFired = 0;
	// Damage windows opened in this action (Flurry: the 3rd window uses the 3rd row of FireDamageIDs).
	int32 DamageWindowIndex = 0;
	bool bGroundFired = false;
	FVector GroundTarget = FVector::ZeroVector;
	bool bStunBreaksOnDamage = false;
	float CounterUntil = 0.f;
	bool bJustShot = false;
	// Bow: click made before the shot window (becomes an uncharged shot when it opens).
	bool bAimClickBuffered = false;
	FName ReactionNext;
	float ActionStartedAt = 0.f;
	void ReleaseCharge();
	FNWActionRow CurrentRow;
	int32 ActionSerial = 0;
	bool bDamageWindowWasOpen = false;
	FName BufferedAction;
	TSet<TWeakObjectPtr<AActor>> HitThisSwing;
	FVector PrevBase = FVector::ZeroVector;
	FVector PrevTip = FVector::ZeroVector;
	bool bSweepActive = false;
	float LastStaminaSpend = -100.f;
	float StunUntil = 0.f;
	FTransform SpawnTransform;
	FVector BladeLocalBase = FVector::ZeroVector;
	FVector BladeLocalTip = FVector::ZeroVector;
	FTimerHandle RespawnTimer;
	FTimerHandle HitStopTimer;
	FTimerHandle ReactionRateTimer;
};

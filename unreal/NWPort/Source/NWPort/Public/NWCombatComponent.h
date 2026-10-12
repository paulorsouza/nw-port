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
 * Regras de combate de um personagem: vida, stamina, ações (ataque, esquiva, bloqueio),
 * acerto por varredura da lâmina e reações. O servidor decide tudo; os clientes pedem por RPC
 * e recebem as montagens por multicast.
 *
 * As janelas de tempo vêm das curvas das sequências (WeaponTrail, CanTransition, Invulnerability...),
 * gravadas a partir dos ProcLayer do .adb do jogo.
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWCombatComponent();

	// ---- dados ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Dados") TObjectPtr<UDataTable> DamageTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Dados") TObjectPtr<UDataTable> ActionTable;

	// ---- números ajustáveis (simplificação do jogo) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float MaxHealth = 1000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float WeaponBaseDamage = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números", meta = (ClampMin = 0, ClampMax = 0.95)) float ArmorMitigation = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float MaxStamina = 100.f;
	// StaminaComponent do jogador (docs/combate-regras-jogo.md §7): 33,33/s; 10/s com a guarda levantada; 1 s de
	// espera depois de gastar (esquiva) ou soltar a guarda; exausto em 0 até encher de novo.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float StaminaRegenPerSecond = 33.33f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float StaminaRegenBlocking = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float StaminaRegenDelay = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float BlockArcDegrees = 180.f;
	// Duração da reação, hit stop e empurrão: fatos da engenharia reversa (FNWRules, docs/analise-re/valores.json).

	// ---- arma em uso e corpo (javelindata_itemdefinitions_weapons, slices do jogo; §4–§6) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Arma") float CritChance = 0.03f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Arma") float CritDamageMultiplier = 1.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Arma") float BaseStaggerDamage = 36.f;
	// Stamina que o golpe tira de quem bloqueia (arma de quem bate) e estabilidade de quem bloqueia (0..100).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Arma") float BlockStaminaDamage = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Arma") float BlockStability = 15.f;
	// Resistência a stagger do corpo (jogador 0, damned 20, javali e lobo 10) e escala do empurrão recebido.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Arma") float StaggerResistRating = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Arma") float ImpactDistanceScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float RespawnDelay = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float BladeRadius = 16.f;
	// Hit stop (combate.hit_stop_s, 0 no jogo): quanto atacante e alvo desaceleram enquanto dura.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float HitStopDilation = 0.05f;
	// Último golpe recebido veio de criatura (mordida, chifrada): a reação não toca o impacto de arma da montagem.
	bool bLastHitByCreature = false;
	// Carga do pesado (Attack_Start_Charge do jogo): soltar antes de HeavyMinHold = leve; entre os dois = pesado;
	// ao chegar em HeavyFullCharge o golpe carregado sai sozinho.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float HeavyMinHold = 0.333f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float HeavyFullCharge = 0.667f;
	// Arco com mira: soltar antes de AimFullCharge = tiro normal, depois = carregado (sem disparo automático).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float AimFullCharge = 0.5f;

	// ---- estado replicado ----
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") float Health = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") float Stamina = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") ENWCombatState State = ENWCombatState::Idle;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") bool bExhausted = false;
	UPROPERTY(Replicated) TArray<FNWCooldown> Cooldowns;
	// Opções do menu de dev (servidor decide; replicadas para a HUD).
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Dev") bool bNoCooldowns = false;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Dev") bool bGodMode = false;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Dev") bool bInfiniteStamina = false;
	// Buff de redução de dano em curso (Determinação do Defensor).
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") float DamageReductionUntil = 0.f;
	float DamageReductionAmount = 0.f;
	// Buff de dano causado em curso (Berserk).
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") float DamageBonusUntil = 0.f;
	float DamageBonusAmount = 0.f;
	float GetDamageBonus() const { return ServerNow() < DamageBonusUntil ? DamageBonusAmount : 0.f; }

	// Projétil arremessado atingiu um alvo (servidor). bHead: acertou o volume da cabeça.
	void ProjectileHit(UNWCombatComponent* Target, FName DamageID, const FVector& Point, bool bHead = false, float Falloff = 1.f);
	// Projétil que explode (Flecha Explosiva): área com outra linha de dano.
	void ExplodeAt(const FVector& Point, float Radius, FName DamageID, AActor* Skip);

	// ---- efeitos de status (servidor; Statuses replica para a HUD) ----
	UPROPERTY(Replicated) TArray<FNWStatus> Statuses;
	void ApplyStatus(FName Id, float Seconds, float Tick, float DamagePerStack, int32 MaxStacks, float MoveSpeed, UNWCombatComponent* Source);
	int32 GetStatusStacks(FName Id) const;
	/** Tira o status e devolve o dano que ele ainda causaria (acúmulos × tique × tiques restantes). */
	float ConsumeStatus(FName Id);
	/** Multiplicador de velocidade de movimento dos status (lentidão, pressa). */
	float GetMoveSpeedMultiplier() const;

	// Servidor: atordoa (loop de atordoado) por Seconds; cura total; limpa recargas.
	void ApplyStun(float Seconds, bool bBreakOnDamage = false);
	void HealFull();
	// Sacar a arma (linha Draw da tabela atual), usado na troca de arma.
	void PlayDraw();
	void ClearCooldowns() { Cooldowns.Reset(); }
	void Kill(AActor* Killer = nullptr) { if (State != ENWCombatState::Dead) { Health = 0.f; Die(Killer); } }
	// Jogador morto: quem matou e quando o botão Renascer libera (tempo do servidor). NPC renasce sozinho.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") FString KilledBy;
	UPROPERTY(Replicated) float RespawnAt = 0.f;
	float GetRespawnRemaining() const { return FMath::Max(0.f, RespawnAt - ServerNow()); }
	UFUNCTION(Server, Reliable) void ServerRequestRespawn();

	UPROPERTY(BlueprintAssignable) FNWOnHitTaken OnHitTaken;

	// Arma da mão principal: a varredura vai da base à ponta, calculadas pelos limites da malha.
	UFUNCTION(BlueprintCallable, Category = "NW") void SetMainWeapon(UPrimitiveComponent* Weapon);

	// ---- pedidos (chamar no cliente dono; vão para o servidor) ----
	UFUNCTION(BlueprintCallable, Category = "NW") void RequestAction(FName Action);
	UFUNCTION(BlueprintCallable, Category = "NW") void RequestLightAttack() { RequestAction(LightComboStart); }
	UFUNCTION(BlueprintCallable, Category = "NW") void RequestHeavyAttack() { RequestAction(TEXT("Heavy")); }
	// Botão de ataque do jogador: apertar começa a carga; soltar decide leve/pesado.
	UFUNCTION(BlueprintCallable, Category = "NW") void AttackPressed();
	UFUNCTION(BlueprintCallable, Category = "NW") void AttackReleased();
	UFUNCTION(BlueprintPure, Category = "NW") bool IsCharging() const { return State == ENWCombatState::Attacking && CurrentAction == ChargeAction; }
	// 0..1 da carga (para HUD).
	UFUNCTION(BlueprintPure, Category = "NW") float GetChargeRatio() const;
	// Segundos que faltam de recarga (0 = pronta); OutDuration recebe a recarga total.
	float GetCooldownRemaining(FName Action, float* OutDuration = nullptr) const;
	// Direção do input em espaço do personagem (X frente, Y direita).
	UFUNCTION(BlueprintCallable, Category = "NW") void RequestDodge(FVector2D LocalInput);
	// Ação direcional pelo input (X frente, Y direita): Prefix + F/B/L/R (Evade_F...).
	void RequestDirectional(const FString& Prefix, FVector2D LocalInput);
	// Arco: mira com o botão direito (estado de mira, câmera perto, anda devagar).
	void RequestAim(bool bAim);
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") bool bAiming = false;
	// A arma em uso mira (arco): o botão direito mira em vez de bloquear e o esquerdo atira.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Arma") bool bAimWeapon = false;
	UFUNCTION(BlueprintCallable, Category = "NW") void RequestBlock(bool bBlock);

	UFUNCTION(BlueprintPure, Category = "NW") bool IsInvulnerable() const;
	UFUNCTION(BlueprintPure, Category = "NW") bool IsBusy() const { return State != ENWCombatState::Idle && State != ENWCombatState::Blocking; }
	UFUNCTION(BlueprintPure, Category = "NW") float GetCurve(FName Curve) const;

	UPROPERTY(EditAnywhere, Category = "NW|Dados") FName LightComboStart = TEXT("Light1");
	FName GetCurrentAction() const { return CurrentAction; }
	// Conta as ações começadas no servidor (a IA percebe "golpe novo" do alvo por aqui).
	int32 GetActionSerial() const { return ActionSerial; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Predição: o cliente dono toca a ação na hora e manda o número dela; o servidor valida, devolve o mesmo
	// número no multicast (o dono não recomeça a montagem) ou recusa (o dono desfaz). 0 = sem predição.
	UFUNCTION(Server, Reliable) void ServerRequestAction(FName Action, uint16 PredictionId);
	UFUNCTION(Server, Reliable) void ServerRequestBlock(bool bBlock);
	UFUNCTION(Server, Reliable) void ServerAttackInput(bool bPressed, uint16 PredictionId);
	UFUNCTION(NetMulticast, Reliable) void MulticastPlayMontage(UAnimMontage* Montage, float Rate, uint16 PredictionId, float StartAt);
	UFUNCTION(Client, Reliable) void ClientRejectPrediction(uint16 PredictionId);
	// Montagem em loop (seção ligada nela mesma), ex.: guarda levantada enquanto o botão está pressionado.
	UFUNCTION(NetMulticast, Reliable) void MulticastPlayLoop(UAnimMontage* Montage);
	UFUNCTION(NetMulticast, Reliable) void MulticastStopMontages(float BlendOut);
	/** Número de dano e, no alvo local, o indicador de direção do golpe (From = posição do atacante). */
	/** Também toca as tremidas de câmera da linha de dano: ByShake na câmera de By, TargetShake na deste. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastShowHit(float Damage, ENWHitResult Result, FVector_NetQuantize From, bool bByCreature,
		bool bCrit, AActor* By, FName ByShake, FName TargetShake);
	UFUNCTION(NetMulticast, Unreliable) void MulticastHitStop(AActor* Other, float Seconds);
	UFUNCTION(NetMulticast, Unreliable) void MulticastSetMontageRate(UAnimMontage* Montage, float Rate);

	UFUNCTION() void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	UFUNCTION() void HandleMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

private:
	// Validação pura (cliente e servidor): estado, janelas das curvas, recarga, vigor; resolve o próximo do combo.
	const FNWActionRow* CheckAction(FName& InOutAction) const;
	bool TryStartAction(FName Action);
	// Cliente dono: toca a ação prevista. Devolve o número da predição (0 = não previu).
	uint16 PredictAction(FName Action);
	bool IsLocalOwner() const;
	uint16 NextPredictionId = 1;
	uint16 PendingPrediction = 0;
	double PredictedAt = 0.0;
	ENWCombatState StateBeforePrediction = ENWCombatState::Idle;
	FName ActionBeforePrediction;
	double LocalChargeStart = 0.0;
	FName LocalChargeComboFrom;
	uint16 ServerRequestId = 0;   // servidor: número da predição do pedido em processamento
	void StartAction(FName Action, const FNWActionRow& Row);
	void PlayReaction(FName Action, float StunSeconds);
	/** Reação a golpe: dura início do procclip HitStun + HitStun/30 e estica o resto da montagem. Devolve a duração. */
	float PlayHitReaction(FName Action, float HitStun, float ExtraSeconds);
	void PlayBlockIdle();
	void SweepWeapon();
	void AreaHit();
	void ResolveHit(UNWCombatComponent* Target, const FVector& ImpactPoint);
	ENWHitResult ReceiveHit(UNWCombatComponent* Attacker, const FNWDamageRow& Damage, float& OutDamage, bool bHead = false);
	void Push(const FVector& Dir, float Meters, float DecayRate, bool bNoDecay, float ReactionSeconds);
	void TickStatuses(float Now);
	// IA: depois de uma reação completa, 2 s em que golpe leve não interrompe (AI_Reaction_Limiter).
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
	// Ação leve que estava em curso quando a carga começou (soltar cedo continua o combo dela).
	FName ChargeComboFrom;
	bool bAttackHeld = false;
	bool TryStartCharge();
	void SpawnProjectile(FName DamageID);
	void FireGround();
	UFUNCTION(Server, Reliable) void ServerRequestAim(bool bAim);
	void PlayAimIdle();
	FName CooldownKey(FName Action, const FNWActionRow& Row) const { return Row.CooldownGroup.IsNone() ? Action : Row.CooldownGroup; }
	// Disparos já feitos na ação (FireTimes) e ponto no chão mirado (Chuva de Flechas).
	int32 ShotsFired = 0;
	// Janelas de dano abertas nesta ação (Rajada: a 3ª janela usa a 3ª linha de FireDamageIDs).
	int32 DamageWindowIndex = 0;
	bool bGroundFired = false;
	FVector GroundTarget = FVector::ZeroVector;
	bool bStunBreaksOnDamage = false;
	float CounterUntil = 0.f;
	bool bJustShot = false;
	// Arco: clique dado antes da janela de tiro (vira tiro sem carga quando ela abrir).
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

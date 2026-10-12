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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float StaminaRegenPerSecond = 35.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float StaminaRegenDelay = 1.f;
	// Stamina perdida ao bloquear = base + por nível de BlockPowerLevel do golpe.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float BlockStaminaBase = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float BlockStaminaPerPower = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float BlockArcDegrees = 180.f;
	// Unidade do HitStun do jogo ainda não confirmada; 0.01 s por unidade como ponto de partida.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float SecondsPerHitStun = 0.01f;
	// Golpes com PowerLevel acima disto interrompem o alvo (fora da janela de grit).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float Poise = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float RespawnDelay = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float BladeRadius = 16.f;
	// Hit-stop: atacante e alvo quase param por um instante no acerto (peso do golpe).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float HitStopSeconds = 0.07f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float HitStopDilation = 0.05f;
	// Carga do pesado (Attack_Start_Charge do jogo): soltar antes de HeavyMinHold = leve; entre os dois = pesado;
	// ao chegar em HeavyFullCharge o golpe carregado sai sozinho.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float HeavyMinHold = 0.333f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float HeavyFullCharge = 0.667f;

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

	// Projétil arremessado atingiu um alvo (servidor).
	void ProjectileHit(UNWCombatComponent* Target, FName DamageID, const FVector& Point);

	// Servidor: atordoa (loop de atordoado) por Seconds; cura total; limpa recargas.
	void ApplyStun(float Seconds);
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
	UFUNCTION(BlueprintCallable, Category = "NW") void RequestBlock(bool bBlock);

	UFUNCTION(BlueprintPure, Category = "NW") bool IsInvulnerable() const;
	UFUNCTION(BlueprintPure, Category = "NW") bool IsBusy() const { return State != ENWCombatState::Idle && State != ENWCombatState::Blocking; }
	UFUNCTION(BlueprintPure, Category = "NW") float GetCurve(FName Curve) const;

	UPROPERTY(EditAnywhere, Category = "NW|Dados") FName LightComboStart = TEXT("Light1");

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Predição: o cliente dono toca a ação na hora e manda o número dela; o servidor valida, devolve o mesmo
	// número no multicast (o dono não recomeça a montagem) ou recusa (o dono desfaz). 0 = sem predição.
	UFUNCTION(Server, Reliable) void ServerRequestAction(FName Action, uint16 PredictionId);
	UFUNCTION(Server, Reliable) void ServerRequestBlock(bool bBlock);
	UFUNCTION(Server, Reliable) void ServerAttackInput(bool bPressed, uint16 PredictionId);
	UFUNCTION(NetMulticast, Reliable) void MulticastPlayMontage(UAnimMontage* Montage, float Rate, uint16 PredictionId);
	UFUNCTION(Client, Reliable) void ClientRejectPrediction(uint16 PredictionId);
	// Montagem em loop (seção ligada nela mesma), ex.: guarda levantada enquanto o botão está pressionado.
	UFUNCTION(NetMulticast, Reliable) void MulticastPlayLoop(UAnimMontage* Montage);
	UFUNCTION(NetMulticast, Reliable) void MulticastStopMontages(float BlendOut);
	/** Número de dano e, no alvo local, o indicador de direção do golpe (From = posição do atacante). */
	UFUNCTION(NetMulticast, Unreliable) void MulticastShowHit(float Damage, ENWHitResult Result, FVector_NetQuantize From);
	UFUNCTION(NetMulticast, Unreliable) void MulticastHitStop(AActor* Other, float Seconds);

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
	void PlayBlockIdle();
	void SweepWeapon();
	void AreaHit();
	void ResolveHit(UNWCombatComponent* Target, const FVector& ImpactPoint);
	ENWHitResult ReceiveHit(UNWCombatComponent* Attacker, const FNWDamageRow& Damage, float& OutDamage);
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
	void SpawnProjectile();
	bool bProjectileFired = false;
	float ActionStartedAt = 0.f;
	void ReleaseCharge();
	FNWActionRow CurrentRow;
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
};

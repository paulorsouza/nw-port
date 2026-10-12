#include "NWCombatComponent.h"
#include "NWText.h"
#include "NWProjectile.h"
#include "NWGameMode.h"
#include "NWCharacter.h"
#include "NWInventoryComponent.h"
#include "NWPlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"

// nw.DebugHits 1: desenha a varredura da lâmina (verde) e os acertos (vermelho/ciano) para ajustar a hitbox.
static TAutoConsoleVariable<int32> CVarNWDebugHits(TEXT("nw.DebugHits"), 0, TEXT("Desenha a hitbox da lâmina e os acertos"));

UNWCombatComponent::UNWCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UNWCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNWCombatComponent, Health);
	DOREPLIFETIME(UNWCombatComponent, Stamina);
	DOREPLIFETIME(UNWCombatComponent, State);
	DOREPLIFETIME_CONDITION(UNWCombatComponent, KilledBy, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UNWCombatComponent, RespawnAt, COND_OwnerOnly);
	DOREPLIFETIME(UNWCombatComponent, bExhausted);
	DOREPLIFETIME(UNWCombatComponent, CurrentAction);
	DOREPLIFETIME(UNWCombatComponent, ChargeStartedAt);
	DOREPLIFETIME(UNWCombatComponent, Cooldowns);
	DOREPLIFETIME(UNWCombatComponent, bNoCooldowns);
	DOREPLIFETIME(UNWCombatComponent, bGodMode);
	DOREPLIFETIME(UNWCombatComponent, bInfiniteStamina);
	DOREPLIFETIME(UNWCombatComponent, DamageReductionUntil);
	DOREPLIFETIME(UNWCombatComponent, DamageBonusUntil);
}

void UNWCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	SpawnTransform = GetOwner()->GetActorTransform();
	if (GetOwner()->HasAuthority())
	{
		Health = MaxHealth;
		Stamina = MaxStamina;
	}
	if (UAnimInstance* Anim = GetAnim())
	{
		Anim->OnMontageEnded.AddDynamic(this, &UNWCombatComponent::HandleMontageEnded);
		Anim->OnMontageBlendingOut.AddDynamic(this, &UNWCombatComponent::HandleMontageBlendingOut);
	}
}

UAnimInstance* UNWCombatComponent::GetAnim() const
{
	const ACharacter* Char = Cast<ACharacter>(GetOwner());
	return Char && Char->GetMesh() ? Char->GetMesh()->GetAnimInstance() : nullptr;
}

float UNWCombatComponent::GetCurve(FName Curve) const
{
	const UAnimInstance* Anim = GetAnim();
	return Anim ? Anim->GetCurveValue(Curve) : 0.f;
}

bool UNWCombatComponent::IsInvulnerable() const
{
	return State == ENWCombatState::Dead || GetCurve(NWCurves::Invulnerability) > 0.5f;
}

void UNWCombatComponent::SetMainWeapon(UPrimitiveComponent* Weapon)
{
	MainWeapon = Weapon;
	if (!Weapon)
	{
		return;
	}
	// Lâmina = maior eixo da caixa local da malha; a base fica na ponta mais perto da origem (a pega).
	const FBoxSphereBounds Local = Weapon->CalcBounds(FTransform::Identity);
	const FVector Ext = Local.BoxExtent;
	FVector Axis = FVector::XAxisVector;
	if (Ext.Y >= Ext.X && Ext.Y >= Ext.Z) Axis = FVector::YAxisVector;
	if (Ext.Z >= Ext.X && Ext.Z >= Ext.Y) Axis = FVector::ZAxisVector;
	const float Half = Ext.Dot(Axis);
	const FVector A = Local.Origin - Axis * Half;
	const FVector B = Local.Origin + Axis * Half;
	BladeLocalBase = A.Size() < B.Size() ? A : B;
	BladeLocalTip = A.Size() < B.Size() ? B : A;
}

bool UNWCombatComponent::BladeEnds(FVector& OutBase, FVector& OutTip) const
{
	if (!MainWeapon)
	{
		return false;
	}
	const FTransform& T = MainWeapon->GetComponentTransform();
	OutBase = T.TransformPosition(BladeLocalBase);
	OutTip = T.TransformPosition(BladeLocalTip);
	return true;
}

// ---------------------------------------------------------------- pedidos

static TAutoConsoleVariable<int32> CVarDebugPrediction(TEXT("nw.DebugPrediction"), 0, TEXT("Registra ações previstas, confirmadas e recusadas."));

bool UNWCombatComponent::IsLocalOwner() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled();
}

uint16 UNWCombatComponent::PredictAction(FName Action)
{
	if (GetOwner()->HasAuthority() || !IsLocalOwner())
	{
		return 0;
	}
	const FNWActionRow* Row = CheckAction(Action);
	UAnimMontage* Montage = Row ? Row->Montage.LoadSynchronous() : nullptr;
	UAnimInstance* Anim = GetAnim();
	if (!Montage || !Anim)
	{
		return 0;
	}
	const uint16 Id = NextPredictionId++;
	if (NextPredictionId == 0)
	{
		NextPredictionId = 1;
	}
	StateBeforePrediction = State;
	ActionBeforePrediction = CurrentAction;
	// Estado local igual ao que o servidor vai replicar; se ele recusar, ClientRejectPrediction desfaz.
	const FString Name = Action.ToString();
	State = Name.StartsWith(TEXT("Dodge")) ? ENWCombatState::Dodging : Name.StartsWith(TEXT("Block")) ? ENWCombatState::Blocking : ENWCombatState::Attacking;
	CurrentAction = Action;
	CurrentMontage = Montage;
	Anim->Montage_Play(Montage, 1.f);
	PendingPrediction = Id;
	PredictedAt = FPlatformTime::Seconds();
	if (CVarDebugPrediction.GetValueOnGameThread())
	{
		UE_LOG(LogTemp, Display, TEXT("NW predição %u: %s"), Id, *Action.ToString());
	}
	return Id;
}

void UNWCombatComponent::ClientRejectPrediction_Implementation(uint16 PredictionId)
{
	if (PredictionId != PendingPrediction)
	{
		return;
	}
	PendingPrediction = 0;
	if (UAnimInstance* Anim = GetAnim())
	{
		Anim->Montage_Stop(0.15f, CurrentMontage);
	}
	State = StateBeforePrediction;
	CurrentAction = ActionBeforePrediction;
	if (CVarDebugPrediction.GetValueOnGameThread())
	{
		UE_LOG(LogTemp, Display, TEXT("NW predição %u recusada após %.0f ms"), PredictionId, (FPlatformTime::Seconds() - PredictedAt) * 1000.0);
	}
}

void UNWCombatComponent::RequestAction(FName Action)
{
	ServerRequestAction(Action, PredictAction(Action));
}

void UNWCombatComponent::AttackPressed()
{
	uint16 Id = 0;
	if (!GetOwner()->HasAuthority() && IsLocalOwner())
	{
		LocalChargeComboFrom = CurrentAction.ToString().StartsWith(TEXT("Light")) ? CurrentAction : NAME_None;
		Id = PredictAction(ChargeAction);
		if (Id)
		{
			LocalChargeStart = FPlatformTime::Seconds();
		}
	}
	ServerAttackInput(true, Id);
}

void UNWCombatComponent::AttackReleased()
{
	uint16 Id = 0;
	// Soltou durante a carga prevista: prevê leve/pesado pelo tempo segurado (abaixo do carregado automático).
	if (!GetOwner()->HasAuthority() && IsLocalOwner() && CurrentAction == ChargeAction && LocalChargeStart > 0.0)
	{
		const float Held = float(FPlatformTime::Seconds() - LocalChargeStart);
		LocalChargeStart = 0.0;
		if (Held < HeavyFullCharge)
		{
			FName Action = TEXT("Heavy");
			if (Held < HeavyMinHold)
			{
				Action = LightComboStart;
				const FNWActionRow* From = !LocalChargeComboFrom.IsNone() && ActionTable ? ActionTable->FindRow<FNWActionRow>(LocalChargeComboFrom, TEXT("combo")) : nullptr;
				if (From && !From->ComboNext.IsNone())
				{
					Action = From->ComboNext;
				}
			}
			// Da carga para o golpe não há janela a checar: só a linha e o vigor.
			const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(Action, TEXT("release")) : nullptr;
			if (Row && (Row->StaminaCost <= 0.f || Stamina > 0.f))
			{
				const ENWCombatState Saved = State;
				State = ENWCombatState::Idle;   // CheckAction de dentro da carga recusaria
				Id = PredictAction(Action);
				if (!Id)
				{
					State = Saved;
				}
				else
				{
					StateBeforePrediction = Saved;
					ActionBeforePrediction = ChargeAction;
				}
			}
		}
	}
	ServerAttackInput(false, Id);
}

void UNWCombatComponent::RequestDodge(FVector2D LocalInput)
{
	FName Action = TEXT("Dodge_B");
	if (!LocalInput.IsNearlyZero())
	{
		if (FMath::Abs(LocalInput.X) >= FMath::Abs(LocalInput.Y))
		{
			Action = LocalInput.X > 0 ? TEXT("Dodge_F") : TEXT("Dodge_B");
		}
		else
		{
			Action = LocalInput.Y > 0 ? TEXT("Dodge_R") : TEXT("Dodge_L");
		}
	}
	RequestAction(Action);
}

void UNWCombatComponent::RequestBlock(bool bBlock)
{
	ServerRequestBlock(bBlock);
}

void UNWCombatComponent::ServerRequestAction_Implementation(FName Action, uint16 PredictionId)
{
	ServerRequestId = PredictionId;
	const bool bStarted = TryStartAction(Action);
	ServerRequestId = 0;
	if (!bStarted)
	{
		if (State == ENWCombatState::Attacking)
		{
			// Fora da janela: guarda para quando CanTransition (ou CanDodgeCancel, na esquiva) ligar.
			BufferedAction = Action;
		}
		if (PredictionId)
		{
			ClientRejectPrediction(PredictionId);
		}
	}
}

void UNWCombatComponent::ServerAttackInput_Implementation(bool bPressed, uint16 PredictionId)
{
	bAttackHeld = bPressed;
	ServerRequestId = PredictionId;
	bool bStarted = false;
	if (bPressed)
	{
		// Fora da janela (meio de um golpe), a carga começa no Tick quando ela abrir, se o botão continuar apertado.
		bStarted = TryStartCharge();
	}
	else if (IsCharging())
	{
		const FName Before = CurrentAction;
		ReleaseCharge();
		bStarted = CurrentAction != Before;
	}
	else
	{
		bStarted = TryStartAction(LightComboStart);
		if (!bStarted && State == ENWCombatState::Attacking)
		{
			BufferedAction = LightComboStart;
		}
	}
	ServerRequestId = 0;
	if (!bStarted && PredictionId)
	{
		ClientRejectPrediction(PredictionId);
	}
}

bool UNWCombatComponent::TryStartCharge()
{
	const FName From = CurrentAction.ToString().StartsWith(TEXT("Light")) ? CurrentAction : NAME_None;
	if (!TryStartAction(ChargeAction))
	{
		return false;
	}
	ChargeComboFrom = From;
	ChargeStartedAt = ServerNow();
	return true;
}

void UNWCombatComponent::ReleaseCharge()
{
	const float Held = ServerNow() - ChargeStartedAt;
	FName Action = TEXT("Heavy");
	if (Held < HeavyMinHold)
	{
		// Toque: ataque leve, seguindo o combo que estava em curso.
		Action = LightComboStart;
		if (!ChargeComboFrom.IsNone())
		{
			const FNWActionRow* From = ActionTable->FindRow<FNWActionRow>(ChargeComboFrom, TEXT("combo"));
			if (From && !From->ComboNext.IsNone())
			{
				Action = From->ComboNext;
			}
		}
	}
	const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(Action, TEXT("release")) : nullptr;
	if (Row && (Row->StaminaCost <= 0.f || Stamina > 0.f))
	{
		StartAction(Action, *Row);
	}
}

float UNWCombatComponent::ServerNow() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	const AGameStateBase* GS = World->GetGameState();
	return GS ? GS->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

float UNWCombatComponent::GetCooldownRemaining(FName Action, float* OutDuration) const
{
	for (const FNWCooldown& C : Cooldowns)
	{
		if (C.Action == Action)
		{
			if (OutDuration)
			{
				*OutDuration = C.Duration;
			}
			return FMath::Max(0.f, C.EndTime - ServerNow());
		}
	}
	if (OutDuration)
	{
		*OutDuration = 0.f;
	}
	return 0.f;
}

float UNWCombatComponent::GetChargeRatio() const
{
	if (!IsCharging() || !GetWorld())
	{
		return 0.f;
	}
	return FMath::Clamp((ServerNow() - ChargeStartedAt) / HeavyFullCharge, 0.f, 1.f);
}

void UNWCombatComponent::ServerRequestBlock_Implementation(bool bBlock)
{
	if (bBlock)
	{
		TryStartAction(TEXT("BlockStart"));
	}
	else if (State == ENWCombatState::Blocking)
	{
		State = ENWCombatState::Idle;
		if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(TEXT("BlockEnd"), TEXT("block")) : nullptr)
		{
			MulticastPlayMontage(Row->Montage.LoadSynchronous(), 1.f, 0);
		}
	}
}

const FNWActionRow* UNWCombatComponent::CheckAction(FName& Action) const
{
	if (!ActionTable || State == ENWCombatState::Dead || State == ENWCombatState::Stunned)
	{
		return nullptr;
	}
	const FNWActionRow* Row = ActionTable->FindRow<FNWActionRow>(Action, TEXT("action"));
	if (!Row || GetCooldownRemaining(Action) > 0.f)
	{
		return nullptr;
	}
	const bool bDodge = Action.ToString().StartsWith(TEXT("Dodge"));
	const bool bBlock = Action == TEXT("BlockStart");

	if (bExhausted && (bDodge || bBlock))
	{
		return nullptr;
	}
	if (State == ENWCombatState::Attacking)
	{
		// Mesmas janelas do jogo: esquiva/bloqueio cancelam o ataque; ataque só encadeia em CanTransition.
		const bool bOpen = bDodge ? GetCurve(NWCurves::CanDodgeCancel) > 0.5f
			: bBlock ? GetCurve(NWCurves::CanBlockCancel) > 0.5f && GetCurve(NWCurves::BlockDisabled) < 0.5f
			: GetCurve(NWCurves::CanTransition) > 0.5f;
		if (!bOpen)
		{
			return nullptr;
		}
		// Ataque leve repetido segue o combo.
		if (Action == LightComboStart && CurrentAction.ToString().StartsWith(TEXT("Light")))
		{
			const FNWActionRow* Cur = ActionTable->FindRow<FNWActionRow>(CurrentAction, TEXT("combo"));
			if (Cur && !Cur->ComboNext.IsNone())
			{
				Action = Cur->ComboNext;
				Row = ActionTable->FindRow<FNWActionRow>(Action, TEXT("combo"));
			}
		}
	}
	else if (State == ENWCombatState::Dodging)
	{
		if (GetCurve(NWCurves::CanTransition) < 0.5f && GetCurve(NWCurves::CanDodgeCancel) < 0.5f)
		{
			return nullptr;
		}
	}
	else if (State == ENWCombatState::Blocking && !bDodge)
	{
		// Do bloqueio só sai por esquiva ou soltando o botão.
		return nullptr;
	}
	if (!Row || (Row->StaminaCost > 0.f && Stamina <= 0.f))
	{
		return nullptr;
	}
	return Row;
}

bool UNWCombatComponent::TryStartAction(FName Action)
{
	const FNWActionRow* Row = CheckAction(Action);
	if (!Row)
	{
		return false;
	}
	StartAction(Action, *Row);
	return true;
}

void UNWCombatComponent::StartAction(FName Action, const FNWActionRow& Row)
{
	UAnimMontage* Montage = Row.Montage.LoadSynchronous();
	if (!Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("NW: montagem ausente para %s"), *Action.ToString());
		return;
	}
	SpendStamina(Row.StaminaCost);
	if (Row.BuffSeconds > 0.f && Row.BuffDamageReduction > 0.f)
	{
		DamageReductionUntil = ServerNow() + Row.BuffSeconds;
		DamageReductionAmount = Row.BuffDamageReduction;
	}
	if (Row.BuffSeconds > 0.f && Row.BuffDamageBonus > 0.f)
	{
		DamageBonusUntil = ServerNow() + Row.BuffSeconds;
		DamageBonusAmount = Row.BuffDamageBonus;
	}
	bProjectileFired = false;
	ActionStartedAt = ServerNow();
	if (Row.Cooldown > 0.f && !bNoCooldowns)
	{
		Cooldowns.RemoveAll([Action](const FNWCooldown& C) { return C.Action == Action; });
		Cooldowns.Add({Action, ServerNow() + Row.Cooldown, Row.Cooldown});
	}
	CurrentAction = Action;
	CurrentRow = Row;
	bDamageWindowWasOpen = false;
	CurrentMontage = Montage;
	BufferedAction = NAME_None;
	HitThisSwing.Reset();
	bSweepActive = false;
	const FString Name = Action.ToString();
	State = Name.StartsWith(TEXT("Dodge")) ? ENWCombatState::Dodging
		: Name.StartsWith(TEXT("Block")) ? ENWCombatState::Blocking
		: ENWCombatState::Attacking;
	MulticastPlayMontage(Montage, 1.f, ServerRequestId);
}

void UNWCombatComponent::MulticastPlayMontage_Implementation(UAnimMontage* Montage, float Rate, uint16 PredictionId)
{
	UAnimInstance* Anim = GetAnim();
	if (!Anim || !Montage)
	{
		return;
	}
	// O dono já está tocando esta ação desde o clique: não recomeça.
	if (PredictionId != 0 && PredictionId == PendingPrediction && IsLocalOwner() && !GetOwner()->HasAuthority())
	{
		PendingPrediction = 0;
		if (CVarDebugPrediction.GetValueOnGameThread())
		{
			UE_LOG(LogTemp, Display, TEXT("NW predição %u confirmada após %.0f ms%s"), PredictionId, (FPlatformTime::Seconds() - PredictedAt) * 1000.0,
				Anim->Montage_IsPlaying(Montage) ? TEXT("") : TEXT(" (servidor escolheu outra ação: corrigindo)"));
		}
		if (Anim->Montage_IsPlaying(Montage))
		{
			return;
		}
	}
	Anim->Montage_Play(Montage, Rate);
}

void UNWCombatComponent::MulticastPlayLoop_Implementation(UAnimMontage* Montage)
{
	UAnimInstance* Anim = GetAnim();
	if (Anim && Montage)
	{
		Anim->Montage_Play(Montage, 1.f);
		const FName Section = Montage->GetSectionName(0);
		Anim->Montage_SetNextSection(Section, Section, Montage);
	}
}

void UNWCombatComponent::PlayBlockIdle()
{
	if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(TEXT("BlockIdle"), TEXT("block")) : nullptr)
	{
		CurrentMontage = Row->Montage.LoadSynchronous();
		MulticastPlayLoop(CurrentMontage);
	}
}

void UNWCombatComponent::MulticastStopMontages_Implementation(float BlendOut)
{
	if (UAnimInstance* Anim = GetAnim())
	{
		Anim->Montage_Stop(BlendOut);
	}
}

void UNWCombatComponent::HandleMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	// Guarda levantada: o loop entra quando o "levantar" (ou a reação) começa a sair, em vez de depois
	// que ele terminou de voltar para a pose base (o escudo descia e subia de novo).
	if (GetOwner()->HasAuthority() && !bInterrupted && Montage == CurrentMontage && State == ENWCombatState::Blocking)
	{
		PlayBlockIdle();
	}
}

void UNWCombatComponent::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!GetOwner()->HasAuthority() || Montage != CurrentMontage)
	{
		return;
	}
	// Guarda levantada: terminou o "levantar" ou a reação ao bloqueio e o botão continua pressionado.
	if (State == ENWCombatState::Blocking && !bInterrupted)
	{
		PlayBlockIdle();
		return;
	}
	if (IsCharging())
	{
		return;
	}
	if (State == ENWCombatState::Attacking || State == ENWCombatState::Dodging)
	{
		State = ENWCombatState::Idle;
		CurrentAction = NAME_None;
		CurrentMontage = nullptr;
	}
}

// ---------------------------------------------------------------- tick

void UNWCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();

	if (State == ENWCombatState::Stunned && Now >= StunUntil)
	{
		if (CurrentAction == TEXT("StunLoop"))
		{
			MulticastStopMontages(0.25f);
			CurrentAction = NAME_None;
		}
		State = ENWCombatState::Idle;
	}
	if (State != ENWCombatState::Dead && State != ENWCombatState::Blocking && Now - LastStaminaSpend > StaminaRegenDelay)
	{
		Stamina = FMath::Min(MaxStamina, Stamina + StaminaRegenPerSecond * DeltaTime);
		// Sai da exaustão com metade da barra.
		if (bExhausted && Stamina >= MaxStamina * 0.5f)
		{
			bExhausted = false;
		}
	}
	if (IsCharging() && Now - ChargeStartedAt >= HeavyFullCharge)
	{
		// Carga cheia: o golpe sai sem esperar soltar (Test_AlwaysTrue no jogo).
		if (const FNWActionRow* Row = ActionTable->FindRow<FNWActionRow>(TEXT("HeavyCharged"), TEXT("charged")))
		{
			StartAction(TEXT("HeavyCharged"), *Row);
		}
	}
	else if (bAttackHeld && !IsCharging() && State == ENWCombatState::Attacking && BufferedAction.IsNone())
	{
		TryStartCharge();
	}
	if (!BufferedAction.IsNone() && State == ENWCombatState::Attacking)
	{
		const FName Pending = BufferedAction;
		if (TryStartAction(Pending))
		{
			BufferedAction = NAME_None;
		}
	}
	if (State == ENWCombatState::Attacking)
	{
		// Cada janela de dano que abre libera um novo acerto (ex.: o giratório tem duas).
		const bool bOpen = GetCurve(NWCurves::DamageWindow) > 0.5f || GetCurve(NWCurves::WeaponTrail) > 0.5f;
		if (bOpen && !bDamageWindowWasOpen)
		{
			HitThisSwing.Reset();
		}
		bDamageWindowWasOpen = bOpen;
		if (CurrentRow.HitShape == ENWHitShape::Projectile)
		{
			// Arremesso: o projétil sai quando a janela de dano abre (ou, sem curva, logo no começo do golpe).
			if (!bProjectileFired && (bOpen || ServerNow() - ActionStartedAt > 0.45f))
			{
				bProjectileFired = true;
				SpawnProjectile();
			}
		}
		else if (CurrentRow.HitShape == ENWHitShape::Blade)
		{
			SweepWeapon();
		}
		else if (bOpen)
		{
			AreaHit();
		}
	}
}

void UNWCombatComponent::SweepWeapon()
{
	const bool bWindow = GetCurve(NWCurves::DamageWindow) > 0.5f || GetCurve(NWCurves::WeaponTrail) > 0.5f;
	FVector Base, Tip;
	if (!bWindow || !BladeEnds(Base, Tip))
	{
		bSweepActive = false;
		return;
	}
	if (!bSweepActive)
	{
		PrevBase = Base;
		PrevTip = Tip;
		bSweepActive = true;
	}
	const bool bDebug = CVarNWDebugHits.GetValueOnGameThread() != 0;
	if (bDebug)
	{
		DrawDebugLine(GetWorld(), PrevBase, Base, FColor::Green, false, 1.5f, 0, 1.f);
		DrawDebugLine(GetWorld(), PrevTip, Tip, FColor::Green, false, 1.5f, 0, 1.f);
		DrawDebugLine(GetWorld(), Base, Tip, FColor(80, 255, 80), false, 1.5f, 0, 2.f);
	}
	// Pontos ao longo da lâmina, cada um varrido da posição do quadro anterior até a atual.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NWBlade), false, GetOwner());
	const int32 Samples = 5;
	for (int32 i = 0; i < Samples; ++i)
	{
		const float A = float(i) / (Samples - 1);
		const FVector From = FMath::Lerp(PrevBase, PrevTip, A);
		const FVector To = FMath::Lerp(Base, Tip, A);
		TArray<FHitResult> Hits;
		GetWorld()->SweepMultiByChannel(Hits, From, To, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(BladeRadius), Params);
		for (const FHitResult& Hit : Hits)
		{
			AActor* Other = Hit.GetActor();
			if (!Other || Other == GetOwner() || HitThisSwing.Contains(Other))
			{
				continue;
			}
			if (UNWCombatComponent* Target = Other->FindComponentByClass<UNWCombatComponent>())
			{
				HitThisSwing.Add(Other);
				ResolveHit(Target, Hit.ImpactPoint);
			}
		}
	}
	PrevBase = Base;
	PrevTip = Tip;
}

void UNWCombatComponent::AreaHit()
{
	const AActor* Self = GetOwner();
	const FVector Center = Self->GetActorLocation();
	const float Range = FMath::Max(CurrentRow.HitRange, 50.f);
	const bool bFront = CurrentRow.HitShape == ENWHitShape::Front;
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(CurrentRow.HitAngle, 1.f, 360.f) * 0.5f));
	if (CVarNWDebugHits.GetValueOnGameThread() != 0)
	{
		if (bFront)
		{
			DrawDebugCone(GetWorld(), Center, Self->GetActorForwardVector(), Range,
				FMath::DegreesToRadians(CurrentRow.HitAngle * 0.5f), FMath::DegreesToRadians(25.f), 12, FColor::Orange, false, 0.f, 0, 1.5f);
		}
		else
		{
			DrawDebugCircle(GetWorld(), Center, Range, 32, FColor::Orange, false, 0.f, 0, 1.5f, FVector::YAxisVector, FVector::XAxisVector, false);
		}
	}
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NWArea), false, Self);
	GetWorld()->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Range), Params);
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Other = O.GetActor();
		if (!Other || Other == Self || HitThisSwing.Contains(Other))
		{
			continue;
		}
		const FVector To = (Other->GetActorLocation() - Center).GetSafeNormal2D();
		if (bFront && FVector::DotProduct(Self->GetActorForwardVector().GetSafeNormal2D(), To) < CosHalf)
		{
			continue;
		}
		if (UNWCombatComponent* Target = Other->FindComponentByClass<UNWCombatComponent>())
		{
			HitThisSwing.Add(Other);
			ResolveHit(Target, Other->GetActorLocation());
		}
	}
}

// ---------------------------------------------------------------- dano

void UNWCombatComponent::ResolveHit(UNWCombatComponent* Target, const FVector& ImpactPoint)
{
	const FNWActionRow* Action = ActionTable ? ActionTable->FindRow<FNWActionRow>(CurrentAction, TEXT("hit")) : nullptr;
	const FNWDamageRow* Damage = (Action && DamageTable) ? DamageTable->FindRow<FNWDamageRow>(Action->DamageID, TEXT("hit")) : nullptr;
	FNWDamageRow Fallback;
	float Dealt = 0.f;
	const ENWHitResult Result = Target->ReceiveHit(this, Damage ? *Damage : Fallback, Dealt);
	if (CVarNWDebugHits.GetValueOnGameThread() != 0)
	{
		DrawDebugSphere(GetWorld(), ImpactPoint, BladeRadius, 10, Result == ENWHitResult::Hit ? FColor::Red : FColor::Cyan, false, 1.5f);
	}
	if (Result == ENWHitResult::Hit || Result == ENWHitResult::Blocked || Result == ENWHitResult::BlockBroken)
	{
		MulticastHitStop(Target->GetOwner(), HitStopSeconds);
	}
	if (Result == ENWHitResult::Hit && Action && Action->TargetStun > 0.f && Target->State != ENWCombatState::Dead)
	{
		Target->ApplyStun(Action->TargetStun);
	}
}

void UNWCombatComponent::SpawnProjectile()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	UStaticMeshComponent* Weapon = Cast<UStaticMeshComponent>(MainWeapon.Get());
	if (!Pawn)
	{
		return;
	}
	// Mira: direção da câmera para o jogador; para a IA, a frente do personagem.
	FRotator Aim = Pawn->GetActorRotation();
	if (Pawn->GetController() && Pawn->IsPlayerControlled())
	{
		Aim = Pawn->GetControlRotation();
	}
	const FVector Start = Pawn->GetActorLocation() + FVector(0.f, 0.f, 60.f) + Aim.Vector() * 60.f;
	const FTransform Where(Aim, Start);
	ANWProjectile* P = GetWorld()->SpawnActorDeferred<ANWProjectile>(ANWProjectile::StaticClass(), Where, GetOwner(), nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (P)
	{
		P->Setup(this, CurrentRow.DamageID, Weapon ? Weapon->GetStaticMesh() : nullptr, CurrentRow.HitRange);
		P->FinishSpawning(Where);
	}
}

void UNWCombatComponent::ProjectileHit(UNWCombatComponent* Target, FName DamageID, const FVector& Point)
{
	if (!Target || Target == this)
	{
		return;
	}
	const FNWDamageRow* Damage = DamageTable ? DamageTable->FindRow<FNWDamageRow>(DamageID, TEXT("projectile")) : nullptr;
	FNWDamageRow Fallback;
	float Dealt = 0.f;
	const ENWHitResult Result = Target->ReceiveHit(this, Damage ? *Damage : Fallback, Dealt);
	if (Result == ENWHitResult::Hit || Result == ENWHitResult::Blocked)
	{
		MulticastHitStop(Target->GetOwner(), HitStopSeconds);
	}
}

ENWHitResult UNWCombatComponent::ReceiveHit(UNWCombatComponent* Attacker, const FNWDamageRow& Damage, float& OutDamage)
{
	OutDamage = 0.f;
	if (State == ENWCombatState::Dead)
	{
		return ENWHitResult::Ignored;
	}
	// Membros do mesmo grupo não se ferem.
	const APawn* MePawn = Cast<APawn>(GetOwner());
	const APawn* ByPawn = Attacker ? Cast<APawn>(Attacker->GetOwner()) : nullptr;
	const ANWPlayerState* MePS = MePawn ? MePawn->GetPlayerState<ANWPlayerState>() : nullptr;
	if (MePS && ByPawn && MePS->IsGroupedWith(ByPawn->GetPlayerState<ANWPlayerState>()))
	{
		return ENWHitResult::Ignored;
	}
	if (IsInvulnerable())
	{
		MulticastShowHit(0.f, ENWHitResult::Dodged);
		OnHitTaken.Broadcast(0.f, ENWHitResult::Dodged, Attacker->GetOwner());
		return ENWHitResult::Dodged;
	}
	const FVector ToAttacker = (Attacker->GetOwner()->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal2D();
	const float FrontDot = FVector::DotProduct(GetOwner()->GetActorForwardVector(), ToAttacker);
	const bool bInArc = FrontDot >= FMath::Cos(FMath::DegreesToRadians(BlockArcDegrees * 0.5f));

	if (State == ENWCombatState::Blocking && bInArc && !Damage.Unblockable)
	{
		SpendStamina(BlockStaminaBase + BlockStaminaPerPower * Damage.BlockPowerLevel);
		if (Stamina <= 0.f)
		{
			PlayReaction(TEXT("BlockBreak"), 1.0f);
			MulticastShowHit(0.f, ENWHitResult::BlockBroken);
			OnHitTaken.Broadcast(0.f, ENWHitResult::BlockBroken, Attacker->GetOwner());
			return ENWHitResult::BlockBroken;
		}
		if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(TEXT("BlockReact"), TEXT("block")) : nullptr)
		{
			// Vira a montagem corrente: quando acabar, HandleMontageEnded volta para a guarda em loop.
			CurrentMontage = Row->Montage.LoadSynchronous();
			MulticastPlayMontage(CurrentMontage, 1.f, 0);
		}
		MulticastShowHit(0.f, ENWHitResult::Blocked);
		OnHitTaken.Broadcast(0.f, ENWHitResult::Blocked, Attacker->GetOwner());
		return ENWHitResult::Blocked;
	}

	OutDamage = Attacker->WeaponBaseDamage * Damage.DmgCoef * (1.f - ArmorMitigation) * (1.f + Attacker->GetDamageBonus());
	// Arma equipada: gear score escala o dano (ANWCharacter com inventário; NPCs ficam em 1).
	if (const ANWCharacter* By = Cast<ANWCharacter>(Attacker->GetOwner()); By && By->Inventory && By->WeaponSets.IsValidIndex(By->ActiveWeapon))
	{
		OutDamage *= By->Inventory->DamageMultiplier(By->WeaponSets[By->ActiveWeapon].Id);
	}
	if (ServerNow() < DamageReductionUntil)
	{
		OutDamage *= 1.f - DamageReductionAmount;
	}
	if (bGodMode)
	{
		OutDamage = 0.f;
	}
	Health = FMath::Max(0.f, Health - OutDamage);
	MulticastShowHit(OutDamage, ENWHitResult::Hit);
	OnHitTaken.Broadcast(OutDamage, ENWHitResult::Hit, Attacker->GetOwner());
	if (Health <= 0.f)
	{
		Die(Attacker->GetOwner());
		return ENWHitResult::Hit;
	}

	const bool bGrit = GetCurve(NWCurves::Grit) > 0.5f;
	if (!bGrit && Damage.PowerLevel >= Poise)
	{
		PlayReaction(DirectionalAction(TEXT("Hit_"), ToAttacker), Damage.HitStun * SecondsPerHitStun);
		if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
		{
			Char->LaunchCharacter(-ToAttacker * Damage.ImpactDistance * 400.f, true, false);
		}
	}
	return ENWHitResult::Hit;
}

void UNWCombatComponent::ApplyStun(float Seconds)
{
	State = ENWCombatState::Stunned;
	StunUntil = FMath::Max(StunUntil, GetWorld()->GetTimeSeconds() + Seconds);
	CurrentAction = TEXT("StunLoop");
	BufferedAction = NAME_None;
	bAttackHeld = false;
	if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(TEXT("StunLoop"), TEXT("stun"), false) : nullptr)
	{
		CurrentMontage = Row->Montage.LoadSynchronous();
		MulticastPlayLoop(CurrentMontage);
	}
}

void UNWCombatComponent::PlayDraw()
{
	if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(TEXT("Draw"), TEXT("draw"), false) : nullptr)
	{
		StartAction(TEXT("Draw"), *Row);
	}
}

void UNWCombatComponent::HealFull()
{
	if (State == ENWCombatState::Dead)
	{
		return;
	}
	Health = MaxHealth;
	Stamina = MaxStamina;
	bExhausted = false;
}

void UNWCombatComponent::PlayReaction(FName Action, float StunSeconds)
{
	State = ENWCombatState::Stunned;
	StunUntil = GetWorld()->GetTimeSeconds() + FMath::Max(StunSeconds, 0.2f);
	CurrentAction = Action;
	BufferedAction = NAME_None;
	if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(Action, TEXT("reaction")) : nullptr)
	{
		CurrentMontage = Row->Montage.LoadSynchronous();
		MulticastPlayMontage(CurrentMontage, 1.f, 0);
	}
}

FName UNWCombatComponent::DirectionalAction(const TCHAR* Prefix, const FVector& WorldDir) const
{
	// WorldDir aponta do personagem para a origem do golpe.
	const FVector Local = GetOwner()->GetActorTransform().InverseTransformVectorNoScale(WorldDir);
	const TCHAR* Side = FMath::Abs(Local.X) >= FMath::Abs(Local.Y) ? (Local.X >= 0 ? TEXT("F") : TEXT("B")) : (Local.Y >= 0 ? TEXT("R") : TEXT("L"));
	return FName(FString(Prefix) + Side);
}

void UNWCombatComponent::MulticastShowHit_Implementation(float Damage, ENWHitResult Result)
{
	const FVector At = GetOwner()->GetActorLocation() + FVector(0, 0, 110);
	FString Text;
	FColor Color = FColor::White;
	switch (Result)
	{
	case ENWHitResult::Hit: Text = FString::Printf(TEXT("%.0f"), Damage); Color = FColor::Orange; break;
	case ENWHitResult::Blocked: Text = TEXT("Bloqueado"); Color = FColor::Cyan; break;
	case ENWHitResult::BlockBroken: Text = NWText::Get(TEXT("nw.hit.guard_broken")); Color = FColor::Yellow; break;
	case ENWHitResult::Dodged: Text = TEXT("Esquivou"); Color = FColor::Green; break;
	default: return;
	}
	DrawDebugString(GetWorld(), At, Text, nullptr, Color, 1.2f, true, 1.4f);
}

void UNWCombatComponent::MulticastHitStop_Implementation(AActor* Other, float Seconds)
{
	AActor* Self = GetOwner();
	Self->CustomTimeDilation = HitStopDilation;
	if (Other)
	{
		Other->CustomTimeDilation = HitStopDilation;
	}
	// O timer usa o tempo do mundo, que não é afetado pela dilatação do ator.
	TWeakObjectPtr<AActor> WeakSelf(Self), WeakOther(Other);
	GetWorld()->GetTimerManager().SetTimer(HitStopTimer, [WeakSelf, WeakOther]()
	{
		if (WeakSelf.IsValid()) WeakSelf->CustomTimeDilation = 1.f;
		if (WeakOther.IsValid()) WeakOther->CustomTimeDilation = 1.f;
	}, Seconds, false);
}

void UNWCombatComponent::SpendStamina(float Amount)
{
	if (Amount <= 0.f || bInfiniteStamina)
	{
		return;
	}
	Stamina = FMath::Max(0.f, Stamina - Amount);
	LastStaminaSpend = GetWorld()->GetTimeSeconds();
	if (Stamina <= 0.f)
	{
		bExhausted = true;
	}
}

static FString KillerName(const AActor* Killer)
{
	const APawn* Pawn = Cast<APawn>(Killer);
	if (const APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr)
	{
		return PS->GetPlayerName();
	}
	if (Killer && Killer->GetClass()->GetName().Contains(TEXT("Lost")))
	{
		return TEXT("Lost");
	}
	return Killer ? NWText::Key(TEXT("nw.dummy")) : FString();
}

void UNWCombatComponent::Die(AActor* Killer)
{
	KilledBy = KillerName(Killer);
	State = ENWCombatState::Dead;
	// Lost morto por jogador: saque para quem matou (e o grupo).
	if (GetOwner()->GetClass()->GetName().Contains(TEXT("Lost")))
	{
		if (ANWGameMode* GM = GetWorld()->GetAuthGameMode<ANWGameMode>())
		{
			GM->GrantLoot(Cast<APawn>(Killer), GetOwner(), TEXT("lost"));
		}
	}
	// Morto não anda (o modo de movimento é replicado: vale no cliente também).
	if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		Char->GetCharacterMovement()->DisableMovement();
	}
	CurrentAction = TEXT("Death");
	BufferedAction = NAME_None;
	if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(TEXT("Death"), TEXT("death")) : nullptr)
	{
		CurrentMontage = Row->Montage.LoadSynchronous();
		MulticastPlayMontage(CurrentMontage, 1.f, 0);
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsPlayerControlled())
	{
		RespawnAt = ServerNow() + RespawnDelay;   // o jogador escolhe quando, pela tela de morte
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &UNWCombatComponent::Respawn, RespawnDelay, false);
}

void UNWCombatComponent::ServerRequestRespawn_Implementation()
{
	if (State != ENWCombatState::Dead || ServerNow() < RespawnAt)
	{
		return;
	}
	// Jogador renasce no ponto de partida do mapa (o assentamento), não onde morreu.
	APawn* Pawn = Cast<APawn>(GetOwner());
	AGameModeBase* GM = GetWorld()->GetAuthGameMode();
	if (const AActor* Start = (Pawn && GM) ? GM->FindPlayerStart(Pawn->GetController()) : nullptr)
	{
		SpawnTransform = FTransform(FRotator(0.f, Start->GetActorRotation().Yaw, 0.f), Start->GetActorLocation());
	}
	Respawn();
}

void UNWCombatComponent::Respawn()
{
	MulticastStopMontages(0.2f);
	GetOwner()->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		Char->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	Health = MaxHealth;
	Stamina = MaxStamina;
	bExhausted = false;
	State = ENWCombatState::Idle;
	CurrentAction = NAME_None;
	CurrentMontage = nullptr;
}

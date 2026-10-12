#include "NWCombatComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Curves/CurveFloat.h"
#include "UObject/Package.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NWCreature.h"
#include "NWBrainComponent.h"
#include "NWFxActor.h"

#include "NWHUD.h"
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
// O jogo não treme a câmera em golpe normal de espada (só crítico); o tranco curto é um extra do port.
static TAutoConsoleVariable<int32> CVarNWHitKick(TEXT("nw.HitKick"), 1, TEXT("Tranco extra na câmera ao acertar/apanhar (0 = só as tremidas do jogo)"));
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
	DOREPLIFETIME(UNWCombatComponent, Statuses);
	DOREPLIFETIME(UNWCombatComponent, bAiming);
	DOREPLIFETIME(UNWCombatComponent, bAimWeapon);
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
	Anim->Montage_Play(Montage, 1.f, EMontagePlayReturnType::MontageLength, Row->StartAt * Montage->GetPlayLength());
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
	if (!GetOwner()->HasAuthority() && IsLocalOwner() && !bAimWeapon)
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
	if (!GetOwner()->HasAuthority() && IsLocalOwner() && !bAimWeapon && CurrentAction == ChargeAction && LocalChargeStart > 0.0)
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

void UNWCombatComponent::RequestDirectional(const FString& Prefix, FVector2D LocalInput)
{
	FString Side = TEXT("F");
	if (!LocalInput.IsNearlyZero())
	{
		Side = FMath::Abs(LocalInput.X) >= FMath::Abs(LocalInput.Y) ? (LocalInput.X > 0 ? TEXT("F") : TEXT("B")) : (LocalInput.Y > 0 ? TEXT("R") : TEXT("L"));
	}
	RequestAction(FName(*(Prefix + Side)));
}

void UNWCombatComponent::RequestBlock(bool bBlock)
{
	ServerRequestBlock(bBlock);
}

void UNWCombatComponent::RequestAim(bool bAim)
{
	ServerRequestAim(bAim);
}

void UNWCombatComponent::ServerRequestAim_Implementation(bool bAim)
{
	if (!bAimWeapon || bAim == bAiming || State == ENWCombatState::Dead)
	{
		return;
	}
	bAiming = bAim;
	bAimClickBuffered = false;
	if (bAim)
	{
		// Levanta o arco (ReadyWeapon, 0,433 s) e fica no loop de mira.
		if (State == ENWCombatState::Idle)
		{
			if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(TEXT("AimStart"), TEXT("aim"), false) : nullptr)
			{
				StartAction(TEXT("AimStart"), *Row);
			}
		}
		return;
	}
	// Soltou a mira: abaixa o arco (do tiro ou da mira) se nada mais estiver tocando.
	if (IsCharging())
	{
		State = ENWCombatState::Idle;
		CurrentAction = NAME_None;
	}
	if (State == ENWCombatState::Idle || CurrentAction == TEXT("AimStart") || CurrentAction == TEXT("Reload") || CurrentAction == TEXT("ReloadCharged"))
	{
		const FName End = CurrentAction == TEXT("ReloadCharged") ? FName(TEXT("AimEndFromShotCharged"))
			: CurrentAction == TEXT("Reload") ? FName(TEXT("AimEndFromShot")) : FName(TEXT("AimEnd"));
		State = ENWCombatState::Idle;
		CurrentAction = NAME_None;
		if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(End, TEXT("aim"), false) : nullptr)
		{
			MulticastPlayMontage(Row->Montage.LoadSynchronous(), 1.f, 0, 0.f);
		}
	}
}

void UNWCombatComponent::PlayAimIdle()
{
	if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(TEXT("AimIdle"), TEXT("aim"), false) : nullptr)
	{
		CurrentMontage = Row->Montage.LoadSynchronous();
		MulticastPlayLoop(CurrentMontage);
	}
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
	if (bAimWeapon && bPressed && !bAiming)
	{
		// Arco sem mira: tiro rápido no clique (BowAttack_Hipshot).
		bStarted = TryStartAction(LightComboStart);
		if (!bStarted && State == ENWCombatState::Attacking)
		{
			BufferedAction = LightComboStart;
		}
	}
	else if (bAimWeapon && bPressed && bAiming)
	{
		// Mirando: segurar puxa (Charge_Bow); se ainda não pode (recarga, levantando o arco), o clique fica guardado e
		// vira tiro sem carga quando Shoot_NoCharge_Bow abrir (Tick).
		bStarted = TryStartCharge();
		bAimClickBuffered = !bStarted;
	}
	else if (bAimWeapon && !bPressed && !IsCharging())
	{
		bStarted = false;
	}
	else if (bPressed)
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
	// Soltar cedo continua a sequência da ação em curso (combo leve, Evadir → ataque, Brandir → Terminar).
	const FName From = State == ENWCombatState::Attacking && !CurrentRow.ComboNext.IsNone() ? CurrentAction : NAME_None;
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
	if (bAimWeapon)
	{
		// Arco: tiro normal antes da puxada cheia (0,5 s), carregado depois.
		const FName Shot = Held < AimFullCharge ? FName(TEXT("AimShot")) : FName(TEXT("ChargedShot"));
		if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(Shot, TEXT("release"), false) : nullptr)
		{
			StartAction(Shot, *Row);
		}
		return;
	}
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
		LastBlockRelease = GetWorld()->GetTimeSeconds();
		if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(TEXT("BlockEnd"), TEXT("block")) : nullptr)
		{
			MulticastPlayMontage(Row->Montage.LoadSynchronous(), 1.f, 0, 0.f);
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
	if (!Row || GetCooldownRemaining(CooldownKey(Action, *Row)) > 0.f)
	{
		return nullptr;
	}
	// Evadir (rapieira): sai de qualquer ação, inclusive habilidades e a guarda.
	if (Row->bCancelsAny && State != ENWCombatState::Stunned)
	{
		return Row;
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
		// Janela própria da ação (a do jogo) ou a genérica de encadear.
		const bool bOpen = bDodge ? GetCurve(NWCurves::CanDodgeCancel) > 0.5f
			: bBlock ? GetCurve(NWCurves::CanBlockCancel) > 0.5f && GetCurve(NWCurves::BlockDisabled) < 0.5f
			: (!Row->TransitionCurve.IsNone() && GetCurve(Row->TransitionCurve) > 0.5f) || GetCurve(NWCurves::CanTransition) > 0.5f;
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
	if (Row.SelfMoveSeconds > 0.f && Row.SelfMoveSpeed != 0.f)
	{
		ApplyStatus(FName(*(Action.ToString() + TEXT("_Self"))), Row.SelfMoveSeconds, 0.f, 0.f, 1, Row.SelfMoveSpeed, this);
	}
	ShotsFired = 0;
	DamageWindowIndex = 0;
	bGroundFired = false;
	CounterUntil = Row.CounterSeconds > 0.f ? GetWorld()->GetTimeSeconds() + Row.CounterSeconds : CounterUntil;
	ActionStartedAt = ServerNow();
	// Chuva de Flechas: o ponto mirado é fixado no começo (raio da câmera até GroundRange, no chão).
	if (Row.GroundRadius > 0.f)
	{
		FVector ViewLoc;
		FRotator ViewRot;
		const APawn* Pawn = Cast<APawn>(GetOwner());
		if (Pawn && Pawn->GetController())
		{
			Pawn->GetController()->GetPlayerViewPoint(ViewLoc, ViewRot);
		}
		else
		{
			ViewLoc = GetOwner()->GetActorLocation() + FVector(0, 0, 150);
			ViewRot = GetOwner()->GetActorRotation();
		}
		FHitResult Hit;
		const FVector End = ViewLoc + ViewRot.Vector() * FMath::Max(Row.GroundRange, 500.f);
		FCollisionQueryParams P(SCENE_QUERY_STAT(NWGround), false, GetOwner());
		GroundTarget = GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc, End, ECC_Visibility, P) ? Hit.Location : End;
		if (GetWorld()->LineTraceSingleByChannel(Hit, GroundTarget + FVector(0, 0, 200), GroundTarget - FVector(0, 0, 3000), ECC_Visibility, P))
		{
			GroundTarget = Hit.Location;
		}
		// Flechas caindo na área, chegando no momento do acerto.
		ANWFxActor::Spawn(GetWorld(), ENWFx::ArrowRain, GroundTarget, Row.GroundRadius, FMath::Max(Row.GroundDelay, 0.3f));
	}
	if (Row.Cooldown > 0.f && !bNoCooldowns)
	{
		const FName Key = CooldownKey(Action, Row);
		Cooldowns.RemoveAll([Key](const FNWCooldown& C) { return C.Action == Key; });
		Cooldowns.Add({Key, ServerNow() + Row.Cooldown, Row.Cooldown});
	}
	CurrentAction = Action;
	CurrentRow = Row;
	++ActionSerial;
	bDamageWindowWasOpen = false;
	CurrentMontage = Montage;
	BufferedAction = NAME_None;
	HitThisSwing.Reset();
	bSweepActive = false;
	const FString Name = Action.ToString();
	State = Name.StartsWith(TEXT("Dodge")) ? ENWCombatState::Dodging
		: Name.StartsWith(TEXT("Block")) ? ENWCombatState::Blocking
		: ENWCombatState::Attacking;
	MulticastPlayMontage(Montage, 1.f, ServerRequestId, Row.StartAt);
}

void UNWCombatComponent::MulticastPlayMontage_Implementation(UAnimMontage* Montage, float Rate, uint16 PredictionId, float StartAt)
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
	Anim->Montage_Play(Montage, Rate, EMontagePlayReturnType::MontageLength, StartAt * Montage->GetPlayLength());
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
		return;
	}
	// Encadeamentos começam quando a montagem começa a sair (não depois que ela terminou de voltar à pose base):
	// tiro → recarga → mira, cair → levantar, postura → saída. Sem isso o arco baixava um instante entre eles.
	if (!GetOwner()->HasAuthority() || bInterrupted || Montage != CurrentMontage)
	{
		return;
	}
	if (State == ENWCombatState::Stunned && !ReactionNext.IsNone())
	{
		HandleMontageEnded(Montage, false);
		return;
	}
	if ((State == ENWCombatState::Attacking || State == ENWCombatState::Dodging) && !IsCharging()
		&& (!CurrentRow.NextAction.IsNone() || (bAiming && bAimWeapon)))
	{
		HandleMontageEnded(Montage, false);
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
	// Reação encadeada (cair → levantar).
	if (State == ENWCombatState::Stunned && !ReactionNext.IsNone())
	{
		const FName Next = ReactionNext;
		ReactionNext = NAME_None;
		if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(Next, TEXT("reaction"), false) : nullptr)
		{
			CurrentAction = Next;
			CurrentMontage = Row->Montage.LoadSynchronous();
			MulticastPlayMontage(CurrentMontage, 1.f, 0, 0.f);
			if (CurrentMontage)
			{
				StunUntil = FMath::Max(StunUntil, GetWorld()->GetTimeSeconds() + CurrentMontage->GetPlayLength() / FMath::Max(CurrentMontage->RateScale, 0.01f));
			}
		}
		return;
	}
	if (State == ENWCombatState::Attacking || State == ENWCombatState::Dodging)
	{
		// Encadeamento da linha (tiro → recarga; postura do Ripostar → saída).
		const FName Next = CurrentRow.NextAction;
		State = ENWCombatState::Idle;
		CurrentAction = NAME_None;
		CurrentMontage = nullptr;
		if (!bInterrupted && !Next.IsNone())
		{
			if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(Next, TEXT("next"), false) : nullptr)
			{
				StartAction(Next, *Row);
				return;
			}
		}
		if (bAiming)
		{
			PlayAimIdle();
		}
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
	// Regenera depois do atraso (gasto ou guarda solta); com a guarda levantada, na taxa de bloqueio.
	if (State != ENWCombatState::Dead && Now - FMath::Max(LastStaminaSpend, LastBlockRelease) > StaminaRegenDelay)
	{
		const float Rate = State == ENWCombatState::Blocking ? StaminaRegenBlocking : StaminaRegenPerSecond;
		Stamina = FMath::Min(MaxStamina, Stamina + Rate * DeltaTime);
		// Exausto até a barra encher (ação Exhausted do jogo).
		if (bExhausted && Stamina >= MaxStamina)
		{
			bExhausted = false;
		}
	}
	TickStatuses(Now);
	if (IsCharging() && !bAimWeapon && Now - ChargeStartedAt >= HeavyFullCharge)
	{
		// Carga cheia: o golpe sai sem esperar soltar (Test_AlwaysTrue no jogo).
		if (const FNWActionRow* Row = ActionTable->FindRow<FNWActionRow>(TEXT("HeavyCharged"), TEXT("charged")))
		{
			StartAction(TEXT("HeavyCharged"), *Row);
		}
	}
	else if (bAimWeapon && bAiming && !IsCharging() && (bAttackHeld || bAimClickBuffered) && BufferedAction.IsNone())
	{
		// Arco mirando (Bow_AimChargeShoot do jogo): botão segurado puxa quando Charge_Bow abre (ou parado na mira);
		// clique guardado atira sem carga quando Shoot_NoCharge_Bow abre.
		const bool bIdle = State == ENWCombatState::Idle;
		if (bAttackHeld && (bIdle || GetCurve(TEXT("Charge_Bow")) > 0.5f) && TryStartCharge())
		{
			bAimClickBuffered = false;
		}
		else if (!bAttackHeld && bAimClickBuffered && (bIdle || GetCurve(TEXT("Shoot_NoCharge_Bow")) > 0.5f))
		{
			bAimClickBuffered = false;
			if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(TEXT("AimShot"), TEXT("aimshot"), false) : nullptr)
			{
				StartAction(TEXT("AimShot"), *Row);
			}
		}
	}
	else if (bAimWeapon && !bAiming && bAttackHeld && BufferedAction.IsNone())
	{
		// Arco sem mira com o botão segurado: tiros rápidos seguidos (CanAttack_Bow_HipFire do jogo, em 1,0 s).
		TryStartAction(LightComboStart);
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
			++DamageWindowIndex;
		}
		bDamageWindowWasOpen = bOpen;
		if (CurrentRow.HitShape == ENWHitShape::Projectile)
		{
			// Disparos nos tempos da linha (FireTimes: arco, Tiro Rápido com 3); sem tempos, quando a janela de dano
			// abre (ou, sem curva, logo no começo do golpe).
			TArray<FString> Times, Ids;
			CurrentRow.FireTimes.ParseIntoArray(Times, TEXT(";"));
			CurrentRow.FireDamageIDs.ParseIntoArray(Ids, TEXT(";"));
			const float T = ServerNow() - ActionStartedAt;
			if (Times.Num() == 0)
			{
				if (ShotsFired == 0 && (bOpen || T > 0.45f))
				{
					++ShotsFired;
					SpawnProjectile(CurrentRow.DamageID);
				}
			}
			else
			{
				while (ShotsFired < Times.Num() && T >= FCString::Atof(*Times[ShotsFired]))
				{
					SpawnProjectile(Ids.IsValidIndex(ShotsFired) ? FName(*Ids[ShotsFired]) : CurrentRow.DamageID);
					++ShotsFired;
				}
			}
		}
		else if (CurrentRow.GroundRadius > 0.f)
		{
			// Chuva de Flechas: um acerto na área do chão depois do atraso.
			if (!bGroundFired && ServerNow() - ActionStartedAt >= CurrentRow.GroundDelay)
			{
				bGroundFired = true;
				FireGround();
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
	// Golpe corpo a corpo com várias janelas e linhas diferentes (FireDamageIDs na ordem das janelas).
	FName DamageID = Action ? Action->DamageID : NAME_None;
	if (Action && !Action->FireDamageIDs.IsEmpty() && Action->HitShape != ENWHitShape::Projectile)
	{
		TArray<FString> Ids;
		Action->FireDamageIDs.ParseIntoArray(Ids, TEXT(";"));
		if (Ids.IsValidIndex(DamageWindowIndex - 1))
		{
			DamageID = FName(*Ids[DamageWindowIndex - 1]);
		}
	}
	const FNWDamageRow* Damage = (Action && DamageTable) ? DamageTable->FindRow<FNWDamageRow>(DamageID, TEXT("hit")) : nullptr;
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
		Target->ApplyStun(Action->TargetStun, Action->bStunBreaksOnDamage);
	}
	if (Result == ENWHitResult::Hit && Action && Target->State != ENWCombatState::Dead)
	{
		// Consumir antes de aplicar (Floreio e Finalização: consome o sangramento e não aplica de novo).
		if (!Action->ConsumeStatusID.IsNone())
		{
			// Terminar: consome as hemorragias e causa 110% do dano que elas ainda fariam.
			const float Left = Target->ConsumeStatus(Action->ConsumeStatusID) * (1.f - Target->ArmorMitigation);
			if (Left > 0.f && Action->ConsumeDamage > 0.f)
			{
				Target->Health = FMath::Max(0.f, Target->Health - Left * Action->ConsumeDamage);
				Target->MulticastShowHit(Left * Action->ConsumeDamage, ENWHitResult::Hit, GetOwner()->GetActorLocation(),
					false, true, GetOwner(), NAME_None, NAME_None);
				if (Target->Health <= 0.f)
				{
					Target->Die(GetOwner());
				}
			}
		}
		if (!Action->StatusID.IsNone() && Target->State != ENWCombatState::Dead)
		{
			Target->ApplyStatus(Action->StatusID, Action->StatusSeconds, Action->StatusTick, WeaponBaseDamage * Action->StatusDamage,
				Action->StatusMaxStacks, Action->StatusMoveSpeed, this);
		}
	}
}

void UNWCombatComponent::SpawnProjectile(FName DamageID)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	UStaticMeshComponent* Weapon = Cast<UStaticMeshComponent>(MainWeapon.Get());
	if (!Pawn)
	{
		return;
	}
	// Jogador: mira pelo centro da câmera (ponto que o raio da câmera acerta, até o alcance); IA: frente do corpo.
	const float Range = CurrentRow.HitRange > 0.f ? CurrentRow.HitRange : 10000.f;
	FVector Start = Pawn->GetActorLocation() + FVector(0.f, 0.f, 60.f) + Pawn->GetActorForwardVector() * 60.f;
	FRotator Aim = Pawn->GetActorRotation();
	if (Pawn->GetController() && Pawn->IsPlayerControlled())
	{
		FVector ViewLoc;
		FRotator ViewRot;
		Pawn->GetController()->GetPlayerViewPoint(ViewLoc, ViewRot);
		FHitResult Hit;
		FCollisionQueryParams P(SCENE_QUERY_STAT(NWAim), false, GetOwner());
		const FVector Far = ViewLoc + ViewRot.Vector() * Range;
		const FVector Target = GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc + ViewRot.Vector() * 150.f, Far, ECC_Visibility, P) ? Hit.ImpactPoint : Far;
		Start = Pawn->GetActorLocation() + FVector(0.f, 0.f, 70.f) + ViewRot.Vector() * 50.f;
		Aim = (Target - Start).Rotation();
	}
	else if (const ANWCharacter* Me = Cast<ANWCharacter>(Pawn); Me && Me->Brain && Me->Brain->GetTarget())
	{
		// IA: mira no peito do alvo.
		Aim = (Me->Brain->GetTarget()->GetActorLocation() + FVector(0.f, 0.f, 40.f) - Start).Rotation();
	}
	const FTransform Where(Aim, Start);
	ANWProjectile* P = GetWorld()->SpawnActorDeferred<ANWProjectile>(ANWProjectile::StaticClass(), Where, GetOwner(), nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (P)
	{
		FNWProjectileParams Params;
		Params.Speed = CurrentRow.ProjectileSpeed;
		Params.Radius = CurrentRow.ProjectileRadius;
		Params.GravityDelay = CurrentRow.GravityDelay;
		Params.bPierce = CurrentRow.bPierce;
		Params.ExplodeRadius = CurrentRow.ExplodeRadius;
		Params.ExplodeDamageID = CurrentRow.ExplodeDamageID;
		Params.CloudRadius = CurrentRow.CloudRadius;
		Params.CloudSeconds = CurrentRow.CloudSeconds;
		Params.StatusID = CurrentRow.StatusID;
		Params.StatusSeconds = CurrentRow.StatusSeconds;
		Params.StatusTick = CurrentRow.StatusTick;
		Params.StatusDamage = WeaponBaseDamage * CurrentRow.StatusDamage;
		Params.Range = Range;
		// Arco: a malha é a flecha (o arco fica na mão); arremesso: a própria arma.
		UStaticMesh* Mesh = bAimWeapon ? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/NW/sword_shield/Mesh/SM_arrow.SM_arrow"))
			: (Weapon ? Weapon->GetStaticMesh().Get() : nullptr);
		P->Setup(this, DamageID, Mesh, Params);
		P->FinishSpawning(Where);
	}
}

void UNWCombatComponent::ExplodeAt(const FVector& Point, float Radius, FName DamageID, AActor* Skip)
{
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NWExplode), false, GetOwner());
	GetWorld()->OverlapMultiByChannel(Overlaps, Point, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Params);
	if (CVarNWDebugHits.GetValueOnGameThread() != 0)
	{
		DrawDebugSphere(GetWorld(), Point, Radius, 16, FColor::Orange, false, 2.f);
	}
	TSet<AActor*> Done;
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Other = O.GetActor();
		if (!Other || Other == GetOwner() || Done.Contains(Other))
		{
			continue;
		}
		Done.Add(Other);
		if (UNWCombatComponent* Target = Other->FindComponentByClass<UNWCombatComponent>())
		{
			ProjectileHit(Target, DamageID, Other->GetActorLocation());
		}
	}
}

void UNWCombatComponent::FireGround()
{
	// Área no chão (cilindro de GroundRadius): um acerto por alvo.
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NWGroundHit), false, GetOwner());
	const float R = CurrentRow.GroundRadius;
	GetWorld()->OverlapMultiByChannel(Overlaps, GroundTarget + FVector(0, 0, 250.f), FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(R, 250.f + R), Params);
	if (CVarNWDebugHits.GetValueOnGameThread() != 0)
	{
		DrawDebugCylinder(GetWorld(), GroundTarget, GroundTarget + FVector(0, 0, 500.f), R, 32, FColor::Orange, false, 2.f);
	}
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Other = O.GetActor();
		if (!Other || Other == GetOwner() || HitThisSwing.Contains(Other) || FVector::Dist2D(Other->GetActorLocation(), GroundTarget) > R)
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

void UNWCombatComponent::ProjectileHit(UNWCombatComponent* Target, FName DamageID, const FVector& Point, bool bHead, float Falloff)
{
	if (!Target || Target == this)
	{
		return;
	}
	const FNWDamageRow* Damage = DamageTable ? DamageTable->FindRow<FNWDamageRow>(DamageID, TEXT("projectile")) : nullptr;
	FNWDamageRow Fallback;
	// Falloff: multiplicador de dano vindo do projétil (queda com a distância; hoje 1).
	FNWDamageRow Scaled = Damage ? *Damage : Fallback;
	Scaled.DmgCoef *= Falloff;
	Damage = &Scaled;
	float Dealt = 0.f;
	const ENWHitResult Result = Target->ReceiveHit(this, Damage ? *Damage : Fallback, Dealt, bHead);
	if (Result == ENWHitResult::Hit || Result == ENWHitResult::Blocked)
	{
		MulticastHitStop(Target->GetOwner(), HitStopSeconds);
	}
}

ENWHitResult UNWCombatComponent::ReceiveHit(UNWCombatComponent* Attacker, const FNWDamageRow& Damage, float& OutDamage, bool bHead)
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
	AActor* By = Attacker->GetOwner();
	const bool bByCreature = By->IsA<ANWCreature>();
	if (IsInvulnerable())
	{
		MulticastShowHit(0.f, ENWHitResult::Dodged, By->GetActorLocation(), bByCreature, false, By, NAME_None, NAME_None);
		OnHitTaken.Broadcast(0.f, ENWHitResult::Dodged, By);
		return ENWHitResult::Dodged;
	}
	// Ripostar: na postura (Rapier_Riposte_Timer) não toma dano nem reage; contra-ataca quem bateu.
	if (State == ENWCombatState::Attacking && GetWorld()->GetTimeSeconds() < CounterUntil && !CurrentRow.CounterAction.IsNone())
	{
		const FName Counter = CurrentRow.CounterAction;
		CounterUntil = 0.f;
		if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(Counter, TEXT("counter"), false) : nullptr)
		{
			GetOwner()->SetActorRotation(FRotator(0.f, (By->GetActorLocation() - GetOwner()->GetActorLocation()).Rotation().Yaw, 0.f));
			StartAction(Counter, *Row);
		}
		MulticastShowHit(0.f, ENWHitResult::Blocked, By->GetActorLocation(), bByCreature, false, By, NAME_None, NAME_None);
		return ENWHitResult::Blocked;
	}
	// Atordoamento do Ripostar acaba no primeiro dano.
	if (State == ENWCombatState::Stunned && bStunBreaksOnDamage)
	{
		StunUntil = 0.f;
		bStunBreaksOnDamage = false;
	}
	const FVector ToAttacker = (By->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal2D();
	const float FrontDot = FVector::DotProduct(GetOwner()->GetActorForwardVector(), ToAttacker);
	const bool bInArc = FrontDot >= FMath::Cos(FMath::DegreesToRadians(BlockArcDegrees * 0.5f));
	const bool bTargetAI = !MePawn || !MePawn->IsPlayerControlled();
	const FVector Away = -ToAttacker;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, By->GetActorForwardVector().GetSafeNormal2D());

	if (State == ENWCombatState::Blocking && bInArc && !Damage.Unblockable)
	{
		// Perda = BlockStaminaDamage (arma de quem bate) × BlockStaminaDmgMod (golpe) × (1 − estabilidade), mínimo 10%.
		const float Mod = Damage.BlockStaminaDmgMod > 0.f ? Damage.BlockStaminaDmgMod : 1.f;
		SpendStamina(Attacker->BlockStaminaDamage * Mod * FMath::Max(1.f - BlockStability / 100.f, 0.1f));
		if (Stamina <= 0.f)
		{
			PlayReaction(TEXT("BlockBreak"), 1.0f);
			MulticastShowHit(0.f, ENWHitResult::BlockBroken, By->GetActorLocation(), bByCreature, false, By, Damage.BlockCameraShakeID, Damage.BlockTargetCameraShakeID);
			OnHitTaken.Broadcast(0.f, ENWHitResult::BlockBroken, By);
			return ENWHitResult::BlockBroken;
		}
		if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(TEXT("BlockReact"), TEXT("block")) : nullptr)
		{
			// Vira a montagem corrente: quando acabar, HandleMontageEnded volta para a guarda em loop.
			CurrentMontage = Row->Montage.LoadSynchronous();
			MulticastPlayMontage(CurrentMontage, 1.f, 0, 0.f);
		}
		MulticastShowHit(0.f, ENWHitResult::Blocked, By->GetActorLocation(), bByCreature, false, By, Damage.BlockCameraShakeID, Damage.BlockTargetCameraShakeID);
		OnHitTaken.Broadcast(0.f, ENWHitResult::Blocked, By);
		return ENWHitResult::Blocked;
	}

	// Crítico: sorteio pela chance da arma ou golpe pelas costas (atacante a 135° ou mais da frente do alvo).
	const bool bBackstab = !Damage.NoBackstab && FrontDot <= FMath::Cos(FMath::DegreesToRadians(135.f));
	const bool bCrit = bBackstab || (Attacker->CritChance > 0.f && FMath::FRand() < Attacker->CritChance);
	float Coef = Damage.DmgCoef;
	if (bHead && !Damage.NoHeadshot && Damage.DmgCoefHead > 0.f)
	{
		Coef *= Damage.DmgCoefHead;
	}
	if (bCrit)
	{
		Coef *= Damage.DmgCoefCrit > 0.f ? Damage.DmgCoefCrit : Attacker->CritDamageMultiplier;
	}
	OutDamage = Attacker->WeaponBaseDamage * Coef * (1.f - ArmorMitigation) * (1.f + Attacker->GetDamageBonus());
	// Arma equipada: gear score escala o dano (ANWCharacter com inventário; NPCs ficam em 1).
	if (const ANWCharacter* ByChar = Cast<ANWCharacter>(By); ByChar && ByChar->Inventory && ByChar->WeaponSets.IsValidIndex(ByChar->ActiveWeapon))
	{
		OutDamage *= ByChar->Inventory->DamageMultiplier(ByChar->WeaponSets[ByChar->ActiveWeapon].Id);
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
	MulticastShowHit(OutDamage, ENWHitResult::Hit, By->GetActorLocation(), bByCreature, bCrit, By,
		bCrit && !Damage.CritCameraShakeID.IsNone() ? Damage.CritCameraShakeID : Damage.CameraShakeID,
		bCrit && !Damage.CritTargetCameraShakeID.IsNone() ? Damage.CritTargetCameraShakeID : Damage.TargetCameraShakeID);
	OnHitTaken.Broadcast(OutDamage, ENWHitResult::Hit, By);
	if (Health <= 0.f)
	{
		Die(By);
		return ENWHitResult::Hit;
	}

	// Reação (§4): PowerLevel é a categoria da reação, não força contra poise.
	auto Pick = [bCrit, bTargetAI](float Normal, float AI, float Crit) { return bCrit && Crit > 0.f ? Crit : bTargetAI && AI > 0.f ? AI : Normal; };
	const float PL = Pick(Damage.PowerLevel, Damage.PowerLevel_AI, Damage.CritPowerLevel);
	const float Stun = Pick(Damage.HitStun, Damage.HitStun_AI, Damage.CritHitStun);
	const float Now = GetWorld()->GetTimeSeconds();
	bool bReact = GetCurve(NWCurves::Grit) < 0.5f;
	if (bReact && !bTargetAI)
	{
		// Jogador: PL 0 só estremece; 1 a 3 interrompe; 4 ou mais derruba.
		bReact = PL >= 1.f;
	}
	else if (bReact)
	{
		// IA: reage a tudo, salvo stagger resistido (dano de stagger < resistência do corpo, que no meio de um ataque
		// vale × StaggerResistModifier da linha dele) e o limitador de 2 s depois de uma reação (golpes leves).
		const float StaggerDmg = Attacker->BaseStaggerDamage * (Damage.StaggerDmgModifier > 0.f ? Damage.StaggerDmgModifier : 1.f) * (bCrit ? 1.5f : 1.f);
		float Resist = StaggerResistRating;
		if (State == ENWCombatState::Attacking && DamageTable && !CurrentRow.DamageID.IsNone())
		{
			if (const FNWDamageRow* Mine = DamageTable->FindRow<FNWDamageRow>(CurrentRow.DamageID, TEXT("stagger"), false))
			{
				Resist *= Mine->StaggerResistModifier;
			}
		}
		const bool bLimited = Now < ReactionLimiterUntil && Damage.AttackType == TEXT("Light");
		bReact = StaggerDmg >= Resist && !bLimited;
	}
	if (bReact)
	{
		const float Seconds = ReactionBaseSeconds + Stun * SecondsPerHitStun + (PL >= 4.f ? 0.8f : 0.f);
		// Animação pela categoria (tabela de reação do jogo): R1 leve, R2, R3 forte, R4 cai e levanta. Quem não tem a
		// animação daquela categoria (Lost, bichos) usa a R2.
		const TCHAR* Prefix = PL >= 4.f ? TEXT("Down_") : PL >= 3.f ? TEXT("Hit3_") : PL >= 2.f ? TEXT("Hit_") : TEXT("Hit1_");
		FName Reaction = DirectionalAction(Prefix, ToAttacker);
		if (!ActionTable || !ActionTable->FindRow<FNWActionRow>(Reaction, TEXT("reaction"), false))
		{
			Reaction = DirectionalAction(TEXT("Hit_"), ToAttacker);
		}
		PlayReaction(Reaction, Seconds);
		if (bTargetAI)
		{
			ReactionLimiterUntil = Now + 2.f;
		}
	}
	// Empurrão (§2): para trás (Y) e para o lado de quem bate (X); com reação decai por quadro, sem reação é o
	// "Pushable" do jogo (velocidade constante por 0,4 s).
	const float Back = Pick(Damage.ImpactDistance, Damage.ImpactDistance_AI, Damage.CritImpactDistance);
	const FVector Dir = Away * Back + Right * Damage.ImpactDistanceX;
	if (!Dir.IsNearlyZero())
	{
		Push(Dir.GetSafeNormal2D(), Dir.Size2D() * ImpactDistanceScale, Damage.ImpactDecayRate, !bReact);
	}
	return ENWHitResult::Hit;
}

namespace
{
	// Curva de força do empurrão: r^k por quadro de 1/30 s, no tempo normalizado (0..1) do root motion.
	UCurveFloat* DecayCurve(float R, int32 Frames)
	{
		static TMap<int32, TObjectPtr<UCurveFloat>> Cache;
		const int32 Key = FMath::RoundToInt(R * 1000.f) * 100 + Frames;
		if (TObjectPtr<UCurveFloat>* Found = Cache.Find(Key))
		{
			return *Found;
		}
		UCurveFloat* C = NewObject<UCurveFloat>(GetTransientPackage());
		C->AddToRoot();
		for (int32 k = 0; k <= Frames; ++k)
		{
			C->FloatCurve.AddKey(float(k) / Frames, FMath::Pow(R, float(k)));
		}
		Cache.Add(Key, C);
		return C;
	}
}

void UNWCombatComponent::Push(const FVector& Dir, float Meters, float DecayRate, bool bNoDecay)
{
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Move = Char ? Char->GetCharacterMovement() : nullptr;
	if (!Move || Meters <= 0.f)
	{
		return;
	}
	// Root motion colado ao chão (LaunchCharacter punha o alvo "caindo", sem atrito: em descida ele voava longe).
	constexpr float Frame = 1.f / 30.f;
	TSharedPtr<FRootMotionSource_ConstantForce> P = MakeShared<FRootMotionSource_ConstantForce>();
	P->InstanceName = TEXT("NWKnockback");
	P->AccumulateMode = ERootMotionAccumulateMode::Override;
	P->Priority = 5;
	P->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	P->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	const float R = FMath::Clamp(DecayRate, 0.f, 1.f);
	if (bNoDecay || R >= 0.999f)
	{
		P->Duration = 0.4f;
		P->Force = Dir * (Meters * 100.f / P->Duration);
	}
	else
	{
		// v_k = v0·r^k até cair a 1%; v0 tal que a soma dos quadros dá a distância toda.
		const int32 Frames = R <= 0.f ? 1 : FMath::Clamp(FMath::CeilToInt(FMath::Loge(0.01f) / FMath::Loge(R)), 1, 45);
		float Sum = 0.f;
		for (int32 k = 0; k < Frames; ++k)
		{
			Sum += FMath::Pow(R, float(k));
		}
		P->Duration = Frames * Frame;
		P->Force = Dir * (Meters * 100.f / (Frame * Sum));
		if (Frames > 1)
		{
			P->StrengthOverTime = DecayCurve(R, Frames);
		}
	}
	Move->ApplyRootMotionSource(P);
}

void UNWCombatComponent::ApplyStun(float Seconds, bool bBreakOnDamage)
{
	bStunBreaksOnDamage = bBreakOnDamage;
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
	ReactionNext = NAME_None;
	if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(Action, TEXT("reaction")) : nullptr)
	{
		CurrentMontage = Row->Montage.LoadSynchronous();
		MulticastPlayMontage(CurrentMontage, 1.f, 0, 0.f);
		// Queda (R4): fica no chão até o levantar terminar.
		ReactionNext = Row->NextAction;
		if (!ReactionNext.IsNone() && CurrentMontage)
		{
			// +0,3 s: o fim da montagem (que chama o levantar) chega antes de o atordoamento acabar.
			StunUntil = FMath::Max(StunUntil, GetWorld()->GetTimeSeconds() + CurrentMontage->GetPlayLength() / FMath::Max(CurrentMontage->RateScale, 0.01f) + 0.3f);
		}
	}
}

FName UNWCombatComponent::DirectionalAction(const TCHAR* Prefix, const FVector& WorldDir) const
{
	// WorldDir aponta do personagem para a origem do golpe.
	const FVector Local = GetOwner()->GetActorTransform().InverseTransformVectorNoScale(WorldDir);
	const TCHAR* Side = FMath::Abs(Local.X) >= FMath::Abs(Local.Y) ? (Local.X >= 0 ? TEXT("F") : TEXT("B")) : (Local.Y >= 0 ? TEXT("R") : TEXT("L"));
	return FName(FString(Prefix) + Side);
}

void UNWCombatComponent::MulticastShowHit_Implementation(float Damage, ENWHitResult Result, FVector_NetQuantize From, bool bByCreature,
	bool bCrit, AActor* By, FName ByShake, FName TargetShake)
{
	bLastHitByCreature = bByCreature;
	// Tremidas da tabela de dano, cada uma na câmera de quem é local.
	if (ANWCharacter* Me = Cast<ANWCharacter>(GetOwner()); Me && Me->IsLocallyControlled() && !TargetShake.IsNone())
	{
		Me->PlayCameraShake(TargetShake);
	}
	if (ANWCharacter* Hitter = Cast<ANWCharacter>(By); Hitter && Hitter->IsLocallyControlled() && !ByShake.IsNone())
	{
		Hitter->PlayCameraShake(ByShake);
	}
	// Número de dano na HUD do jogador local (como o damagenumbers do jogo); servidor dedicado não desenha.
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	ANWHUD* HUD = PC ? Cast<ANWHUD>(PC->GetHUD()) : nullptr;
	if (!HUD)
	{
		return;
	}
	const FVector At = GetOwner()->GetActorLocation() + FVector(0, 0, 110);
	// Dano em mim: vermelho; dano que eu ou outros causam: claro; bloqueio e esquiva: texto do port.
	const bool bMe = GetOwner() == PC->GetPawn();
	if (bMe && (Result == ENWHitResult::Hit || Result == ENWHitResult::BlockBroken))
	{
		HUD->AddHitIndicator(From, MaxHealth > 0.f ? Damage / MaxHealth : 0.f);
		// Extra do port (nw.HitKick 0 desliga): tranco na câmera proporcional ao dano (10% da vida = 1).
		if (ANWCharacter* Me = Cast<ANWCharacter>(GetOwner()); Me && CVarNWHitKick.GetValueOnGameThread() != 0)
		{
			Me->AddCameraKick(FMath::Clamp(0.5f + (MaxHealth > 0.f ? 5.f * Damage / MaxHealth : 0.f), 0.5f, 1.5f));
		}
	}
	switch (Result)
	{
	case ENWHitResult::Hit:
		// Crítico: dourado e grande.
		HUD->AddDamageNumber(At, FString::Printf(TEXT("%.0f"), Damage), bMe ? FLinearColor(FColor(0xFF, 0x5A, 0x4A))
			: bCrit ? FLinearColor(FColor(0xED, 0xC6, 0x3E)) : FLinearColor(FColor(0xF2, 0xEC, 0xDF)), bCrit || Damage >= 150.f);
		break;
	case ENWHitResult::Blocked: HUD->AddDamageNumber(At, NWText::Get(TEXT("nw.hit.blocked")), FLinearColor(FColor(0x9A, 0xB8, 0xC8)), false); break;
	case ENWHitResult::BlockBroken: HUD->AddDamageNumber(At, NWText::Get(TEXT("nw.hit.guard_broken")), FLinearColor(FColor(0xED, 0xC6, 0x3E)), true); break;
	case ENWHitResult::Dodged: HUD->AddDamageNumber(At, NWText::Get(TEXT("nw.hit.dodged")), FLinearColor(FColor(0x9A, 0x96, 0x8E)), false); break;
	default: break;
	}
}

void UNWCombatComponent::MulticastHitStop_Implementation(AActor* Other, float Seconds)
{
	AActor* Self = GetOwner();
	Self->CustomTimeDilation = HitStopDilation;
	// Extra do port (nw.HitKick): quem acertou sente o golpe também, tranco leve na câmera do jogador local.
	if (ANWCharacter* Me = Cast<ANWCharacter>(Self); Me && Me->IsLocallyControlled() && CVarNWHitKick.GetValueOnGameThread() != 0)
	{
		Me->AddCameraKick(0.35f);
	}
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

void UNWCombatComponent::ApplyStatus(FName Id, float Seconds, float Tick, float DamagePerStack, int32 MaxStacks, float MoveSpeed, UNWCombatComponent* Source)
{
	if (Id.IsNone() || Seconds <= 0.f || State == ENWCombatState::Dead)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	FNWStatus* S = Statuses.FindByPredicate([Id](const FNWStatus& X) { return X.Id == Id; });
	if (!S)
	{
		S = &Statuses.AddDefaulted_GetRef();
		S->Id = Id;
		S->Stacks = 0;
		S->NextTick = Now + Tick;
	}
	// Reaplicar renova a duração e soma um acúmulo até o máximo.
	S->Stacks = FMath::Clamp(S->Stacks + 1, 1, FMath::Max(1, MaxStacks));
	S->EndTime = Now + Seconds;
	S->Tick = Tick;
	S->DamagePerStack = DamagePerStack;
	S->MoveSpeed = MoveSpeed;
	S->Source = Source;
}

int32 UNWCombatComponent::GetStatusStacks(FName Id) const
{
	const FNWStatus* S = Statuses.FindByPredicate([Id](const FNWStatus& X) { return X.Id == Id; });
	return S ? S->Stacks : 0;
}

float UNWCombatComponent::ConsumeStatus(FName Id)
{
	// Dano que o status ainda causaria: acúmulos × dano por tique × tiques restantes.
	float Remaining = 0.f;
	if (const FNWStatus* S = Statuses.FindByPredicate([Id](const FNWStatus& X) { return X.Id == Id; }); S && S->Tick > 0.f)
	{
		const int32 Ticks = FMath::Max(0, FMath::CeilToInt((S->EndTime - GetWorld()->GetTimeSeconds()) / S->Tick));
		Remaining = S->DamagePerStack * S->Stacks * Ticks;
	}
	Statuses.RemoveAll([Id](const FNWStatus& X) { return X.Id == Id; });
	return Remaining;
}

float UNWCombatComponent::GetMoveSpeedMultiplier() const
{
	float M = 1.f;
	for (const FNWStatus& S : Statuses)
	{
		M *= FMath::Max(0.1f, 1.f + S.MoveSpeed);
	}
	return M;
}

void UNWCombatComponent::TickStatuses(float Now)
{
	for (int32 i = Statuses.Num() - 1; i >= 0; --i)
	{
		FNWStatus& S = Statuses[i];
		// Dano por tique (sangramento, veneno): sem reação, número na cor do status.
		if (S.Tick > 0.f && S.DamagePerStack > 0.f && Now >= S.NextTick && State != ENWCombatState::Dead)
		{
			S.NextTick += S.Tick;
			const float Dmg = bGodMode ? 0.f : S.DamagePerStack * S.Stacks * (1.f - ArmorMitigation);
			Health = FMath::Max(0.f, Health - Dmg);
			UNWCombatComponent* Src = S.Source.Get();
			MulticastShowHit(Dmg, ENWHitResult::Hit, Src ? Src->GetOwner()->GetActorLocation() : GetOwner()->GetActorLocation(),
				false, false, nullptr, NAME_None, NAME_None);
			if (Health <= 0.f)
			{
				Die(Src ? Src->GetOwner() : nullptr);
				Statuses.Reset();
				return;
			}
		}
		if (Now >= S.EndTime)
		{
			Statuses.RemoveAt(i);
		}
	}
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
	if (const ANWCreature* Creature = Cast<ANWCreature>(Killer); Creature && Creature->Def())
	{
		return TEXT("#") + Creature->Def()->NameKey;   // nome do jogo, traduzido no cliente (NWText::Resolve)
	}
	if (Killer && Killer->GetClass()->GetName().Contains(TEXT("Lost")))
	{
		return TEXT("Lost");
	}
	return Killer ? NWText::Key(TEXT("nw.dummy")) : FString();
}

void UNWCombatComponent::Die(AActor* Killer)
{
	Statuses.Reset();
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
		MulticastPlayMontage(CurrentMontage, 1.f, 0, 0.f);
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

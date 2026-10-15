#include "NWCombatComponent.h"
#include "NWManaComponent.h"
#include "NWEvents.h"
#include "NWRules.h"
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
#include "Animation/AnimSequenceBase.h"
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

// nw.DebugHits 1: draws the blade sweep (green) and the hits (red/cyan) to tune the hitbox.
// The game does not shake the camera on a normal sword hit (only on crits); the short kick is a port extra.
static TAutoConsoleVariable<int32> CVarNWHitKick(TEXT("nw.HitKick"), 1, TEXT("Extra camera kick when hitting/getting hit (0 = only the game's shakes)"));
static TAutoConsoleVariable<int32> CVarNWDebugHits(TEXT("nw.DebugHits"), 0, TEXT("Draws the blade hitbox and the hits"));

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

float UNWCombatComponent::GetActionCurve(FName Curve) const
{
	const UAnimInstance* Anim = GetAnim();
	const FAnimMontageInstance* Inst = Anim && CurrentMontage ? Anim->GetActiveInstanceForMontage(CurrentMontage) : nullptr;
	if (!Inst)
	{
		return GetCurve(Curve);
	}
	const float Pos = Inst->GetPosition();
	for (const FSlotAnimationTrack& Track : CurrentMontage->SlotAnimTracks)
	{
		for (const FAnimSegment& Seg : Track.AnimTrack.AnimSegments)
		{
			const UAnimSequenceBase* Seq = Seg.GetAnimReference();
			if (Seq && Seg.IsInRange(Pos))
			{
				return Seq->EvaluateCurveData(Curve, FAnimExtractContext(double(Seg.ConvertTrackPosToAnimPos(Pos))));
			}
		}
	}
	return 0.f;
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
	// Blade = longest axis of the mesh's local box; the base is the end closest to the origin (the grip).
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

// ---------------------------------------------------------------- requests

static TAutoConsoleVariable<int32> CVarDebugPrediction(TEXT("nw.DebugPrediction"), 0, TEXT("Logs predicted, confirmed and rejected actions."));

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
	// Local state equal to what the server will replicate; if it rejects, ClientRejectPrediction undoes it.
	const FString Name = Action.ToString();
	State = Name.StartsWith(TEXT("Dodge")) ? ENWCombatState::Dodging : Name.StartsWith(TEXT("Block")) ? ENWCombatState::Blocking : ENWCombatState::Attacking;
	CurrentAction = Action;
	CurrentMontage = Montage;
	Anim->Montage_Play(Montage, 1.f, EMontagePlayReturnType::MontageLength, Row->StartAt * Montage->GetPlayLength());
	PendingPrediction = Id;
	PredictedAt = FPlatformTime::Seconds();
	if (CVarDebugPrediction.GetValueOnGameThread())
	{
		UE_LOG(LogTemp, Display, TEXT("NW prediction %u: %s"), Id, *Action.ToString());
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
		UE_LOG(LogTemp, Display, TEXT("NW prediction %u rejected after %.0f ms"), PredictionId, (FPlatformTime::Seconds() - PredictedAt) * 1000.0);
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
	// Released during the predicted charge: predicts light/heavy from the hold time (below the automatic charged one).
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
			// From charge to strike there is no window to check: only the row and the stamina.
			const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(Action, TEXT("release")) : nullptr;
			if (Row && (Row->StaminaCost <= 0.f || Stamina > 0.f))
			{
				const ENWCombatState Saved = State;
				State = ENWCombatState::Idle;   // CheckAction from inside the charge would reject it
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
		// Raises the bow (ReadyWeapon, 0.433 s) and stays in the aim loop.
		if (State == ENWCombatState::Idle)
		{
			if (const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(TEXT("AimStart"), TEXT("aim"), false) : nullptr)
			{
				StartAction(TEXT("AimStart"), *Row);
			}
		}
		return;
	}
	// Aim released: lowers the bow (from the shot or the aim) if nothing else is playing.
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
			// Outside the window: buffered until CanTransition (or CanDodgeCancel, for the dodge) turns on.
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
		// Bow without aim: quick shot on click (BowAttack_Hipshot).
		bStarted = TryStartAction(LightComboStart);
		if (!bStarted && State == ENWCombatState::Attacking)
		{
			BufferedAction = LightComboStart;
		}
	}
	else if (bAimWeapon && bPressed && bAiming)
	{
		// Aiming: holding draws (Charge_Bow); if it cannot yet (reload, raising the bow), the click is buffered and
		// becomes an uncharged shot when Shoot_NoCharge_Bow opens (Tick).
		bStarted = TryStartCharge();
		bAimClickBuffered = !bStarted;
	}
	else if (bAimWeapon && !bPressed && !IsCharging())
	{
		bStarted = false;
	}
	else if (bPressed && State == ENWCombatState::Attacking && !CurrentRow.AttackAction.IsNone())
	{
		bStarted = TryStartAction(CurrentRow.AttackAction);
	}
	else if (bPressed)
	{
		// Outside the window (mid-strike), the charge starts in Tick when it opens, if the button is still held.
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
	// Releasing early continues the sequence of the current action (light combo, Evade → attack, Flourish → Finish).
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
		// Bow: normal shot before the full draw (0.5 s), charged after.
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
		// Tap: light attack, following the combo in progress.
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
	// Evade (Rapier): leaves any action, including abilities and the guard.
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
		// Same windows as the game: dodge/block cancel the attack; attack only chains on CanTransition.
		// The action's own window (the game's) or the generic chaining one.
		const bool bOpen = bDodge ? GetActionCurve(NWCurves::CanDodgeCancel) > 0.5f
			: bBlock ? GetActionCurve(NWCurves::CanBlockCancel) > 0.5f && GetActionCurve(NWCurves::BlockDisabled) < 0.5f
			: (!Row->TransitionCurve.IsNone() && GetActionCurve(Row->TransitionCurve) > 0.5f) || GetActionCurve(NWCurves::CanTransition) > 0.5f;
		if (!bOpen)
		{
			return nullptr;
		}
		// Repeated light attack follows the combo.
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
		if (GetActionCurve(NWCurves::CanTransition) < 0.5f && GetActionCurve(NWCurves::CanDodgeCancel) < 0.5f)
		{
			return nullptr;
		}
	}
	else if (State == ENWCombatState::Blocking && !bDodge)
	{
		// Blocking is only left by dodging or releasing the button.
		return nullptr;
	}
	if (!Row || (Row->StaminaCost > 0.f && Stamina <= 0.f))
	{
		return nullptr;
	}
	// IfManaCost GreaterThanOrEqual (magic weapons, docs/game/weapons/ice-gauntlet.md §2).
	const UNWManaComponent* Mana = Row->ManaCost > 0.f ? GetOwner()->FindComponentByClass<UNWManaComponent>() : nullptr;
	if (Mana && !Mana->CanPay(Row->ManaCost))
	{
		return nullptr;
	}
	return Row;
}

bool UNWCombatComponent::ForceAction(FName Action)
{
	const FNWActionRow* Row = GetOwner()->HasAuthority() && ActionTable && State != ENWCombatState::Dead
		? ActionTable->FindRow<FNWActionRow>(Action, TEXT("force"), false) : nullptr;
	if (!Row)
	{
		return false;
	}
	StartAction(Action, *Row);
	return CurrentAction == Action;
}

void UNWCombatComponent::StartCooldown(FName Key, float Seconds)
{
	if (!GetOwner()->HasAuthority() || bNoCooldowns || Seconds <= 0.f)
	{
		return;
	}
	Cooldowns.RemoveAll([Key](const FNWCooldown& C) { return C.Action == Key; });
	Cooldowns.Add({Key, ServerNow() + Seconds, Seconds});
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
		UE_LOG(LogTemp, Warning, TEXT("NW: missing montage for %s"), *Action.ToString());
		return;
	}
	if (Row.ManaCost > 0.f)
	{
		UNWManaComponent* Mana = GetOwner()->FindComponentByClass<UNWManaComponent>();
		if (Mana && !Mana->CanPay(Row.ManaCost))
		{
			return;
		}
		if (Mana)
		{
			Mana->Pay(Row.ManaCost);
		}
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
	// Rain of Arrows: the aimed point is fixed at the start (camera ray up to GroundRange, on the ground).
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
		// Arrows falling on the area, arriving at the moment of the hit.
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
	// Action that chains into itself (gauntlet channels: wind, tomb): the montage loops on its own until another action
	// comes in. Restarting the same montage would interrupt the previous one, and the interruption ends the action.
	if (Row.NextAction == Action)
	{
		MulticastPlayLoop(Montage);
		return;
	}
	MulticastPlayMontage(Montage, 1.f, ServerRequestId, Row.StartAt);
}

void UNWCombatComponent::MulticastPlayMontage_Implementation(UAnimMontage* Montage, float Rate, uint16 PredictionId, float StartAt)
{
	UAnimInstance* Anim = GetAnim();
	if (!Anim || !Montage)
	{
		return;
	}
	// The owner has been playing this action since the click: do not restart.
	if (PredictionId != 0 && PredictionId == PendingPrediction && IsLocalOwner() && !GetOwner()->HasAuthority())
	{
		PendingPrediction = 0;
		if (CVarDebugPrediction.GetValueOnGameThread())
		{
			UE_LOG(LogTemp, Display, TEXT("NW prediction %u confirmed after %.0f ms%s"), PredictionId, (FPlatformTime::Seconds() - PredictedAt) * 1000.0,
				Anim->Montage_IsPlaying(Montage) ? TEXT("") : TEXT(" (server chose another action: correcting)"));
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
	// Guard up: the loop starts when the "raise" (or the reaction) starts blending out, instead of after
	// it finished returning to the base pose (the shield went down and up again).
	if (GetOwner()->HasAuthority() && !bInterrupted && Montage == CurrentMontage && State == ENWCombatState::Blocking)
	{
		PlayBlockIdle();
		return;
	}
	// Chains start when the montage starts blending out (not after it finished returning to the base pose):
	// shot → reload → aim, fall → get up, stance → exit. Without this the bow dropped for an instant between them.
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
	// Guard up: the "raise" or the block reaction ended and the button is still held.
	if (State == ENWCombatState::Blocking && !bInterrupted)
	{
		PlayBlockIdle();
		return;
	}
	if (IsCharging())
	{
		return;
	}
	// Chained reaction (fall → get up).
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
		// Row chaining (shot → reload; Riposte stance → exit).
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
	// Regenerates after the delay (spend or guard released); with the guard up, at the blocking rate.
	if (State != ENWCombatState::Dead && Now - FMath::Max(LastStaminaSpend, LastBlockRelease) > StaminaRegenDelay)
	{
		const float Rate = State == ENWCombatState::Blocking ? StaminaRegenBlocking : StaminaRegenPerSecond;
		Stamina = FMath::Min(MaxStamina, Stamina + Rate * DeltaTime);
		// Exhausted until the bar fills (the game's Exhausted action).
		if (bExhausted && Stamina >= MaxStamina)
		{
			bExhausted = false;
		}
	}
	TickStatuses(Now);
	if (IsCharging() && !bAimWeapon && Now - ChargeStartedAt >= HeavyFullCharge)
	{
		// Full charge: the strike goes off without waiting for the release (Test_AlwaysTrue in the game).
		if (const FNWActionRow* Row = ActionTable->FindRow<FNWActionRow>(TEXT("HeavyCharged"), TEXT("charged")))
		{
			StartAction(TEXT("HeavyCharged"), *Row);
		}
	}
	else if (bAimWeapon && bAiming && !IsCharging() && (bAttackHeld || bAimClickBuffered) && BufferedAction.IsNone())
	{
		// Bow aiming (the game's Bow_AimChargeShoot): a held button draws when Charge_Bow opens (or standing in aim);
		// a buffered click shoots uncharged when Shoot_NoCharge_Bow opens.
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
		// Bow without aim with the button held: rapid consecutive shots (the game's CanAttack_Bow_HipFire, at 1.0 s).
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
		// Each damage window that opens allows a new hit (e.g. the spin has two).
		const bool bOpen = GetCurve(NWCurves::DamageWindow) > 0.5f || GetCurve(NWCurves::WeaponTrail) > 0.5f;
		if (bOpen && !bDamageWindowWasOpen)
		{
			HitThisSwing.Reset();
			++DamageWindowIndex;
		}
		bDamageWindowWasOpen = bOpen;
		if (CurrentRow.HitShape == ENWHitShape::Projectile)
		{
			// Shots at the row's times (FireTimes: bow, Rapid Shot with 3); without times, when the damage window
			// opens (or, without a curve, right at the start of the strike).
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
			// Rain of Arrows: one hit in the ground area after the delay.
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
	// Points along the blade, each swept from its position in the previous frame to the current one.
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

// ---------------------------------------------------------------- damage

void UNWCombatComponent::ResolveHit(UNWCombatComponent* Target, const FVector& ImpactPoint)
{
	const FNWActionRow* Action = ActionTable ? ActionTable->FindRow<FNWActionRow>(CurrentAction, TEXT("hit")) : nullptr;
	// Melee strike with several windows and different rows (FireDamageIDs in window order).
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
		MulticastHitStop(Target->GetOwner(), FNWRules::Num(TEXT("combate.hit_stop_s"), 0.0));
	}
	if (Result == ENWHitResult::Hit && Action && Action->TargetStun > 0.f && Target->State != ENWCombatState::Dead)
	{
		Target->ApplyStun(Action->TargetStun, Action->bStunBreaksOnDamage);
	}
	if (Result == ENWHitResult::Hit && Action && Target->State != ENWCombatState::Dead)
	{
		// Consume before applying (Flourish and Finish: consumes the bleed and does not apply it again).
		if (!Action->ConsumeStatusID.IsNone())
		{
			// Finish: consumes the bleeds and deals 110% of the damage they would still deal.
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
	// Player: aims through the camera center (point the camera ray hits, up to the range); AI: front of the body.
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
		// AI: aims at the target's chest.
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
		Params.FxPath = CurrentRow.ProjectileFx;
		// Bow: the mesh is the arrow (the bow stays in hand); throw: the weapon itself.
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
	// Ground area (cylinder of GroundRadius): one hit per target.
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
	// Falloff: damage multiplier coming from the projectile (drop-off with distance; currently 1).
	FNWDamageRow Scaled = Damage ? *Damage : Fallback;
	Scaled.DmgCoef *= Falloff;
	Damage = &Scaled;
	float Dealt = 0.f;
	const ENWHitResult Result = Target->ReceiveHit(this, Damage ? *Damage : Fallback, Dealt, bHead);
	if (Result == ENWHitResult::Hit || Result == ENWHitResult::Blocked)
	{
		MulticastHitStop(Target->GetOwner(), FNWRules::Num(TEXT("combate.hit_stop_s"), 0.0));
	}
}

ENWHitResult UNWCombatComponent::ReceiveHit(UNWCombatComponent* Attacker, const FNWDamageRow& Damage, float& OutDamage, bool bHead)
{
	OutDamage = 0.f;
	if (State == ENWCombatState::Dead)
	{
		return ENWHitResult::Ignored;
	}
	// Members of the same group do not hurt each other.
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
	// Riposte: in the stance (Rapier_Riposte_Timer) takes no damage and does not react; counterattacks the attacker.
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
	// Riposte stun ends on the first damage.
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
		// Loss = BlockStaminaDamage (attacker's weapon) × BlockStaminaDmgMod (strike) × (1 − stability), minimum 10%.
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
			// Becomes the current montage: when it ends, HandleMontageEnded returns to the looping guard.
			CurrentMontage = Row->Montage.LoadSynchronous();
			MulticastPlayMontage(CurrentMontage, 1.f, 0, 0.f);
		}
		MulticastShowHit(0.f, ENWHitResult::Blocked, By->GetActorLocation(), bByCreature, false, By, Damage.BlockCameraShakeID, Damage.BlockTargetCameraShakeID);
		OnHitTaken.Broadcast(0.f, ENWHitResult::Blocked, By);
		return ENWHitResult::Blocked;
	}

	// Crit: roll on the weapon's chance, or backstab (attacker at 135° or more from the target's front).
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
	// Equipped weapon: gear score scales the damage (ANWCharacter with inventory; NPCs stay at 1).
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

	// Reaction (§4): PowerLevel is the reaction category, not force against poise.
	auto Pick = [bCrit, bTargetAI](float Normal, float AI, float Crit) { return bCrit && Crit > 0.f ? Crit : bTargetAI && AI > 0.f ? AI : Normal; };
	const float PL = Pick(Damage.PowerLevel, Damage.PowerLevel_AI, Damage.CritPowerLevel);
	const float Stun = Pick(Damage.HitStun, Damage.HitStun_AI, Damage.CritHitStun);
	const float Now = GetWorld()->GetTimeSeconds();
	bool bReact = GetCurve(NWCurves::Grit) < 0.5f;
	if (bReact && !bTargetAI)
	{
		// Player: PL 0 only flinches; 1 to 3 interrupts; 4 or more knocks down.
		bReact = PL >= 1.f;
	}
	else if (bReact)
	{
		// AI: reacts to everything, except resisted stagger (stagger damage < body resistance, which mid-attack
		// is × the StaggerResistModifier of its row) and the 2 s limiter after a reaction (light strikes).
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
	float ReactionSeconds = 0.f;
	if (bReact)
	{
		// Animation by category (the game's reaction table): R1 light, R2, R3 heavy, R4 falls and gets up. Whoever lacks the
		// animation for that category (Lost, beasts) uses R2.
		const TCHAR* Prefix = PL >= 4.f ? TEXT("Down_") : PL >= 3.f ? TEXT("Hit3_") : PL >= 2.f ? TEXT("Hit_") : TEXT("Hit1_");
		FName Reaction = DirectionalAction(Prefix, ToAttacker);
		if (!ActionTable || !ActionTable->FindRow<FNWActionRow>(Reaction, TEXT("reaction"), false))
		{
			Reaction = DirectionalAction(TEXT("Hit_"), ToAttacker);
		}
		ReactionSeconds = PlayHitReaction(Reaction, Stun, PL >= 4.f ? 0.8f : 0.f);
		if (bTargetAI)
		{
			ReactionLimiterUntil = Now + 2.f;
		}
	}
	// Push (§2): backwards (Y) and to the attacker's side (X); with a reaction it follows the game's curve until the end of
	// the reaction, without a reaction it is the game's "Pushable" (constant speed for 0.4 s).
	const float Back = Pick(Damage.ImpactDistance, Damage.ImpactDistance_AI, Damage.CritImpactDistance);
	const FVector Dir = Away * Back + Right * Damage.ImpactDistanceX;
	if (!Dir.IsNearlyZero())
	{
		Push(Dir.GetSafeNormal2D(), Dir.Size2D() * ImpactDistanceScale, Damage.ImpactDecayRate, !bReact, ReactionSeconds);
	}
	return ENWHitResult::Hit;
}

namespace
{
	// Push strength over the normalized time (0..1) of the root motion: e^(−K·x·T) = r^(30·t).
	UCurveFloat* DecayCurve(float K, float Seconds)
	{
		static TMap<FIntPoint, TObjectPtr<UCurveFloat>> Cache;
		const FIntPoint Key(FMath::RoundToInt(K * 100.f), FMath::RoundToInt(Seconds * 1000.f));
		if (TObjectPtr<UCurveFloat>* Found = Cache.Find(Key))
		{
			return *Found;
		}
		UCurveFloat* C = NewObject<UCurveFloat>(GetTransientPackage());
		C->AddToRoot();
		constexpr int32 Keys = 24;
		for (int32 i = 0; i <= Keys; ++i)
		{
			const float X = float(i) / Keys;
			C->FloatCurve.AddKey(X, FMath::Exp(-K * X * Seconds));
		}
		Cache.Add(Key, C);
		return C;
	}
}

void UNWCombatComponent::Push(const FVector& Dir, float Meters, float DecayRate, bool bNoDecay, float ReactionSeconds)
{
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Move = Char ? Char->GetCharacterMovement() : nullptr;
	if (!Move || Meters <= 0.f)
	{
		return;
	}
	// Root motion glued to the ground (LaunchCharacter left the target "falling", without friction: on slopes it flew far).
	const float Fps = float(FNWRules::Num(TEXT("combate.quadros_por_s"), 30.0));
	TSharedPtr<FRootMotionSource_ConstantForce> P = MakeShared<FRootMotionSource_ConstantForce>();
	P->InstanceName = TEXT("NWKnockback");
	P->AccumulateMode = ERootMotionAccumulateMode::Override;
	P->Priority = 5;
	P->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	P->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	const float R = DecayRate;
	if (bNoDecay)
	{
		P->Duration = 0.4f;
		P->Force = Dir * (Meters * 100.f / P->Duration);
	}
	else if (R <= 0.001f || R >= 0.999f || ReactionSeconds <= 0.f)
	{
		// combate.empurrao_curva: r ≈ 0, r ≈ 1 or no duration, the whole push in one frame.
		P->Duration = 1.f / Fps;
		P->Force = Dir * (Meters * 100.f * Fps);
	}
	else
	{
		// combate.empurrao_curva: s(t) = D·(1 − r^(30t)) until the end of the reaction; v(t) = D·K·e^(−K·t), K = −30·ln r.
		const float K = -FMath::Loge(R) * Fps;
		P->Duration = ReactionSeconds;
		P->Force = Dir * (Meters * 100.f * K);
		P->StrengthOverTime = DecayCurve(K, ReactionSeconds);
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
		// Knockdown (R4): stays on the ground until the get-up ends.
		ReactionNext = Row->NextAction;
		if (!ReactionNext.IsNone() && CurrentMontage)
		{
			// +0.3 s: the end of the montage (which calls the get-up) arrives before the stun ends.
			StunUntil = FMath::Max(StunUntil, GetWorld()->GetTimeSeconds() + CurrentMontage->GetPlayLength() / FMath::Max(CurrentMontage->RateScale, 0.01f) + 0.3f);
		}
	}
}

float UNWCombatComponent::PlayHitReaction(FName Action, float HitStun, float ExtraSeconds)
{
	// combate.reacao_duracao: the reaction lasts the start of the HitStun procclip (HitStunStart, from the .adb) + HitStun/30;
	// the rest of the montage plays at the rate that makes it end together, if the montage has no speed of its own.
	const FNWActionRow* Row = ActionTable ? ActionTable->FindRow<FNWActionRow>(Action, TEXT("reaction"), false) : nullptr;
	const float Start = Row ? Row->HitStunStart : 0.f;
	const float Fps = float(FNWRules::Num(TEXT("combate.quadros_por_s"), 30.0));
	const float Stun = FMath::Max(HitStun, float(FNWRules::Num(TEXT("combate.hitstun_min"), 0.0025))) / Fps;
	const float Seconds = Start + Stun + ExtraSeconds;
	PlayReaction(Action, Seconds);
	GetWorld()->GetTimerManager().ClearTimer(ReactionRateTimer);
	UAnimMontage* Montage = CurrentMontage;
	if (Row && Montage && Row->NextAction.IsNone() && Start > 0.f && FMath::IsNearlyEqual(Montage->RateScale, 1.f))
	{
		const float Rate = FMath::Clamp((Montage->GetPlayLength() - Start) / Stun,
			float(FNWRules::Num(TEXT("combate.reacao_velocidade_min"), 0.001)), float(FNWRules::Num(TEXT("combate.reacao_velocidade_max"), 3.0)));
		TWeakObjectPtr<UNWCombatComponent> Weak(this);
		GetWorld()->GetTimerManager().SetTimer(ReactionRateTimer, [Weak, Montage, Rate]()
		{
			if (Weak.IsValid())
			{
				Weak->MulticastSetMontageRate(Montage, Rate);
			}
		}, Start, false);
	}
	return Seconds;
}

void UNWCombatComponent::MulticastSetMontageRate_Implementation(UAnimMontage* Montage, float Rate)
{
	if (UAnimInstance* Anim = GetAnim(); Anim && Montage && Anim->Montage_IsPlaying(Montage))
	{
		Anim->Montage_SetPlayRate(Montage, Rate);
	}
}

FName UNWCombatComponent::DirectionalAction(const TCHAR* Prefix, const FVector& WorldDir) const
{
	// WorldDir points from the character to the origin of the strike.
	const FVector Local = GetOwner()->GetActorTransform().InverseTransformVectorNoScale(WorldDir);
	const TCHAR* Side = FMath::Abs(Local.X) >= FMath::Abs(Local.Y) ? (Local.X >= 0 ? TEXT("F") : TEXT("B")) : (Local.Y >= 0 ? TEXT("R") : TEXT("L"));
	return FName(FString(Prefix) + Side);
}

void UNWCombatComponent::MulticastShowHit_Implementation(float Damage, ENWHitResult Result, FVector_NetQuantize From, bool bByCreature,
	bool bCrit, AActor* By, FName ByShake, FName TargetShake)
{
	bLastHitByCreature = bByCreature;
	// Shakes from the damage table, each on the camera of whoever is local.
	if (ANWCharacter* Me = Cast<ANWCharacter>(GetOwner()); Me && Me->IsLocallyControlled() && !TargetShake.IsNone())
	{
		Me->PlayCameraShake(TargetShake);
	}
	if (ANWCharacter* Hitter = Cast<ANWCharacter>(By); Hitter && Hitter->IsLocallyControlled() && !ByShake.IsNone())
	{
		Hitter->PlayCameraShake(ByShake);
	}
	// Damage number on the local player's HUD (like the game's damagenumbers); a dedicated server does not draw.
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	ANWHUD* HUD = PC ? Cast<ANWHUD>(PC->GetHUD()) : nullptr;
	if (!HUD)
	{
		return;
	}
	const FVector At = GetOwner()->GetActorLocation() + FVector(0, 0, 110);
	// Damage to me: red; damage I or others deal: light; block and dodge: port text.
	const bool bMe = GetOwner() == PC->GetPawn();
	if (bMe && (Result == ENWHitResult::Hit || Result == ENWHitResult::BlockBroken))
	{
		HUD->AddHitIndicator(From, MaxHealth > 0.f ? Damage / MaxHealth : 0.f);
		// Port extra (nw.HitKick 0 disables it): camera kick proportional to the damage (10% of health = 1).
		if (ANWCharacter* Me = Cast<ANWCharacter>(GetOwner()); Me && CVarNWHitKick.GetValueOnGameThread() != 0)
		{
			Me->AddCameraKick(FMath::Clamp(0.5f + (MaxHealth > 0.f ? 5.f * Damage / MaxHealth : 0.f), 0.5f, 1.5f));
		}
	}
	switch (Result)
	{
	case ENWHitResult::Hit:
		// Crit: gold and large.
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
	// Port extra (nw.HitKick): the attacker feels the hit too, a light kick on the local player's camera.
	if (ANWCharacter* Me = Cast<ANWCharacter>(Self); Me && Me->IsLocallyControlled() && CVarNWHitKick.GetValueOnGameThread() != 0)
	{
		Me->AddCameraKick(0.35f);
	}
	// combate.hit_stop_s: 0 in the game, no pause.
	if (Seconds <= 0.f)
	{
		return;
	}
	Self->CustomTimeDilation = HitStopDilation;
	if (Other)
	{
		Other->CustomTimeDilation = HitStopDilation;
	}
	// The timer uses world time, which is not affected by the actor's dilation.
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
	// Reapplying renews the duration and adds a stack, up to the maximum.
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
	// Damage the status would still deal: stacks × damage per tick × remaining ticks.
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
		// Damage per tick (bleed, poison): no reaction, number in the status color.
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
		return TEXT("#") + Creature->Def()->NameKey;   // game name, translated on the client (NWText::Resolve)
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
	FNWEvents::Get().OnKill.Broadcast(Killer, GetOwner());   // level tutorials (area 4, NWEvents.h)
	// Lost killed by a player: loot for the killer (and the group).
	if (GetOwner()->GetClass()->GetName().Contains(TEXT("Lost")))
	{
		if (ANWGameMode* GM = GetWorld()->GetAuthGameMode<ANWGameMode>())
		{
			GM->GrantLoot(Cast<APawn>(Killer), GetOwner(), TEXT("lost"));
		}
	}
	// The dead do not walk (the movement mode is replicated: applies on the client too).
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
		RespawnAt = ServerNow() + RespawnDelay;   // the player chooses when, through the death screen
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
	// The player respawns at the map's starting point (the settlement), not where they died.
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

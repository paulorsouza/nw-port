#include "NWBrainComponent.h"

#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NWCharacter.h"
#include "NWCombatComponent.h"

namespace
{
	// Quem está atacando cada alvo agora (vez da matilha / dos duelistas) e quem está em volta dele.
	TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<UNWBrainComponent>>> GTokens;
	TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<UNWBrainComponent>>> GAround;

	void Prune(TArray<TWeakObjectPtr<UNWBrainComponent>>& List)
	{
		List.RemoveAll([](const TWeakObjectPtr<UNWBrainComponent>& B) { return !B.IsValid() || !B->GetTarget(); });
	}
}

UNWBrainComponent::UNWBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	StrafeSign = FMath::RandBool() ? 1.f : -1.f;
}

void UNWBrainComponent::ApplyStyle(ENWBrainStyle NewStyle)
{
	Style = NewStyle;
	switch (Style)
	{
	case ENWBrainStyle::Duelist:
		AttackPause = FVector2D(0.8f, 2.0f); HoldDistance = 380.f; AttackersPerTarget = 1; bFaceMovement = false;
		break;
	case ENWBrainStyle::Pack:
		AttackPause = FVector2D(0.6f, 1.6f); HoldDistance = 520.f; AttackersPerTarget = 1; bFaceMovement = true;
		break;
	case ENWBrainStyle::Charger:
		AttackPause = FVector2D(1.0f, 2.2f); HoldDistance = 0.f; AttackersPerTarget = 3; bFaceMovement = true;
		break;
	case ENWBrainStyle::Archer:
		AttackPause = FVector2D(1.4f, 2.6f); HoldDistance = 1400.f; AttackersPerTarget = 4; bFaceMovement = false;
		break;
	default:
		AttackPause = FVector2D(1.5f, 2.5f); HoldDistance = 0.f; AttackersPerTarget = 4;
		break;
	}
}

void UNWBrainComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ReleaseToken();
	Target.Reset();
	Super::EndPlay(Reason);
}

ANWCharacter* UNWBrainComponent::FindTarget(float& OutDist) const
{
	const ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	ANWCharacter* Best = nullptr;
	OutDist = Me->AggroRange;
	if (OutDist <= 0.f)
	{
		return nullptr;
	}
	// Mantém o alvo atual com folga de 50% (não troca de alvo a cada passo).
	if (ANWCharacter* Cur = Target.Get(); Cur && Cur->Combat->State != ENWCombatState::Dead)
	{
		const float D = FVector::Dist2D(Cur->GetActorLocation(), Me->GetActorLocation());
		if (D < Me->AggroRange * 1.5f)
		{
			OutDist = D;
			return Cur;
		}
	}
	for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != Me && It->IsPlayerControlled() && It->Combat->State != ENWCombatState::Dead)
		{
			const float D = FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation());
			if (D < OutDist)
			{
				OutDist = D;
				Best = *It;
			}
		}
	}
	return Best;
}

const FNWBrainAttack* UNWBrainComponent::PickAttack(float Dist) const
{
	const float Now = GetWorld()->GetTimeSeconds();
	float Total = 0.f;
	TArray<const FNWBrainAttack*, TInlineAllocator<8>> Ok;
	for (const FNWBrainAttack& A : Attacks)
	{
		const float* Ready = ReadyAt.Find(A.Action);
		if (Dist >= A.MinRange && Dist <= A.MaxRange && (!Ready || Now >= *Ready) && A.Weight > 0.f)
		{
			Ok.Add(&A);
			Total += A.Weight;
		}
	}
	float Roll = FMath::FRandRange(0.f, Total);
	for (const FNWBrainAttack* A : Ok)
	{
		Roll -= A->Weight;
		if (Roll <= 0.f)
		{
			return A;
		}
	}
	return Ok.Num() ? Ok.Last() : nullptr;
}

bool UNWBrainComponent::TakeToken()
{
	if (bHasToken)
	{
		return true;
	}
	AActor* T = Target.Get();
	if (!T)
	{
		return false;
	}
	TArray<TWeakObjectPtr<UNWBrainComponent>>& List = GTokens.FindOrAdd(T);
	Prune(List);
	int32 Same = 0;
	for (const TWeakObjectPtr<UNWBrainComponent>& B : List)
	{
		Same += B->Style == Style ? 1 : 0;
	}
	if (Same >= AttackersPerTarget)
	{
		return false;
	}
	List.Add(this);
	bHasToken = true;
	return true;
}

void UNWBrainComponent::ReleaseToken()
{
	if (!bHasToken)
	{
		return;
	}
	bHasToken = false;
	for (auto& Pair : GTokens)
	{
		Pair.Value.Remove(this);
	}
}

int32 UNWBrainComponent::PackIndex() const
{
	if (const TArray<TWeakObjectPtr<UNWBrainComponent>>* List = GAround.Find(Target))
	{
		return List->IndexOfByKey(TWeakObjectPtr<UNWBrainComponent>(const_cast<UNWBrainComponent*>(this)));
	}
	return 0;
}

void UNWBrainComponent::Steer(ANWCharacter* Me, const FVector& Dir, bool bSlow)
{
	Me->bWalking = bSlow;
	if (!Dir.IsNearlyZero())
	{
		Me->AddMovementInput(Dir.GetSafeNormal2D(), 1.f);
	}
}

void UNWBrainComponent::Face(ANWCharacter* Me, const FVector& Dir, float DeltaTime, float Rate)
{
	if (Dir.IsNearlyZero())
	{
		return;
	}
	Me->SetActorRotation(FMath::RInterpConstantTo(Me->GetActorRotation(), FRotator(0.f, Dir.Rotation().Yaw, 0.f), DeltaTime, Rate));
	if (Me->Controller)
	{
		Me->Controller->SetControlRotation(Me->GetActorRotation());
	}
}

void UNWBrainComponent::StartAttack(const FNWBrainAttack& A)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	Current = &A;
	ChainLeft = FMath::Max(1, A.Chain) - 1;
	FollowIndex = 0;
	ChainTimer = 0.f;
	Me->Combat->RequestAction(A.Action);
	if (A.Cooldown > 0.f)
	{
		ReadyAt.Add(A.Action, GetWorld()->GetTimeSeconds() + A.Cooldown);
	}
	Mode = EMode::Attack;
	ModeTime = 0.f;
}

void UNWBrainComponent::TickDefense(ANWCharacter* Me, ANWCharacter* T, float Dist, float DeltaTime)
{
	UNWCombatComponent* C = Me->Combat;
	const float Now = GetWorld()->GetTimeSeconds();
	if (C->State == ENWCombatState::Blocking)
	{
		if (Now >= BlockUntil)
		{
			C->RequestBlock(false);
		}
		return;
	}
	// Golpe novo do alvo, de perto e virado para mim: decide uma vez se defende e reage depois do tempo de reação.
	const int32 Serial = T->Combat->GetActionSerial();
	if (Serial != SeenTargetAction)
	{
		SeenTargetAction = Serial;
		const FVector ToMe = (Me->GetActorLocation() - T->GetActorLocation()).GetSafeNormal2D();
		const bool bThreat = T->Combat->State == ENWCombatState::Attacking && Dist < 450.f
			&& FVector::DotProduct(T->GetActorForwardVector().GetSafeNormal2D(), ToMe) > 0.5f;
		bWillBlock = bThreat && FMath::FRand() < BlockChance;
		bWillDodge = bThreat && !bWillBlock && FMath::FRand() < DodgeChance;
		ReactAt = (bWillBlock || bWillDodge) ? Now + ReactionTime * FMath::FRandRange(0.8f, 1.4f) : -1.f;
	}
	if (ReactAt < 0.f || Now < ReactAt || C->IsBusy())
	{
		return;
	}
	ReactAt = -1.f;
	if (bWillBlock)
	{
		C->RequestBlock(true);
		BlockUntil = Now + FMath::FRandRange(0.7f, 1.3f);
	}
	else if (bWillDodge)
	{
		// Para trás ou para um lado (X frente, Y direita no espaço do personagem).
		const int32 Pick = FMath::RandRange(0, 2);
		C->RequestDodge(Pick == 0 ? FVector2D(-1.f, 0.f) : FVector2D(0.f, Pick == 1 ? 1.f : -1.f));
	}
}

void UNWBrainComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!IsActive() || !Me || !Me->HasAuthority() || Me->IsPlayerControlled())
	{
		return;
	}
	UNWCombatComponent* C = Me->Combat;
	if (C->State == ENWCombatState::Dead)
	{
		ReleaseToken();
		Target.Reset();
		return;
	}
	float Dist = 0.f;
	ANWCharacter* T = FindTarget(Dist);
	if (T != Target.Get())
	{
		ReleaseToken();
		if (TArray<TWeakObjectPtr<UNWBrainComponent>>* Old = GAround.Find(Target))
		{
			Old->Remove(this);
		}
		Target = T;
		Mode = EMode::Approach;
		if (T)
		{
			GAround.FindOrAdd(T).AddUnique(this);
			NextAttackAt = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.3f, 1.0f);
		}
	}
	if (!T)
	{
		Me->bWalking = false;
		return;
	}
	if (TArray<TWeakObjectPtr<UNWBrainComponent>>* Around = GAround.Find(T))
	{
		Prune(*Around);
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const FVector ToT = (T->GetActorLocation() - Me->GetActorLocation()).GetSafeNormal2D();
	ModeTime += DeltaTime;

	// Atordoado ou reagindo a golpe: a vez volta para a fila.
	if (C->State == ENWCombatState::Stunned)
	{
		ReleaseToken();
		Mode = EMode::Recover;
		ModeTime = 0.f;
		ModeLength = 0.4f;
		return;
	}
	if (BlockChance > 0.f || DodgeChance > 0.f)
	{
		TickDefense(Me, T, Dist, DeltaTime);
		if (C->State == ENWCombatState::Blocking)
		{
			Face(Me, ToT, DeltaTime, 540.f);
			return;
		}
	}

	if (Mode == EMode::Attack)
	{
		// Combo: o próximo pedido entra no buffer do combate e sai na janela certa.
		ChainTimer += DeltaTime;
		if (ChainLeft > 0 && ChainTimer > 0.25f && Current)
		{
			C->RequestAction(Current->Action);
			--ChainLeft;
			ChainTimer = 0.f;
		}
		if (Current && Current->bGapCloser && C->IsBusy())
		{
			Steer(Me, Me->GetActorForwardVector(), false);
		}
		else if (C->IsBusy() && C->GetCurve(TEXT("Homing")) > 0.5f)
		{
			Face(Me, ToT, DeltaTime, 360.f);
		}
		if (!C->IsBusy() && ModeTime > 0.3f && ChainLeft == 0 && Current && FollowIndex < Current->FollowUps.Num())
		{
			C->RequestAction(Current->FollowUps[FollowIndex++]);
			ModeTime = 0.f;
			return;
		}
		if (!C->IsBusy() && ModeTime > 0.3f && ChainLeft == 0)
		{
			ReleaseToken();
			Current = nullptr;
			NextAttackAt = Now + FMath::FRandRange(AttackPause.X, AttackPause.Y);
			Mode = EMode::Recover;
			ModeTime = 0.f;
			ModeLength = Style == ENWBrainStyle::Duelist || Style == ENWBrainStyle::Pack ? FMath::FRandRange(0.4f, 0.9f) : 0.2f;
		}
		return;
	}
	if (C->IsBusy())
	{
		return;
	}
	if (Mode == EMode::Recover)
	{
		// Recuo curto depois de atacar (duelista, matilha) ou de levar golpe.
		if (Style == ENWBrainStyle::Duelist || Style == ENWBrainStyle::Pack)
		{
			Steer(Me, -ToT, true);
		}
		Face(Me, ToT, DeltaTime, 360.f);
		if (ModeTime >= ModeLength)
		{
			Mode = EMode::Approach;
			ModeTime = 0.f;
		}
		return;
	}

	// Escolhe o ataque quando a pausa acabou e (duelista/matilha) é a vez deste.
	if (Now >= NextAttackAt)
	{
		if (const FNWBrainAttack* A = PickAttack(Dist))
		{
			const bool bNeedsTurn = Style == ENWBrainStyle::Duelist || Style == ENWBrainStyle::Pack;
			if (!bNeedsTurn || TakeToken())
			{
				if (FVector::DotProduct(Me->GetActorForwardVector().GetSafeNormal2D(), ToT) > 0.9f)
				{
					StartAttack(*A);
					return;
				}
				Face(Me, ToT, DeltaTime, 720.f);
				return;
			}
		}
	}

	// Alcance para onde andar: o maior entre os ataques prontos; com todos em recarga, o mais curto.
	float MaxReach = 0.f, MinReach = TNumericLimits<float>::Max();
	for (const FNWBrainAttack& A : Attacks)
	{
		const float* Ready = ReadyAt.Find(A.Action);
		if (!Ready || Now >= *Ready)
		{
			MaxReach = FMath::Max(MaxReach, A.MaxRange);
		}
		MinReach = FMath::Min(MinReach, A.MaxRange);
	}
	if (MaxReach <= 0.f && Attacks.Num())
	{
		MaxReach = MinReach;
	}
	switch (Style)
	{
	case ENWBrainStyle::Archer:
	{
		// Faixa preferida: recua se chegou perto, aproxima se longe, senão anda de lado.
		if (Dist < HoldDistance * 0.55f)
		{
			Steer(Me, -ToT, false);
		}
		else if (Dist > HoldDistance * 1.25f)
		{
			Steer(Me, ToT, false);
		}
		else
		{
			Steer(Me, FVector::CrossProduct(ToT, FVector::UpVector) * StrafeSign, true);
			if (ModeTime > 2.5f) { StrafeSign = -StrafeSign; ModeTime = 0.f; }
		}
		Face(Me, ToT, DeltaTime, 540.f);
		break;
	}
	case ENWBrainStyle::Duelist:
	case ENWBrainStyle::Pack:
	{
		// Sem a vez (ou esperando a pausa): rodeia na distância de espera; a matilha se espalha pelos flancos.
		FVector Want = T->GetActorLocation();
		if (Style == ENWBrainStyle::Pack)
		{
			const int32 N = FMath::Max(1, GAround.FindRef(T).Num());
			const float Angle = (360.f / N) * PackIndex() + 25.f * FMath::Sin(Now * 0.6f + PackIndex());
			Want += FRotator(0.f, T->GetActorRotation().Yaw + 180.f + Angle, 0.f).Vector() * HoldDistance;
		}
		else
		{
			Want -= ToT * HoldDistance;
		}
		const FVector Off = Want - Me->GetActorLocation();
		FVector Dir;
		if (Off.Size2D() > 120.f)
		{
			Dir = Off.GetSafeNormal2D();
		}
		else
		{
			Dir = FVector::CrossProduct(ToT, FVector::UpVector) * StrafeSign;
			if (ModeTime > FMath::FRandRange(1.5f, 3.f)) { StrafeSign = -StrafeSign; ModeTime = 0.f; }
		}
		const bool bFar = Dist > HoldDistance * 1.8f;
		Steer(Me, Dir, !bFar && Style == ENWBrainStyle::Duelist);
		Face(Me, bFaceMovement ? Dir : ToT, DeltaTime, 540.f);
		break;
	}
	default:
	{
		// Brawler e Charger: vai direto até o alcance do golpe mais longo.
		if (Dist > MaxReach * 0.8f || MaxReach <= 0.f)
		{
			Steer(Me, ToT, false);
		}
		Face(Me, ToT, DeltaTime, bFaceMovement ? 300.f : 540.f);
		break;
	}
	}
}

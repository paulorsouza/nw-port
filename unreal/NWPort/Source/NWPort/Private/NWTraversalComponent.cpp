#include "NWTraversalComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

static TAutoConsoleVariable<int32> CVarDebugTraversal(TEXT("nw.DebugTraversal"), 0,
	TEXT("Desenha os raios de detecção de degrau/vault/mantle."));

static const TCHAR* AnimDir = TEXT("/Game/NW/sword_shield/Anims/");

static UAnimMontage* LoadMontage(const FString& Name)
{
	const FString Path = FString(AnimDir) + Name + TEXT(".") + Name;
	UAnimMontage* M = LoadObject<UAnimMontage>(nullptr, *Path);
	if (!M)
	{
		UE_LOG(LogTemp, Warning, TEXT("NW: montagem ausente %s"), *Path);
	}
	return M;
}

UNWTraversalComponent::UNWTraversalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UNWTraversalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNWTraversalComponent, Active);
}

ACharacter* UNWTraversalComponent::GetCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

void UNWTraversalComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!bEnabled)
	{
		SetComponentTickEnabled(false);
		return;
	}
	LoadMontages();
	if (ACharacter* C = GetCharacter())
	{
		C->GetCharacterMovement()->ProcessRootMotionPreConvertToWorld.BindUObject(this, &UNWTraversalComponent::WarpRootMotion);
	}
}

void UNWTraversalComponent::LoadMontages()
{
	const TPair<ENWTraversal, const TCHAR*> Names[] = {
		{ENWTraversal::StepUpLow, TEXT("AM_StepUpLow")}, {ENWTraversal::StepUp, TEXT("AM_StepUp")},
		{ENWTraversal::VaultLow, TEXT("AM_VaultLow")}, {ENWTraversal::Vault, TEXT("AM_Vault")},
		{ENWTraversal::MantleLow, TEXT("AM_MantleLow")}, {ENWTraversal::MantleHigh, TEXT("AM_MantleHigh")},
	};
	for (const auto& [Kind, Name] : Names)
	{
		UAnimMontage* M = LoadMontage(Name);
		if (!M)
		{
			continue;
		}
		TraversalMontages.Add(Kind, M);
		// Deslocamento total do osso raiz no espaço da malha: a malha olha para +Y do componente.
		const FVector T = M->ExtractRootMotionFromTrackRange(0.f, M->GetPlayLength(), FAnimExtractContext()).GetTranslation();
		NaturalMotion.Add(Kind, FVector2D(T.Y, T.Z));
		const FString Label(Name);
		UE_LOG(LogTemp, Log, TEXT("NW: %s root motion frente %.0f cima %.0f cm (lado %.0f)"), *Label, T.Y, T.Z, T.X);
	}
	JumpStand = LoadMontage(TEXT("AM_JumpStand"));
	for (const TCHAR* D : {TEXT("F"), TEXT("FR"), TEXT("R"), TEXT("BR"), TEXT("B"), TEXT("BL"), TEXT("L"), TEXT("FL")})
	{
		JumpDir.Add(LoadMontage(FString(TEXT("AM_Jump_")) + D));
		LandMoveDir.Add(LoadMontage(FString(TEXT("AM_LandMove_")) + D));
	}
	Fall = LoadMontage(TEXT("AM_Fall"));
	FallFlail = LoadMontage(TEXT("AM_FallFlail"));
	Land = LoadMontage(TEXT("AM_Land"));
	LandHard = LoadMontage(TEXT("AM_LandHard"));
}

UAnimMontage* UNWTraversalComponent::MontageFor(ENWTraversal Kind) const
{
	const TObjectPtr<UAnimMontage>* M = TraversalMontages.Find(Kind);
	return M ? M->Get() : nullptr;
}

// ---------------------------------------------------------------- detecção

bool UNWTraversalComponent::CapsuleFits(const FVector& Feet) const
{
	const ACharacter* C = GetCharacter();
	const UCapsuleComponent* Cap = C->GetCapsuleComponent();
	const float R = Cap->GetScaledCapsuleRadius() - 2.f;
	const float H = Cap->GetScaledCapsuleHalfHeight() - 2.f;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NWTraversalFit), false, C);
	const FVector Center = Feet + FVector(0.f, 0.f, H + 4.f);
	const bool bBlocked = GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(R, H), Q);
	if (bDebugDraw || CVarDebugTraversal.GetValueOnGameThread())
	{
		DrawDebugCapsule(GetWorld(), Center, H, R, FQuat::Identity, bBlocked ? FColor::Red : FColor::Green, false, 3.f);
	}
	return !bBlocked;
}

FNWTraversalPlan UNWTraversalComponent::Detect() const
{
	FNWTraversalPlan Plan;
	const ACharacter* C = GetCharacter();
	if (!C)
	{
		return Plan;
	}
	const UCharacterMovementComponent* Move = C->GetCharacterMovement();
	const float Radius = C->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector Loc = C->GetActorLocation();
	const FVector Feet = Loc - FVector(0.f, 0.f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	// Direção: para onde o jogador está empurrando; parado, para a frente do personagem.
	FVector Dir = Move->GetCurrentAcceleration().GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = C->GetActorForwardVector().GetSafeNormal2D();
	}

	const bool bDebug = bDebugDraw || CVarDebugTraversal.GetValueOnGameThread() != 0;
	UWorld* World = GetWorld();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NWTraversal), true, C);
	const float Reach = Radius + FMath::Max3(StepUpRange, VaultRange, MantleRange);
	auto Forward = [&](float H, FHitResult& Hit)
	{
		const FVector A = FVector(Loc.X, Loc.Y, Feet.Z + H);
		const bool bHit = World->SweepSingleByChannel(Hit, A, A + Dir * Reach, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(8.f), Q);
		if (bDebug)
		{
			DrawDebugLine(World, A, A + Dir * Reach, bHit ? FColor::Orange : FColor::Silver, false, 3.f);
		}
		return bHit && FMath::Abs(Hit.ImpactNormal.Z) < 0.5f;
	};
	auto Down = [&](const FVector& XY, float FromZ, float ToZ, FHitResult& Hit)
	{
		const FVector A(XY.X, XY.Y, FromZ), B(XY.X, XY.Y, ToZ);
		const bool bHit = World->LineTraceSingleByChannel(Hit, A, B, ECC_Visibility, Q);
		if (bDebug)
		{
			DrawDebugLine(World, A, bHit ? Hit.ImpactPoint : B, bHit ? FColor::Cyan : FColor::Silver, false, 3.f);
		}
		return bHit;
	};

	// 1) Parede: o raio mais baixo que bate numa superfície quase vertical.
	FHitResult Wall;
	float WallH = -1.f;
	for (float H = StepUpMinHeight * 0.6f; H <= MantleMaxHeight; H += 15.f)
	{
		if (Forward(H, Wall))
		{
			WallH = H;
			break;
		}
	}
	if (WallH < 0.f)
	{
		return Plan;
	}
	const FVector N = FVector(Wall.ImpactNormal.X, Wall.ImpactNormal.Y, 0.f).GetSafeNormal();
	const float WallDist = FVector::DotProduct(Wall.ImpactPoint - Loc, Dir) - Radius;

	// 2) Fim da parede: subindo, o primeiro raio que passa livre (ou bate bem mais longe) marca o topo.
	float ClearH = -1.f;
	for (float H = WallH + 10.f; H <= MantleMaxHeight + 30.f; H += 10.f)
	{
		FHitResult Hit;
		if (!Forward(H, Hit) || FVector::DotProduct(Hit.ImpactPoint - Wall.ImpactPoint, Dir) > 30.f)
		{
			ClearH = H;
			break;
		}
	}
	if (ClearH < 0.f)
	{
		return Plan;
	}

	// 3) Topo: raio para baixo logo depois da borda.
	FHitResult Top;
	const FVector Edge = Wall.ImpactPoint - N * 15.f;
	if (!Down(Edge, Feet.Z + ClearH + 20.f, Feet.Z + WallH - 20.f, Top) || Top.ImpactNormal.Z < MinFloorNormalZ)
	{
		return Plan;
	}
	const float Height = Top.ImpactPoint.Z - Feet.Z;
	Plan.Height = Height;
	Plan.Yaw = (-N).Rotation().Yaw;

	// 4) Espessura e pouso do outro lado (vault).
	float Thickness = -1.f;
	for (float D = 15.f; D <= VaultMaxThickness + 15.f; D += 15.f)
	{
		FHitResult H;
		if (!Down(Wall.ImpactPoint - N * D, Top.ImpactPoint.Z + 20.f, Top.ImpactPoint.Z - 40.f, H))
		{
			Thickness = D;
			break;
		}
	}
	bool bCanVault = false;
	FVector VaultEnd = FVector::ZeroVector;
	if (Thickness >= VaultMinThickness && Thickness <= VaultMaxThickness)
	{
		FHitResult Ground;
		const FVector P = Wall.ImpactPoint - N * (Thickness + Radius + 15.f);
		if (Down(P, Top.ImpactPoint.Z + 20.f, Feet.Z - 150.f, Ground) && Ground.ImpactNormal.Z >= MinFloorNormalZ
			&& Ground.ImpactPoint.Z <= Feet.Z + VaultMaxLandingDiff && Ground.ImpactPoint.Z >= Feet.Z - 60.f)
		{
			VaultEnd = Ground.ImpactPoint;
			bCanVault = CapsuleFits(VaultEnd);
		}
	}

	// 5) Lugar para ficar em pé em cima (degrau/mantle).
	FVector TopEnd = FVector::ZeroVector;
	bool bFitsOnTop = false;
	{
		FHitResult Floor;
		const FVector P = Wall.ImpactPoint - N * (Radius + 15.f);
		if (Down(P, Top.ImpactPoint.Z + 30.f, Top.ImpactPoint.Z - 30.f, Floor) && Floor.ImpactNormal.Z >= MinFloorNormalZ)
		{
			TopEnd = Floor.ImpactPoint;
			bFitsOnTop = CapsuleFits(TopEnd);
		}
	}

	// Entre duas variantes, a que tem o deslocamento natural mais perto da altura pedida.
	auto Closest = [&](ENWTraversal A, ENWTraversal B)
	{
		const FVector2D* NA = NaturalMotion.Find(A);
		const FVector2D* NB = NaturalMotion.Find(B);
		if (!NA) return B;
		if (!NB) return A;
		return FMath::Abs(NA->Y - Height) <= FMath::Abs(NB->Y - Height) ? A : B;
	};

	if (bCanVault && Height >= VaultMinHeight && Height <= VaultMaxHeight && WallDist <= VaultRange)
	{
		Plan.Kind = Height < 75.f ? ENWTraversal::VaultLow : ENWTraversal::Vault;
		Plan.End = VaultEnd;
	}
	else if (bFitsOnTop && Height >= StepUpMinHeight && Height <= StepUpMaxHeight && WallDist <= StepUpRange)
	{
		Plan.Kind = Closest(ENWTraversal::StepUpLow, ENWTraversal::StepUp);
		Plan.End = TopEnd;
	}
	else if (bFitsOnTop && Height >= MantleMinHeight && Height <= MantleMaxHeight && WallDist <= MantleRange)
	{
		Plan.Kind = Closest(ENWTraversal::MantleLow, ENWTraversal::MantleHigh);
		Plan.End = TopEnd;
	}
	if (Plan.Kind != ENWTraversal::None && !MontageFor(Plan.Kind))
	{
		Plan.Kind = ENWTraversal::None;
	}
	if (bDebug)
	{
		UE_LOG(LogTemp, Log, TEXT("NW travessia: altura %.0f dist %.0f espessura %.0f vault %d topo %d -> %d"),
			Height, WallDist, Thickness, bCanVault, bFitsOnTop, (int32)Plan.Kind);
	}
	return Plan;
}

// ---------------------------------------------------------------- execução

void UNWTraversalComponent::JumpPressed()
{
	ACharacter* C = GetCharacter();
	if (!C || !bEnabled || IsTraversing())
	{
		return;
	}
	if (Detect().Kind != ENWTraversal::None)
	{
		ServerTraverse();
		return;
	}
	C->Jump();   // no ar não faz nada (um pulo só)
}

void UNWTraversalComponent::ServerTraverse_Implementation()
{
	if (IsTraversing())
	{
		return;
	}
	const FNWTraversalPlan Plan = Detect();
	if (Plan.Kind == ENWTraversal::None)
	{
		return;
	}
	const ACharacter* C = GetCharacter();
	const FVector Feet = C->GetActorLocation() - FVector(0.f, 0.f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	Active = Plan.Kind;
	MulticastStartTraversal(Plan.Kind, Feet, Plan);
}

void UNWTraversalComponent::MulticastStartTraversal_Implementation(ENWTraversal Kind, FVector Start, FNWTraversalPlan Plan)
{
	ACharacter* C = GetCharacter();
	UAnimMontage* M = MontageFor(Kind);
	UAnimInstance* Anim = C ? C->GetMesh()->GetAnimInstance() : nullptr;
	if (!M || !Anim)
	{
		Active = ENWTraversal::None;
		return;
	}
	Active = Kind;
	TravStart = Start;
	TravPlan = Plan;
	TravMontage = M;

	// Ajuste do root motion: multiplica quando o clipe tem deslocamento suficiente naquele eixo,
	// senão soma a diferença ao longo da montagem.
	const FVector2D Natural = NaturalMotion.FindRef(Kind);
	const FVector Facing = FRotator(0.f, Plan.Yaw, 0.f).Vector();
	const FVector2D Target(FVector::DotProduct(Plan.End - Start, Facing), Plan.End.Z - Start.Z);
	const float Len = FMath::Max(M->GetPlayLength(), 0.1f);
	WarpScale = FVector2D(1.f, 1.f);
	WarpAdd = FVector2D::ZeroVector;
	if (FMath::Abs(Natural.X) > 20.f) WarpScale.X = Target.X / Natural.X; else WarpAdd.X = (Target.X - Natural.X) / Len;
	if (FMath::Abs(Natural.Y) > 30.f) WarpScale.Y = Target.Y / Natural.Y; else WarpAdd.Y = (Target.Y - Natural.Y) / Len;
	WarpScale.X = FMath::Clamp(WarpScale.X, 0.2f, 3.f);
	WarpScale.Y = FMath::Clamp(WarpScale.Y, 0.2f, 3.f);

	C->SetActorRotation(FRotator(0.f, Plan.Yaw, 0.f));
	UCharacterMovementComponent* Move = C->GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->SetMovementMode(MOVE_Flying);
	// Sem colisão com o cenário durante o movimento (o caminho do clipe corta a quina); no fim encaixa no ponto final.
	UCapsuleComponent* Cap = C->GetCapsuleComponent();
	SavedStaticResponse = Cap->GetCollisionResponseToChannel(ECC_WorldStatic);
	SavedDynamicResponse = Cap->GetCollisionResponseToChannel(ECC_WorldDynamic);
	Cap->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	Cap->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);

	AirMontage = nullptr;
	Anim->Montage_Play(M, 1.f);
	FOnMontageEnded End;
	End.BindWeakLambda(this, [this](UAnimMontage* Ended, bool) { if (Ended == TravMontage) FinishTraversal(); });
	Anim->Montage_SetEndDelegate(End, M);
	UE_LOG(LogTemp, Log, TEXT("NW travessia %d: alvo frente %.0f cima %.0f, natural %.0f/%.0f, escala %.2f/%.2f"),
		(int32)Kind, Target.X, Target.Y, Natural.X, Natural.Y, WarpScale.X, WarpScale.Y);
}

FTransform UNWTraversalComponent::WarpRootMotion(const FTransform& Delta, UCharacterMovementComponent* Move, float DeltaSeconds)
{
	if (!IsTraversing() || !TravMontage)
	{
		return Delta;
	}
	// Espaço da malha: frente = +Y, cima = +Z.
	FVector T = Delta.GetTranslation();
	T.Y = T.Y * WarpScale.X + WarpAdd.X * DeltaSeconds;
	T.Z = T.Z * WarpScale.Y + WarpAdd.Y * DeltaSeconds;
	FTransform Out = Delta;
	Out.SetTranslation(T);
	return Out;
}

void UNWTraversalComponent::FinishTraversal()
{
	ACharacter* C = GetCharacter();
	if (!C || !IsTraversing())
	{
		return;
	}
	UCapsuleComponent* Cap = C->GetCapsuleComponent();
	Cap->SetCollisionResponseToChannel(ECC_WorldStatic, SavedStaticResponse);
	Cap->SetCollisionResponseToChannel(ECC_WorldDynamic, SavedDynamicResponse);
	if (GetOwner()->HasAuthority())
	{
		// Encaixa no ponto calculado (o clipe ajustado chega perto; isto corrige o resto).
		const FVector Target = TravPlan.End + FVector(0.f, 0.f, Cap->GetScaledCapsuleHalfHeight() + 1.f);
		C->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
	}
	C->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Active = ENWTraversal::None;
	TravMontage = nullptr;
	bWasFalling = true;   // o "pouso" seguinte não toca animação de queda
	AirTime = 0.f;
}

// ---------------------------------------------------------------- pulo, queda e pouso (local)

UAnimMontage* UNWTraversalComponent::Directional(const TArray<TObjectPtr<UAnimMontage>>& Set) const
{
	const ACharacter* C = GetCharacter();
	const FVector Local = C->GetActorTransform().InverseTransformVectorNoScale(C->GetVelocity().GetSafeNormal2D());
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X)); // 0 = frente, 90 = direita
	const int32 Index = ((FMath::RoundToInt(Angle / 45.f) % 8) + 8) % 8;
	return Set.IsValidIndex(Index) ? Set[Index].Get() : nullptr;
}

void UNWTraversalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickAirAnimation(DeltaTime);

	// No ar com Espaço segurado: agarra a borda que aparecer à frente (só mantle), como no jogo.
	ACharacter* C = GetCharacter();
	if (C && C->IsLocallyControlled() && !IsTraversing() && C->bPressedJump && C->GetCharacterMovement()->IsFalling())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextAirCheck)
		{
			NextAirCheck = Now + 0.1f;
			const ENWTraversal K = Detect().Kind;
			if (K == ENWTraversal::MantleLow || K == ENWTraversal::MantleHigh)
			{
				ServerTraverse();
			}
		}
	}
}

void UNWTraversalComponent::TickAirAnimation(float DeltaTime)
{
	ACharacter* C = GetCharacter();
	UAnimInstance* Anim = C ? C->GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim || IsTraversing())
	{
		return;
	}
	const UCharacterMovementComponent* Move = C->GetCharacterMovement();
	const bool bFalling = Move->IsFalling();
	// Montagem de outro sistema (ataque, esquiva, reação) tem prioridade.
	UAnimMontage* Current = Anim->GetCurrentActiveMontage();
	const bool bOurs = Current == nullptr || Current == AirMontage || Current == Land || Current == LandHard
		|| LandMoveDir.Contains(Current);

	auto Play = [&](UAnimMontage* M, bool bLoop)
	{
		if (!M || !bOurs)
		{
			return;
		}
		Anim->Montage_Play(M, 1.f);
		if (bLoop)
		{
			const FName S = M->GetSectionName(0);
			Anim->Montage_SetNextSection(S, S, M);
		}
		AirMontage = M;
	};

	if (bFalling)
	{
		if (!bWasFalling)
		{
			AirTime = 0.f;
			if (C->GetVelocity().Z > 50.f)
			{
				Play(C->GetVelocity().Size2D() < 100.f ? JumpStand.Get() : Directional(JumpDir), false);
			}
			else
			{
				Play(Fall, true);
			}
		}
		AirTime += DeltaTime;
		const bool bPlaying = AirMontage && Anim->Montage_IsPlaying(AirMontage);
		if (AirTime > FlailAirTime && AirMontage != FallFlail)
		{
			Play(FallFlail, true);
		}
		else if (!bPlaying && AirMontage != FallFlail)
		{
			Play(Fall, true);
		}
	}
	else if (bWasFalling)
	{
		if (AirTime > 0.15f)
		{
			// Chegando em movimento, pouso curto que já emenda na corrida; parado, pouso normal ou pesado.
			const bool bMoving = C->GetVelocity().Size2D() > 100.f || !Move->GetCurrentAcceleration().IsNearlyZero();
			UAnimMontage* L = bMoving ? Directional(LandMoveDir) : AirTime > HardLandAirTime ? LandHard.Get() : Land.Get();
			if (AirMontage && Anim->Montage_IsPlaying(AirMontage))
			{
				Anim->Montage_Stop(0.1f, AirMontage);
			}
			Play(L, false);
		}
		else if (AirMontage && Anim->Montage_IsPlaying(AirMontage))
		{
			Anim->Montage_Stop(0.15f, AirMontage);
		}
		AirMontage = nullptr;
	}
	else if ((Current == Land || Current == LandHard) && !Move->GetCurrentAcceleration().IsNearlyZero()
		&& Anim->Montage_GetPosition(Current) > 0.15f)
	{
		// Pouso parado interrompido por input: devolve as pernas para a locomoção (senão o corpo desliza).
		Anim->Montage_Stop(0.2f, Current);
	}
	bWasFalling = bFalling;
}

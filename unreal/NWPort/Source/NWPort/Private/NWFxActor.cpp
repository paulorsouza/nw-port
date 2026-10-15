#include "NWFxActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "NWCombatComponent.h"

ANWFxActor::ANWFxActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ANWFxActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANWFxActor, Type);
	DOREPLIFETIME(ANWFxActor, Radius);
	DOREPLIFETIME(ANWFxActor, Seconds);
}

ANWFxActor* ANWFxActor::Spawn(UWorld* World, ENWFx InType, const FVector& At, float InRadius, float InSeconds, UNWCombatComponent* InSource)
{
	if (!World)
	{
		return nullptr;
	}
	const FTransform Where(At);
	ANWFxActor* Fx = World->SpawnActorDeferred<ANWFxActor>(ANWFxActor::StaticClass(), Where, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Fx)
	{
		Fx->Type = InType;
		Fx->Radius = InRadius;
		Fx->Seconds = InSeconds;
		Fx->Source = InSource;
		Fx->FinishSpawning(Where);
	}
	return Fx;
}

void ANWFxActor::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(Seconds + 0.2f);
	if (GetNetMode() != NM_DedicatedServer)
	{
		Build();
	}
}

void ANWFxActor::Build()
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NW/Materials/M_NW_FX.M_NW_FX"));
	Mat = Base ? UMaterialInstanceDynamic::Create(Base, this) : nullptr;
	if (Mat)
	{
		const FLinearColor C = Type == ENWFx::PoisonCloud ? FLinearColor(0.25f, 0.9f, 0.2f) : Type == ENWFx::Explosion ? FLinearColor(1.f, 0.55f, 0.15f)
			: FLinearColor(1.f, 0.85f, 0.6f);
		Mat->SetVectorParameterValue(TEXT("Color"), C);
		Mat->SetScalarParameterValue(TEXT("Intensity"), Type == ENWFx::PoisonCloud ? 0.6f : 3.f);
	}
	auto Make = [this](const TCHAR* MeshPath)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		if (Mat)
		{
			C->SetMaterial(0, Mat);
		}
		return C;
	};
	if (Type == ENWFx::ArrowRain)
	{
		// Arrows falling from ~15 m at random points in the circle, all landing at the end (the Rain's hit).
		for (int32 i = 0; i < 24; ++i)
		{
			UStaticMeshComponent* A = Make(TEXT("/Game/NW/sword_shield/Mesh/SM_arrow.SM_arrow"));
			const FVector2D P = FMath::RandPointInCircle(Radius);
			ArrowStart.Add(FVector(P.X + FMath::FRandRange(-150.f, 150.f), P.Y + FMath::FRandRange(-150.f, 150.f), 1500.f));
			A->SetRelativeLocation(ArrowStart.Last());
			A->SetRelativeRotation(FRotator(-80.f, FMath::FRandRange(0.f, 360.f), 0.f));
			A->SetMaterial(0, nullptr);
			Arrows.Add(A);
		}
		return;
	}
	Shape = Make(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Shape->SetRelativeScale3D(FVector(0.01f));
}

void ANWFxActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / FMath::Max(Seconds, 0.01f), 0.f, 1.f);
	if (Shape)
	{
		// Sphere of 100 cm diameter at scale 1.
		const float R = Type == ENWFx::Explosion ? Radius * FMath::Sqrt(FMath::Min(1.f, T * 2.f)) : Radius * (0.9f + 0.1f * FMath::Sin(Age * 3.f));
		Shape->SetRelativeScale3D(FVector(R / 50.f, R / 50.f, (Type == ENWFx::PoisonCloud ? 0.45f : 1.f) * R / 50.f));
		if (Mat)
		{
			const float Fade = Type == ENWFx::Explosion ? 1.f - T : FMath::Min(1.f, (1.f - T) * 4.f);
			Mat->SetScalarParameterValue(TEXT("Intensity"), (Type == ENWFx::PoisonCloud ? 0.6f : 3.f) * Fade);
		}
	}
	for (int32 i = 0; i < Arrows.Num(); ++i)
	{
		// Each arrow falls over the whole duration (all of them touch the ground at the end).
		const float Lag = (i % 6) * 0.04f;
		const float K = FMath::Clamp((T - Lag) / (1.f - Lag), 0.f, 1.f);
		Arrows[i]->SetRelativeLocation(FMath::Lerp(ArrowStart[i], FVector(ArrowStart[i].X * 0.95f, ArrowStart[i].Y * 0.95f, 0.f), K * K));
	}
	// Poison cloud: on the server, every tick, poisons whoever is inside (except the shooter).
	if (Type == ENWFx::PoisonCloud && HasAuthority() && !StatusID.IsNone() && Age >= NextApply)
	{
		NextApply = Age + 0.5f;
		UNWCombatComponent* From = Source.Get();
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(NWCloud), false, From ? From->GetOwner() : nullptr);
		GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Params);
		TSet<AActor*> Done;
		for (const FOverlapResult& O : Overlaps)
		{
			AActor* Other = O.GetActor();
			if (!Other || Done.Contains(Other) || (From && Other == From->GetOwner()))
			{
				continue;
			}
			Done.Add(Other);
			if (UNWCombatComponent* Target = Other->FindComponentByClass<UNWCombatComponent>())
			{
				Target->ApplyStatus(StatusID, StatusSeconds, StatusTick, StatusDamage, 1, 0.f, From);
			}
		}
	}
}

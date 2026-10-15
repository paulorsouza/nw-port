#include "NWProjectile.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "NWCharacter.h"
#include "NWCombatComponent.h"
#include "NWFxActor.h"
#include "Materials/MaterialInterface.h"

ANWProjectile::ANWProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(22.f);
	Collision->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	RootComponent = Collision;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Trail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Trail"));
	Trail->SetupAttachment(Collision);
	Trail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Trail->SetCastShadow(false);
	Trail->SetVisibility(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;
	Movement->ProjectileGravityScale = 0.15f;
	Movement->bRotationFollowsVelocity = false;
}

void ANWProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANWProjectile, ReplicatedMesh);
	DOREPLIFETIME(ANWProjectile, bArrow);
	DOREPLIFETIME(ANWProjectile, FxPath);
}

void ANWProjectile::Setup(UNWCombatComponent* InOwnerCombat, FName InDamageID, UStaticMesh* Mesh, const FNWProjectileParams& Params)
{
	OwnerCombat = InOwnerCombat;
	DamageID = InDamageID;
	P = Params;
	ReplicatedMesh = Mesh;
	Visual->SetStaticMesh(Mesh);
	FxPath = Params.FxPath;
	bArrow = Params.Speed > 0.f;
	if (bArrow)
	{
		// Arrow: row speed, no gravity until GravityDelay, row radius, points where it flies.
		Speed = Params.Speed;
		Movement->InitialSpeed = Speed;
		Movement->MaxSpeed = Speed;
		Movement->ProjectileGravityScale = Params.GravityDelay > 0.f ? 0.f : 1.f;
		Movement->bRotationFollowsVelocity = true;
		Collision->SetSphereRadius(FMath::Max(Params.Radius, 5.f));
		SpinDegreesPerSecond = 0.f;
		if (Mesh)
		{
			// Longest axis of the mesh (the arrow body) forward (X).
			const FVector E = Mesh->GetBoundingBox().GetExtent();
			Visual->SetRelativeRotation(E.Z >= E.X && E.Z >= E.Y ? FRotator(-90.f, 0.f, 0.f) : E.Y >= E.X ? FRotator(0.f, -90.f, 0.f) : FRotator::ZeroRotator);
		}
	}
	else
	{
		// Blade forward: the weapon mesh has the handle at -Y and the tip at +Z (same correction as the hand).
		Visual->SetRelativeRotation(FRotator(0.f, 0.f, -90.f));
	}
	SetLifeSpan(FMath::Max(Params.Range, 500.f) / Speed + 0.5f);
	if (InOwnerCombat)
	{
		Collision->IgnoreActorWhenMoving(InOwnerCombat->GetOwner(), true);
	}
}

void ANWProjectile::OnRep_Mesh()
{
	Visual->SetStaticMesh(ReplicatedMesh);
}

void ANWProjectile::BeginPlay()
{
	Super::BeginPlay();
	// Game effect (trail and crystal) in place of the mesh and the simple trail; collision and movement stay the same.
	UNiagaraSystem* Niagara = !FxPath.IsEmpty() && GetNetMode() != NM_DedicatedServer ? LoadObject<UNiagaraSystem>(nullptr, *FxPath) : nullptr;
	if (Niagara)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(Niagara, RootComponent, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true);
		Visual->SetVisibility(false);
	}
	if (ReplicatedMesh)
	{
		Visual->SetStaticMesh(ReplicatedMesh);
	}
	if (bArrow && !Niagara && GetNetMode() != NM_DedicatedServer)
	{
		// Trail: thin glowing 2.5 m cylinder behind the arrow (cylinder axis = Z; laid backwards).
		Trail->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		if (UMaterialInterface* Fx = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NW/Materials/M_NW_FX.M_NW_FX")))
		{
			Trail->SetMaterial(0, Fx);
		}
		Trail->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
		Trail->SetRelativeLocation(FVector(-125.f, 0.f, 0.f));
		Trail->SetRelativeScale3D(FVector(0.012f, 0.012f, 2.5f));
		Trail->SetVisibility(true);
		// Arrow bigger than the game's mesh, to be seen in flight (the game has the particle trail).
		Visual->SetRelativeScale3D(FVector(1.3f));
	}
	StartLocation = PrevLocation = GetActorLocation();
	if (HasAuthority())
	{
		Collision->OnComponentBeginOverlap.AddDynamic(this, &ANWProjectile::OnOverlap);
		Movement->OnProjectileStop.AddDynamic(this, &ANWProjectile::OnStop);
	}
}

void ANWProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bArrow)
	{
		// Spins around the lateral axis, like a thrown Hatchet.
		Visual->AddLocalRotation(FRotator(0.f, 0.f, SpinDegreesPerSecond * DeltaSeconds));
	}
	// Arrow: straight until GravityDelay, then full gravity.
	if (bArrow && P.GravityDelay > 0.f && Movement->ProjectileGravityScale == 0.f
		&& FVector::Dist(StartLocation, GetActorLocation()) >= P.GravityDelay)
	{
		Movement->ProjectileGravityScale = 1.f;
	}
	PrevLocation = GetActorLocation();
}

bool ANWProjectile::IsHeadHit(const AActor* Target) const
{
	// The game's head volume: 0.12 m sphere on the "hat" bone. Measures the distance from the bone to the segment flown this
	// frame (the overlap happens at the capsule's edge, far from the head center).
	const ACharacter* Char = Cast<ACharacter>(Target);
	const USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	if (!Mesh)
	{
		return false;
	}
	const FName Bone = Mesh->DoesSocketExist(TEXT("hat")) ? FName(TEXT("hat")) : Mesh->DoesSocketExist(TEXT("Head")) ? FName(TEXT("Head")) : NAME_None;
	if (Bone.IsNone())
	{
		return false;
	}
	const FVector HeadPos = Mesh->GetSocketLocation(Bone);
	const FVector From = PrevLocation;
	const FVector To = GetActorLocation() + Movement->Velocity * 0.05f;
	const FVector Closest = FMath::ClosestPointOnSegment(HeadPos, From, To);
	return FVector::Dist(Closest, HeadPos) <= 12.f + Collision->GetScaledSphereRadius();
}

void ANWProjectile::OnOverlap(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Hit)
{
	UNWCombatComponent* From = OwnerCombat.Get();
	const ANWCharacter* Target = Cast<ANWCharacter>(Other);
	if (bDone || !From || !Target || Other == From->GetOwner() || HitActors.Contains(Other))
	{
		return;
	}
	HitActors.Add(Other);
	From->ProjectileHit(Target->Combat, DamageID, GetActorLocation(), bArrow && IsHeadHit(Target));
	Impact(GetActorLocation());
	if (!P.bPierce)
	{
		bDone = true;
		Destroy();
	}
}

void ANWProjectile::OnStop(const FHitResult& Hit)
{
	// Hit the scenery: explodes / releases the cloud and stays stuck for a bit.
	if (!bDone)
	{
		Impact(Hit.ImpactPoint);
	}
	bDone = true;
	Trail->SetVisibility(false);
	SetLifeSpan(bArrow ? 8.f : 1.0f);
}

void ANWProjectile::Impact(const FVector& At)
{
	UNWCombatComponent* From = OwnerCombat.Get();
	if (P.ExplodeRadius > 0.f && From)
	{
		From->ExplodeAt(At, P.ExplodeRadius, P.ExplodeDamageID, nullptr);
		ANWFxActor::Spawn(GetWorld(), ENWFx::Explosion, At, P.ExplodeRadius, 0.35f);
		P.ExplodeRadius = 0.f;   // explodes once
	}
	if (P.CloudRadius > 0.f)
	{
		if (ANWFxActor* Cloud = ANWFxActor::Spawn(GetWorld(), ENWFx::PoisonCloud, At, P.CloudRadius, P.CloudSeconds, From))
		{
			Cloud->StatusID = P.StatusID;
			Cloud->StatusSeconds = P.StatusSeconds;
			Cloud->StatusTick = P.StatusTick;
			Cloud->StatusDamage = P.StatusDamage;
		}
		P.CloudRadius = 0.f;
	}
}

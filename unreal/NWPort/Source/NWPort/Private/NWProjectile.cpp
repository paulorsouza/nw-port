#include "NWProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "NWCharacter.h"
#include "NWCombatComponent.h"

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
}

void ANWProjectile::Setup(UNWCombatComponent* InOwnerCombat, FName InDamageID, UStaticMesh* Mesh, float Range)
{
	OwnerCombat = InOwnerCombat;
	DamageID = InDamageID;
	ReplicatedMesh = Mesh;
	Visual->SetStaticMesh(Mesh);
	// Lâmina para a frente: a malha da arma tem o cabo em -Y e a ponta em +Z (mesma correção da mão).
	Visual->SetRelativeRotation(FRotator(0.f, 0.f, -90.f));
	SetLifeSpan(FMath::Max(Range, 500.f) / Speed + 0.1f);
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
	if (ReplicatedMesh)
	{
		Visual->SetStaticMesh(ReplicatedMesh);
	}
	if (HasAuthority())
	{
		Collision->OnComponentBeginOverlap.AddDynamic(this, &ANWProjectile::OnOverlap);
		Movement->OnProjectileStop.AddDynamic(this, &ANWProjectile::OnStop);
	}
}

void ANWProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Gira em torno do eixo lateral, como uma machadinha arremessada.
	Visual->AddLocalRotation(FRotator(0.f, 0.f, SpinDegreesPerSecond * DeltaSeconds));
}

void ANWProjectile::OnOverlap(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Hit)
{
	UNWCombatComponent* From = OwnerCombat.Get();
	const ANWCharacter* Target = Cast<ANWCharacter>(Other);
	if (bDone || !From || !Target || Other == From->GetOwner())
	{
		return;
	}
	bDone = true;
	From->ProjectileHit(Target->Combat, DamageID, GetActorLocation());
	Destroy();
}

void ANWProjectile::OnStop(const FHitResult& Hit)
{
	// Bateu no cenário: some logo depois.
	bDone = true;
	SetLifeSpan(1.0f);
}

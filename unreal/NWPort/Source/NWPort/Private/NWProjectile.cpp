#include "NWProjectile.h"

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
}

void ANWProjectile::Setup(UNWCombatComponent* InOwnerCombat, FName InDamageID, UStaticMesh* Mesh, const FNWProjectileParams& Params)
{
	OwnerCombat = InOwnerCombat;
	DamageID = InDamageID;
	P = Params;
	ReplicatedMesh = Mesh;
	Visual->SetStaticMesh(Mesh);
	bArrow = Params.Speed > 0.f;
	if (bArrow)
	{
		// Flecha: velocidade da linha, sem gravidade até GravityDelay, raio da linha, aponta para onde voa.
		Speed = Params.Speed;
		Movement->InitialSpeed = Speed;
		Movement->MaxSpeed = Speed;
		Movement->ProjectileGravityScale = Params.GravityDelay > 0.f ? 0.f : 1.f;
		Movement->bRotationFollowsVelocity = true;
		Collision->SetSphereRadius(FMath::Max(Params.Radius, 5.f));
		SpinDegreesPerSecond = 0.f;
		if (Mesh)
		{
			// Eixo mais comprido da malha (o corpo da flecha) para a frente (X).
			const FVector E = Mesh->GetBoundingBox().GetExtent();
			Visual->SetRelativeRotation(E.Z >= E.X && E.Z >= E.Y ? FRotator(-90.f, 0.f, 0.f) : E.Y >= E.X ? FRotator(0.f, -90.f, 0.f) : FRotator::ZeroRotator);
		}
	}
	else
	{
		// Lâmina para a frente: a malha da arma tem o cabo em -Y e a ponta em +Z (mesma correção da mão).
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
	if (ReplicatedMesh)
	{
		Visual->SetStaticMesh(ReplicatedMesh);
	}
	if (bArrow && GetNetMode() != NM_DedicatedServer)
	{
		// Rastro: cilindro fino e luminoso de 2,5 m atrás da flecha (eixo do cilindro = Z; deitado para trás).
		Trail->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		if (UMaterialInterface* Fx = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NW/Materials/M_NW_FX.M_NW_FX")))
		{
			Trail->SetMaterial(0, Fx);
		}
		Trail->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
		Trail->SetRelativeLocation(FVector(-125.f, 0.f, 0.f));
		Trail->SetRelativeScale3D(FVector(0.012f, 0.012f, 2.5f));
		Trail->SetVisibility(true);
		// Flecha maior que a malha do jogo, para ser vista em voo (o jogo tem o rastro de partícula).
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
		// Gira em torno do eixo lateral, como uma machadinha arremessada.
		Visual->AddLocalRotation(FRotator(0.f, 0.f, SpinDegreesPerSecond * DeltaSeconds));
	}
	// Flecha: reta até GravityDelay, depois gravidade cheia.
	if (bArrow && P.GravityDelay > 0.f && Movement->ProjectileGravityScale == 0.f
		&& FVector::Dist(StartLocation, GetActorLocation()) >= P.GravityDelay)
	{
		Movement->ProjectileGravityScale = 1.f;
	}
	PrevLocation = GetActorLocation();
}

bool ANWProjectile::IsHeadHit(const AActor* Target) const
{
	// Volume da cabeça do jogo: esfera de 0,12 m no osso "hat". Mede a distância do osso ao trecho voado neste
	// quadro (a sobreposição acontece na borda da cápsula, longe do centro da cabeça).
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
	// Bateu no cenário: explode / solta a nuvem e fica cravada um pouco.
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
		P.ExplodeRadius = 0.f;   // explode uma vez
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

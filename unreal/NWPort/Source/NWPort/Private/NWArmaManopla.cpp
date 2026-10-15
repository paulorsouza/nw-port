// Ice Gauntlet (sub-area 1a): docs/game/weapons/ice-gauntlet.md.
#include "NWArmaManopla.h"
#include "NWCharacter.h"
#include "NWDevRegistry.h"
#include "NWPlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NWCombatComponent.h"
#include "NWManaComponent.h"
#include "NWProjectile.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

static const FName WeaponSkinName(TEXT("WeaponSkin"));

void NWArmaManopla::AddWeaponSet(TArray<FNWWeaponSet>& Sets)
{
	// 1hElementalGauntlet_IceT2..T5 (javelindata_itemdefinitions_weapons): damage 67 (×100/64), crit 9% ×1.3,
	// stagger 43, block 43 / stability 30. Skinned glove (SkinOverride1, .skin on forearm and fingers) and palm
	// crystal (gauntletcore, rigid on handattach, like the Rapier).
	FNWWeaponSet Ice;
	Ice.Id = TEXT("IceGauntlet");
	Ice.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Mesh/SM_icegauntlet_core.SM_icegauntlet_core")));
	Ice.SkinMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Mesh/SK_icegauntlet.SK_icegauntlet")));
	Ice.Actions = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/NW/Data/DT_IceGauntletActions.DT_IceGauntletActions")));
	Ice.Locomotion = TSoftObjectPtr<UBlendSpace>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Anims/BS_Ig_Strafe.BS_Ig_Strafe")));
	Ice.HudIcon = TEXT("lyshineui/images/icons/items/drawing/icegauntlet.dds");
	Ice.BaseDamage = 104.7f; Ice.CritChance = 0.09f; Ice.CritDamageMultiplier = 1.3f;
	Ice.StaggerDamage = 43.f; Ice.BlockStaminaDamage = 43.f; Ice.BlockStability = 30.f;
	// Attack_Start_Charge_UpperBody [MagicGauntlet+Ice]: release before 0.667 s = light (Primary); 0.667 s = heavy.
	Ice.HeavyMinHold = 0.667f; Ice.HeavyFullCharge = 0.667f;
	// The first three of the Tempest tree (the others are swapped on the K screen).
	Ice.Abilities = {TEXT("IceStorm"), TEXT("WindChill"), TEXT("Spikes")};
	Sets.Add(Ice);
}

void NWArmaManopla::ApplySkin(ANWCharacter* Character, const FNWWeaponSet& Set)
{
	USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
	if (!Body)
	{
		return;
	}
	USkeletalMeshComponent* Skin = nullptr;
	TArray<USkeletalMeshComponent*> Comps;
	Character->GetComponents(Comps);
	for (USkeletalMeshComponent* C : Comps)
	{
		if (C->GetFName() == WeaponSkinName)
		{
			Skin = C;
		}
	}
	USkeletalMesh* Mesh = Set.SkinMesh.IsNull() ? nullptr : Set.SkinMesh.LoadSynchronous();
	if (!Mesh)
	{
		if (Skin)
		{
			Skin->SetVisibility(false);
		}
		return;
	}
	if (!Skin)
	{
		Skin = NewObject<USkeletalMeshComponent>(Character, WeaponSkinName);
		Skin->SetupAttachment(Body);
		Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Skin->SetReceivesDecals(false);
		Skin->RegisterComponent();
	}
	Skin->SetSkeletalMesh(Mesh);
	Skin->SetLeaderPoseComponent(Body);
	Skin->SetVisibility(true);
	// Abilities: server-only component, created the first time the gauntlet is equipped.
	if (Character->HasAuthority() && !Character->FindComponentByClass<UNWIceGauntletComponent>())
	{
		UNWIceGauntletComponent* Ice = NewObject<UNWIceGauntletComponent>(Character, TEXT("IceGauntlet"));
		Ice->RegisterComponent();
	}
}

// F10: Ice Gauntlet + sword and shield pair (the F10 pair cycle does not know the gauntlet).
NW_REGISTER_DEV(IceGauntletLoadout, TEXT("armas.icegauntlet_loadout"), TEXT("nw.dev.icegauntlet_loadout"),
	[](ANWPlayerController* PC)
	{
		ANWCharacter* C = PC ? Cast<ANWCharacter>(PC->GetPawn()) : nullptr;
		if (!C)
		{
			return;
		}
		const int32 Ice = C->WeaponSets.IndexOfByPredicate([](const FNWWeaponSet& S) { return S.Id == TEXT("IceGauntlet"); });
		if (Ice != INDEX_NONE)
		{
			C->SetLoadout(Ice, 0);
		}
	});

// ---------------------------------------------------------------- game numbers
// Sources: playeractions_ability_icemagic.actionlist, player_magicgauntlet_ice.adb (procclips), spelltable,
// statuseffects and the game's items/weapons/* slices (extract/weapons_icegauntlet); docs/game/weapons/ice-gauntlet.md §3.
namespace NWIce
{
	// Converted game effects (tools/ue/import_fx.py): items/weapons/* pieces and procclips from the .adb.
	static FString Fx(const TCHAR* Name) { return FString::Printf(TEXT("/Game/NW/FX/NS_%s.NS_%s"), Name, Name); }
	// Spikes: IceMagic_Spell_Spikes NumToSpawn 7, SpawnRate 65 ms, CastDistance 1.1 m (step between spikes, H),
	// CastSpell at 0.2 s; Ice_Spike_Spawn m_radius 0.8 m; Duration 2.5 s.
	constexpr int32 SpikeCount = 7;
	constexpr float SpikeFirst = 0.2f, SpikeEvery = 0.065f, SpikeStep = 110.f, SpikeRadius = 80.f, SpikeLife = 2.5f;
	// Empowered Spike: Spawn_End1..5 at 2.2/3.3/4.4/5.5/6.6 m, radius 1 m, Duration 1 s.
	constexpr float EndDistances[] = {220.f, 330.f, 440.f, 550.f, 660.f};
	constexpr float EndRadius = 100.f, EndLife = 1.f;
	// Wind Chill (WindChill_Aim): CAGE-Damage at 0.067 and every 0.2 s until 2.0 s (11 ticks), exits at 2.0 s.
	// Capsules on aim_direction: near r 0.3 m from 0 to 4 m (half-height 2, UseEndAsCenter); far r 0.4 m centered at
	// 5.5 m with half-height 0.7 (4.8 to 6.2 m).
	constexpr float WindFirst = 0.067f, WindEvery = 0.2f, WindLength = 2.f;
	// Ice Storm: CastDistance 20 m, SpellCastIceStorm marker at 0.2 s, sphere r 5 m height 5 m, 5 s
	// (spell_aoe_blizzard m_timedelayseconds 5), tick 0.33 s, IceMagic_IceStorm −25%.
	constexpr float StormAt = 0.2f, StormRange = 2000.f, StormRadius = 500.f, StormSeconds = 5.f, StormTick = 0.33f;
	// Shower: Shower1..5 at 0.4/0.467/0.533/0.6/0.667 s, CastDistance 2.8/2.25/2/2.25/2.8 m rotated 60/35/0/−35/−60°
	// (CharRelRotOffsetZ), box 0.5 × 0.5 × 3 m, 4 s (spell_aoe_shower). Frostbite −50%, 3 s after leaving; rooted 1 s,
	// immune for 6 s afterwards (IceMagic_IceShower_Freeze / _Cooldown).
	constexpr float ShowerAt[] = {0.4f, 0.467f, 0.533f, 0.6f, 0.667f};
	constexpr float ShowerDist[] = {280.f, 225.f, 200.f, 225.f, 280.f};
	constexpr float ShowerYaw[] = {60.f, 35.f, 0.f, -35.f, -60.f};
	constexpr float ShowerSeconds = 4.f;
	// Pylon: CastSpell at 0.6 s, 2 m ahead (CharRelPosOffsetY), life 15 s (m_lifetime), first shot at 1 s
	// (m_initialcastdelay), then every 1 s (m_cooldown 1, H), range 20 m; 10 s cooldown when it disappears.
	constexpr float PylonAt = 0.6f, PylonAhead = 200.f, PylonLife = 15.f, PylonFirst = 1.f, PylonEvery = 1.f, PylonRange = 2000.f;
	constexpr float PylonCooldown = 10.f;
	// Entombed: tomb 10 s, ManaRate 6 (IceMagic_Entombed_ManaRegen), break = 3 m sphere at 0.033 s; 30 s
	// cooldown on exit (SetCooldownTimer OnExit).
	constexpr float TombSeconds = 10.f, TombManaPerSecond = 6.f, BreakAt = 0.033f, BreakRadius = 300.f, TombCooldown = 30.f;
}

static UMaterialInstanceDynamic* IceMaterial(UObject* Outer, float Intensity)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NW/Materials/M_NW_FX.M_NW_FX"));
	UMaterialInstanceDynamic* M = Base ? UMaterialInstanceDynamic::Create(Base, Outer) : nullptr;
	if (M)
	{
		M->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.45f, 0.75f, 1.f));
		M->SetScalarParameterValue(TEXT("Intensity"), Intensity);
	}
	return M;
}

// ---------------------------------------------------------------- ice area

ANWIceArea::ANWIceArea()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ANWIceArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANWIceArea, Shapes);
	DOREPLIFETIME(ANWIceArea, Seconds);
	DOREPLIFETIME(ANWIceArea, FxPath);
}

void ANWIceArea::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(Seconds + 0.1f);
	if (GetNetMode() != NM_DedicatedServer)
	{
		Build();
	}
}

void ANWIceArea::Build()
{
	if (UNiagaraSystem* Fx = FxPath.IsEmpty() ? nullptr : LoadObject<UNiagaraSystem>(nullptr, *FxPath))
	{
		for (const FNWIceShape& S : Shapes)
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(Fx, RootComponent, NAME_None, S.Center, S.Rotation, EAttachLocation::KeepWorldPosition, true);
		}
		return;
	}
	Mat = IceMaterial(this, 0.5f);
	for (const FNWIceShape& S : Shapes)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, S.bBox ? TEXT("/Engine/BasicShapes/Cube.Cube") : TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		// 100 cm basic meshes; the area sits flush with the ground (thin disc or box).
		C->SetWorldLocationAndRotation(S.Center, S.Rotation);
		C->SetWorldScale3D(S.bBox ? FVector(S.Extent.X / 50.f, S.Extent.Y / 50.f, S.Extent.Z / 50.f)
			: FVector(S.Extent.X / 50.f, S.Extent.X / 50.f, 0.05f));
		if (Mat)
		{
			C->SetMaterial(0, Mat);
		}
		Visuals.Add(C);
	}
}

bool ANWIceArea::Inside(const FVector& P) const
{
	for (const FNWIceShape& S : Shapes)
	{
		const FVector L = S.Rotation.UnrotateVector(P - S.Center);
		if (S.bBox ? (FMath::Abs(L.X) <= S.Extent.X && FMath::Abs(L.Y) <= S.Extent.Y && FMath::Abs(L.Z) <= S.Extent.Z)
			: (FVector2D(L.X, L.Y).Size() <= S.Extent.X && FMath::Abs(L.Z) <= FMath::Max(S.Extent.Z, 250.f)))
		{
			return true;
		}
	}
	return false;
}

void ANWIceArea::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Mat)
	{
		Mat->SetScalarParameterValue(TEXT("Intensity"), 0.5f * FMath::Clamp((Seconds - Age) * 2.f, 0.f, 1.f));
	}
	if (!HasAuthority() || Age < NextTick)
	{
		return;
	}
	NextTick = Age + TickSeconds;
	UNWCombatComponent* From = Source.Get();
	TSet<TWeakObjectPtr<UNWCombatComponent>> Now;
	float MaxR = 0.f;
	for (const FNWIceShape& S : Shapes)
	{
		MaxR = FMath::Max(MaxR, S.Extent.Size());
	}
	FVector Mid = FVector::ZeroVector;
	for (const FNWIceShape& S : Shapes)
	{
		Mid += S.Center / float(FMath::Max(1, Shapes.Num()));
	}
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NWIceArea), false, From ? From->GetOwner() : nullptr);
	GetWorld()->OverlapMultiByChannel(Overlaps, Mid, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(MaxR + 400.f), Params);
	const float T = GetWorld()->GetTimeSeconds();
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Other = O.GetActor();
		UNWCombatComponent* Target = Other ? Other->FindComponentByClass<UNWCombatComponent>() : nullptr;
		if (!Target || Target == From || Now.Contains(Target) || !Inside(Other->GetActorLocation()))
		{
			continue;
		}
		Now.Add(Target);
		if (From && !DamageID.IsNone())
		{
			From->ProjectileHit(Target, DamageID, Other->GetActorLocation());
		}
		if (!SlowID.IsNone())
		{
			Target->ApplyStatus(SlowID, TickSeconds + 0.15f, 1.f, 0.f, 1, SlowMove, From);
		}
		if (!RootID.IsNone() && !InsideNow.Contains(Target) && T >= RootImmuneUntil.FindRef(Target))
		{
			// Rooted: the port does not yet have "no walking" separate from stun; uses the lowest speed (−90%).
			Target->ApplyStatus(RootID, RootSeconds, 1.f, 0.f, 1, -1.f, From);
			RootImmuneUntil.Add(Target, T + RootSeconds + RootImmunity);
		}
	}
	for (const TWeakObjectPtr<UNWCombatComponent>& Was : InsideNow)
	{
		if (Was.IsValid() && !Now.Contains(Was) && !LingerID.IsNone())
		{
			Was->ApplyStatus(LingerID, LingerSeconds, 1.f, 0.f, 1, SlowMove, From);
		}
	}
	InsideNow = MoveTemp(Now);
}

// ---------------------------------------------------------------- ice meshes

ANWIceProp::ANWIceProp()
{
	bReplicates = true;
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Visual;
}

void ANWIceProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANWIceProp, MeshPath);
	DOREPLIFETIME(ANWIceProp, FxPaths);
	DOREPLIFETIME(ANWIceProp, bHideMesh);
}

void ANWIceProp::OnRep_Mesh()
{
	Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *MeshPath));
	Visual->SetVisibility(!bHideMesh);
}

void ANWIceProp::OnRep_Fx()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	Visual->SetVisibility(!bHideMesh);
	for (const FString& Path : FxPaths)
	{
		if (UNiagaraSystem* Fx = LoadObject<UNiagaraSystem>(nullptr, *Path))
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(Fx, RootComponent, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget, true);
		}
	}
}

ANWIceProp* ANWIceProp::Spawn(UWorld* World, const TCHAR* InMeshPath, const FTransform& Where, float Seconds,
	const TArray<FString>& Fx, bool bInHideMesh)
{
	ANWIceProp* P = World ? World->SpawnActor<ANWIceProp>(ANWIceProp::StaticClass(), Where) : nullptr;
	if (P)
	{
		P->bHideMesh = bInHideMesh;
		P->MeshPath = InMeshPath;
		P->OnRep_Mesh();
		P->FxPaths = Fx;
		P->OnRep_Fx();
		P->SetLifeSpan(Seconds);
	}
	return P;
}

ANWIcePylon::ANWIcePylon()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ANWIcePylon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UNWCombatComponent* From = Source.Get();
	if (!HasAuthority() || !From || GetWorld()->GetTimeSeconds() < FireAt)
	{
		return;
	}
	FireAt = GetWorld()->GetTimeSeconds() + NWIce::PylonEvery;
	const FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, 150.f);
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NWIcePylon), false, From->GetOwner());
	GetWorld()->OverlapMultiByChannel(Overlaps, Muzzle, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(NWIce::PylonRange), Params);
	AActor* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Other = O.GetActor();
		const UNWCombatComponent* C = Other ? Other->FindComponentByClass<UNWCombatComponent>() : nullptr;
		if (!C || C == From || C->State == ENWCombatState::Dead || Other->IsA<ANWIceProp>())
		{
			continue;
		}
		// No allies: the dummy and mobs are targets; players in the same group are not (ReceiveHit already ignores the damage).
		const float D = FVector::DistSquared(Muzzle, Other->GetActorLocation());
		if (D < BestD)
		{
			BestD = D;
			Best = Other;
		}
	}
	if (!Best)
	{
		return;
	}
	const FRotator Aim = (Best->GetActorLocation() + FVector(0.f, 0.f, 40.f) - Muzzle).Rotation();
	const FTransform Where(Aim, Muzzle);
	ANWProjectile* P = GetWorld()->SpawnActorDeferred<ANWProjectile>(ANWProjectile::StaticClass(), Where, From->GetOwner(), nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (P)
	{
		// IceMagic/Pylon profile (radb): 100 m/s, radius 0.15 m, range 50 m, no drop.
		FNWProjectileParams Pp;
		Pp.Speed = 10000.f;
		Pp.Radius = 15.f;
		Pp.GravityDelay = 10000.f;
		Pp.Range = 5000.f;
		P->Setup(From, TEXT("IceMagic_Pylon"), LoadObject<UStaticMesh>(nullptr, TEXT("/Game/NW/sword_shield/Mesh/SM_icegauntlet_core.SM_icegauntlet_core")), Pp);
		P->FinishSpawning(Where);
	}
}

void ANWIcePylon::EndPlay(const EEndPlayReason::Type Reason)
{
	// SetCooldownTimer OnExit (IceMagic_IcePylon_Cooldown2): the real cooldown starts when the pylon disappears.
	if (HasAuthority() && Reason != EEndPlayReason::EndPlayInEditor && Source.IsValid())
	{
		Source->StartCooldown(TEXT("IcePylon"), NWIce::PylonCooldown);
	}
	Super::EndPlay(Reason);
}

// ---------------------------------------------------------------- abilities

UNWIceGauntletComponent::UNWIceGauntletComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UNWCombatComponent* UNWIceGauntletComponent::Combat() const
{
	return GetOwner()->FindComponentByClass<UNWCombatComponent>();
}

FVector UNWIceGauntletComponent::Forward() const
{
	// Abilities aim through the camera (ApplyRotationDuringCameraLock / AlignToCamera): direction on the plane.
	const APawn* P = Cast<APawn>(GetOwner());
	const FRotator R = P && P->GetController() ? P->GetController()->GetControlRotation() : GetOwner()->GetActorRotation();
	return FRotator(0.f, R.Yaw, 0.f).Vector();
}

FVector UNWIceGauntletComponent::GroundAhead(float MaxCm) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	FVector ViewLoc = GetOwner()->GetActorLocation() + FVector(0, 0, 150);
	FRotator ViewRot = GetOwner()->GetActorRotation();
	if (Pawn && Pawn->GetController())
	{
		Pawn->GetController()->GetPlayerViewPoint(ViewLoc, ViewRot);
	}
	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(NWIceGround), false, GetOwner());
	// Range counted from the character (CastDistance); the ray comes from the camera.
	const FVector End = ViewLoc + ViewRot.Vector() * (MaxCm + FVector::Dist(ViewLoc, GetOwner()->GetActorLocation()));
	FVector Target = GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc, End, ECC_Visibility, P) ? Hit.Location : End;
	const FVector Me = GetOwner()->GetActorLocation();
	const FVector Flat = FVector(Target.X - Me.X, Target.Y - Me.Y, 0.f);
	if (Flat.Size() > MaxCm)
	{
		Target = Me + Flat.GetSafeNormal() * MaxCm + FVector(0, 0, Target.Z - Me.Z);
	}
	if (GetWorld()->LineTraceSingleByChannel(Hit, Target + FVector(0, 0, 300), Target - FVector(0, 0, 3000), ECC_Visibility, P))
	{
		Target = Hit.Location;
	}
	return Target;
}

void UNWIceGauntletComponent::CapsuleHit(const FVector& Start, const FVector& InDir, float FromCm, float ToCm, float RadiusCm, FName DamageID, TSet<AActor*>& Hit)
{
	UNWCombatComponent* C = Combat();
	const float Half = (ToCm - FromCm) * 0.5f;
	const FVector Center = Start + InDir * (FromCm + Half);
	const FQuat Rot = FRotationMatrix::MakeFromZ(InDir).ToQuat();
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NWIceCapsule), false, GetOwner());
	GetWorld()->OverlapMultiByChannel(Overlaps, Center, Rot, ECC_Pawn, FCollisionShape::MakeCapsule(RadiusCm, Half + RadiusCm), Params);
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Other = O.GetActor();
		UNWCombatComponent* Target = Other ? Other->FindComponentByClass<UNWCombatComponent>() : nullptr;
		if (Target && C && Target != C && !Hit.Contains(Other))
		{
			Hit.Add(Other);
			C->ProjectileHit(Target, DamageID, Other->GetActorLocation());
		}
	}
}

void UNWIceGauntletComponent::SpawnSpike(float DistanceCm, bool bEnd)
{
	UNWCombatComponent* C = Combat();
	FVector At = Origin + Dir * DistanceCm;
	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(NWIceSpike), false, GetOwner());
	FVector Ground = At - FVector(0, 0, 90);
	if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 150), At - FVector(0, 0, 400), ECC_Visibility, P))
	{
		Ground = Hit.Location;
	}
	// Ice_Spike_Spawn: IceSpike_sm (crystal, shards, vapor, decal); Ice_Spike_End: IceSpikeOnly_lg. The effect brings the
	// crystal: the static mesh stays hidden.
	ANWIceProp::Spawn(GetWorld(), bEnd ? TEXT("/Game/NW/sword_shield/Mesh/SM_icespike_end.SM_icespike_end")
		: TEXT("/Game/NW/sword_shield/Mesh/SM_icespike.SM_icespike"), FTransform(Dir.Rotation(), Ground), bEnd ? NWIce::EndLife : NWIce::SpikeLife,
		{NWIce::Fx(bEnd ? TEXT("wfxo_IceGaunt_Tempest_IceSpikeOnly_lg") : TEXT("wfxo_IceGaunt_Tempest_IceSpike_sm"))}, true);
	if (C)
	{
		C->ExplodeAt(Ground + FVector(0, 0, 90), bEnd ? NWIce::EndRadius : NWIce::SpikeRadius,
			bEnd ? FName(TEXT("IceMagic_Spikes_End")) : FName(TEXT("IceMagic_Spikes")), nullptr);
	}
}

void UNWIceGauntletComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UNWCombatComponent* C = Combat();
	if (!C || !GetOwner()->HasAuthority())
	{
		return;
	}
	const FName Now = C->GetCurrentAction();
	if (Now != Current)
	{
		const FName Was = Current;
		Current = Now;
		Exit(Was, Now);
		Enter(Now);
	}
	if (!Current.IsNone())
	{
		Step(Current, GetWorld()->GetTimeSeconds() - StartedAt);
	}
}

void UNWIceGauntletComponent::Enter(FName Action)
{
	const float T = GetWorld()->GetTimeSeconds();
	StartedAt = T;
	Done = 0;
	const FString A = Action.ToString();
	if (A == TEXT("Spikes") || A == TEXT("IceShower"))
	{
		Origin = GetOwner()->GetActorLocation();
		Dir = Forward();
	}
	else if (A == TEXT("IceStorm"))
	{
		GroundTarget = GroundAhead(NWIce::StormRange);
	}
	else if (A == TEXT("WindChillAim") || A == TEXT("EntombedLoop"))
	{
		// Phase with the montage looping (its own NextAction, UNWCombatComponent::StartAction): count from here.
		PhaseStartedAt = T;
	}
	if (A == TEXT("EntombedLoop") && !Tomb.IsValid())
	{
		Tomb = ANWIceProp::Spawn(GetWorld(), TEXT("/Game/NW/sword_shield/Mesh/SM_icetomb.SM_icetomb"),
			FTransform(GetOwner()->GetActorRotation(), GetOwner()->GetActorLocation() - FVector(0, 0, 90)), NWIce::TombSeconds + 1.f,
			{NWIce::Fx(TEXT("wfxo_IceGuant_Builder_Entombed_Hld"))});
	}
}

void UNWIceGauntletComponent::Exit(FName Action, FName Next)
{
	UNWCombatComponent* C = Combat();
	const FString A = Action.ToString();
	const FString N = Next.ToString();
	// Spikes: on leaving the cast (natural end or basic attack), the Empowered Spike in front of the last spike.
	if (A == TEXT("Spikes") && (N == TEXT("SpikesOut") || N == TEXT("SpikesEnd")))
	{
		const float Last = NWIce::SpikeStep * FMath::Max(Done, 1);
		float End = NWIce::EndDistances[UE_ARRAY_COUNT(NWIce::EndDistances) - 1];
		if (N == TEXT("SpikesEnd"))
		{
			for (const float D : NWIce::EndDistances)
			{
				if (D >= Last)
				{
					End = D;
					break;
				}
			}
		}
		SpawnSpike(End, true);
	}
	// Entombed: the tomb ends when leaving the loop (end, dodge, break); the cooldown starts here.
	if (A == TEXT("EntombedLoop") && N != TEXT("EntombedLoop"))
	{
		if (Tomb.IsValid())
		{
			Tomb->Destroy();
		}
		if (C)
		{
			C->StartCooldown(TEXT("Entombed"), NWIce::TombCooldown);
		}
	}
	if (A == TEXT("Entombed") && N != TEXT("EntombedLoop") && C)
	{
		C->StartCooldown(TEXT("Entombed"), NWIce::TombCooldown);
	}
}

void UNWIceGauntletComponent::Step(FName Action, float T)
{
	UNWCombatComponent* C = Combat();
	const FString A = Action.ToString();
	if (A == TEXT("Spikes"))
	{
		while (Done < NWIce::SpikeCount && T >= NWIce::SpikeFirst + NWIce::SpikeEvery * Done)
		{
			++Done;
			SpawnSpike(NWIce::SpikeStep * Done, false);
		}
	}
	else if (A == TEXT("WindChillAim"))
	{
		const float P = GetWorld()->GetTimeSeconds() - PhaseStartedAt;
		const ACharacter* Char = Cast<ACharacter>(GetOwner());
		const FVector Joint = Char && Char->GetMesh() ? Char->GetMesh()->GetSocketLocation(TEXT("aim_direction")) : GetOwner()->GetActorLocation();
		// Ticks at 0.067 s and then 0.2; 0.4 … 2.0 s (11).
		while (Done < 11 && P >= (Done == 0 ? NWIce::WindFirst : NWIce::WindEvery * Done))
		{
			++Done;
			TSet<AActor*> Near, Far;
			const FVector D = Forward();
			CapsuleHit(Joint, D, 0.f, 400.f, 30.f, TEXT("IceMagic_WindChill"), Near);
			CapsuleHit(Joint, D, 480.f, 620.f, 40.f, TEXT("IceMagic_WindChill_End"), Far);
		}
		if (P >= NWIce::WindLength && C)
		{
			C->ForceAction(TEXT("WindChillOutro"));
		}
	}
	else if (A == TEXT("IceStorm"))
	{
		if (Done == 0 && T >= NWIce::StormAt)
		{
			Done = 1;
			ANWIceArea* Area = GetWorld()->SpawnActorDeferred<ANWIceArea>(ANWIceArea::StaticClass(), FTransform(GroundTarget), nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Area)
			{
				FNWIceShape S;
				S.Center = GroundTarget;
				S.Extent = FVector(NWIce::StormRadius, 0.f, 500.f);
				Area->Shapes.Add(S);
				Area->Seconds = NWIce::StormSeconds;
				Area->Source = C;
				Area->DamageID = TEXT("IceMagic_IceStorm");
				Area->TickSeconds = NWIce::StormTick;
				Area->SlowID = TEXT("IceMagic_IceStorm");
				Area->SlowMove = -0.25f;
				// spell_aoe_blizzard: wfxo_IceGaunt_Tempest.Blizzard_AOE (docs/tech/particles.md).
				Area->FxPath = TEXT("/Game/NW/FX/NS_wfxo_IceGaunt_Tempest_Blizzard_AOE.NS_wfxo_IceGaunt_Tempest_Blizzard_AOE");
				Area->FinishSpawning(FTransform(GroundTarget));
			}
		}
	}
	else if (A == TEXT("IceShower"))
	{
		if (Done < 5 && T >= NWIce::ShowerAt[Done])
		{
			// One area with the 5 boxes (each spawns at its own time; damage/status applies from the first one).
			if (Done == 0)
			{
				ANWIceArea* Area = GetWorld()->SpawnActorDeferred<ANWIceArea>(ANWIceArea::StaticClass(), FTransform(Origin), nullptr, nullptr,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (Area)
				{
					for (int32 i = 0; i < 5; ++i)
					{
						const FVector D = FRotator(0.f, NWIce::ShowerYaw[i], 0.f).RotateVector(Dir);
						FNWIceShape S;
						S.Center = Origin + D * NWIce::ShowerDist[i];
						S.Rotation = D.Rotation();
						S.Extent = FVector(25.f, 25.f, 150.f);
						S.bBox = true;
						Area->Shapes.Add(S);
					}
					Area->Seconds = NWIce::ShowerSeconds;
					Area->Source = C;
					Area->TickSeconds = 0.1f;
					Area->SlowID = TEXT("IceMagic_Frostbite");
					Area->SlowMove = -0.5f;
					Area->RootID = TEXT("IceMagic_IceShower_Freeze");
					Area->RootSeconds = 1.f;
					Area->RootImmunity = 6.f;
					Area->LingerID = TEXT("IceMagic_Frostbite_Dur3");
					// spell_aoe_shower: wfxo_IceGuant_Builder.IceShower_AOE on each box.
					Area->FxPath = NWIce::Fx(TEXT("wfxo_IceGuant_Builder_IceShower_AOE"));
					Area->LingerSeconds = 3.f;
					Area->FinishSpawning(FTransform(Origin));
				}
			}
			Done = 5;
		}
	}
	else if (A == TEXT("IcePylon"))
	{
		if (Done == 0 && T >= NWIce::PylonAt)
		{
			Done = 1;
			// RemoveExcessAbilityInstances: one pylon at a time.
			if (Pylon.IsValid())
			{
				Pylon->Destroy();
			}
			FVector At = GetOwner()->GetActorLocation() + Forward() * NWIce::PylonAhead;
			FHitResult Hit;
			FCollisionQueryParams P(SCENE_QUERY_STAT(NWIcePylonGround), false, GetOwner());
			if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 150), At - FVector(0, 0, 1000), ECC_Visibility, P))
			{
				At = Hit.Location;
			}
			ANWIcePylon* New = GetWorld()->SpawnActorDeferred<ANWIcePylon>(ANWIcePylon::StaticClass(), FTransform(Forward().Rotation(), At), nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (New)
			{
				New->Source = C;
				New->MeshPath = TEXT("/Game/NW/sword_shield/Mesh/SM_icepylon.SM_icepylon");
				New->FxPaths = {NWIce::Fx(TEXT("wfxo_IceGuant_Builder_IcePylon_Idle_Main")), NWIce::Fx(TEXT("wfxo_IceGuant_Builder_IcePylon_Idle_Top"))};
				New->FireAt = GetWorld()->GetTimeSeconds() + NWIce::PylonFirst;
				New->FinishSpawning(FTransform(Forward().Rotation(), At));
				New->OnRep_Mesh();
				New->OnRep_Fx();
				New->SetLifeSpan(NWIce::PylonLife);
				Pylon = New;
			}
		}
	}
	else if (A == TEXT("EntombedLoop"))
	{
		const float P = GetWorld()->GetTimeSeconds() - PhaseStartedAt;
		if (UNWManaComponent* M = GetOwner()->FindComponentByClass<UNWManaComponent>())
		{
			M->Add(NWIce::TombManaPerSecond * GetWorld()->GetDeltaSeconds());
		}
		if (C)
		{
			// IceMagic_Entombed NoNav: stopped inside the tomb (−90%, the port's minimum).
			C->ApplyStatus(TEXT("IceMagic_Entombed"), 0.2f, 1.f, 0.f, 1, -1.f, C);
		}
		if (P >= NWIce::TombSeconds && C)
		{
			C->ForceAction(TEXT("EntombedOutro"));
		}
	}
	else if (A == TEXT("EntombedBreak"))
	{
		if (Done == 0 && T >= NWIce::BreakAt && C)
		{
			Done = 1;
			C->ExplodeAt(GetOwner()->GetActorLocation(), NWIce::BreakRadius, TEXT("IceMagic_Entombed_Break"), nullptr);
		}
	}
}

#include "NWCreature.h"
#include "NWPlayerGrid.h"

#include "Animation/BlendSpace.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "NWBehaviorTreeComponent.h"
#include "NWBrainComponent.h"
#include "NWCombatComponent.h"
#include "NWSpawnField.h"
#include "NWTraversalComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FNWCreatureCatalog& FNWCreatureCatalog::Get()
{
	static FNWCreatureCatalog Catalog;
	return Catalog;
}

void FNWCreatureCatalog::Load(const FString& File)
{
	if (Loaded.Contains(File))
	{
		return;
	}
	Loaded.Add(File);
	FString Raw;
	TSharedPtr<FJsonObject> Root;
	const TSharedPtr<FJsonObject>* Creatures = nullptr;
	if (!FFileHelper::LoadFileToString(Raw, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root)
		|| !Root.IsValid() || !Root->TryGetObjectField(TEXT("creatures"), Creatures))
	{
		return;
	}
	for (const auto& Pair : (*Creatures)->Values)
	{
		const TSharedPtr<FJsonObject> O = Pair.Value->AsObject();
		FNWCreatureDef D;
		D.Id = FName(*Pair.Key);
		D.NameKey = O->GetStringField(TEXT("name_key"));
		D.Set = FName(O->GetStringField(TEXT("set")));
		D.Level = O->GetIntegerField(TEXT("level"));
		D.Health = O->GetNumberField(TEXT("health"));
		D.Damage = O->GetNumberField(TEXT("damage"));
		D.Mitigation = O->GetNumberField(TEXT("mitigation"));
		D.Scale = O->GetNumberField(TEXT("scale"));
		D.bHostile = O->GetBoolField(TEXT("hostile"));
		const TSharedPtr<FJsonObject>* Skin = nullptr;
		if (O->TryGetObjectField(TEXT("skin"), Skin))
		{
			D.SkinLoot = FName((*Skin)->GetStringField(TEXT("loot")));
			D.SkinTime = (*Skin)->GetNumberField(TEXT("time"));
			D.SkinTradeskill = FName((*Skin)->GetStringField(TEXT("tradeskill")));
			D.SkinTradeXp = int32((*Skin)->GetNumberField(TEXT("trade_xp")));
			D.SkinNameKey = (*Skin)->GetStringField(TEXT("name_key"));
		}
		Defs.Add(D.Id, D);
	}
}

namespace
{
	// Per set: capsule (radius, half height, before the game's scale), reach and attacks, speeds.
	struct FSetTuning
	{
		float Radius, HalfHeight, AttackRange, Walk, Chase;
		TArray<FName> Attacks;
		ENWBrainStyle Style;
		TArray<FNWBrainAttack> Brain;
	};
	FNWBrainAttack Atk(const TCHAR* Action, float Min, float Max, float Weight, float Cooldown = 0.f, bool bGap = false, TArray<FName> Then = {})
	{
		FNWBrainAttack A;
		A.Action = Action; A.MinRange = Min; A.MaxRange = Max; A.Weight = Weight; A.Cooldown = Cooldown; A.bGapCloser = bGap;
		A.FollowUps = MoveTemp(Then);
		return A;
	}
	const FSetTuning* Tuning(FName Set)
	{
		// Boar: gore up close; from afar it winds up (snorts and paws) and charges in a line, then brakes exposed.
		// Wolf: a pack that surrounds from the flanks; bites up close, mid-range pounce, leap from afar.
		static const TMap<FName, FSetTuning> T = {
			{TEXT("boar"), {42.f, 42.f, 240.f, 140.f, 450.f, {TEXT("Attack1"), TEXT("Attack1"), TEXT("Heavy")}, ENWBrainStyle::Charger,
				{Atk(TEXT("Attack1"), 0.f, 250.f, 3.f),
				 Atk(TEXT("ChargeStart"), 500.f, 1200.f, 2.f, 7.f, true, {TEXT("Heavy"), TEXT("Heavy"), TEXT("ChargeEnd")})}}},
			{TEXT("wolf"), {36.f, 42.f, 220.f, 160.f, 520.f, {TEXT("Attack1"), TEXT("Attack2"), TEXT("Heavy"), TEXT("Leap")}, ENWBrainStyle::Pack,
				{Atk(TEXT("Attack1"), 0.f, 230.f, 2.f), Atk(TEXT("Attack2"), 0.f, 230.f, 1.f, 3.f),
				 Atk(TEXT("Heavy"), 180.f, 450.f, 1.f, 4.f, true), Atk(TEXT("Leap"), 450.f, 800.f, 1.f, 6.f, true)}}},
		};
		return T.Find(Set);
	}
	constexpr float LeashRange = 4000.f;   // farther than this from home, gives up and goes back
}

ANWCreature::ANWCreature(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDummyAttacks = true;
	DummyAttackInterval = 2.5f;
	Traversal->bEnabled = false;
	WeaponSets.Reset();
	Sword->SetVisibility(false);
	Shield->SetVisibility(false);
	Stowed->SetVisibility(false);
	Combat->LightComboStart = TEXT("Attack1");
	Combat->StaggerResistRating = 10.f;   // boar and wolf (GritComponent of the slices)
	Behavior = CreateDefaultSubobject<UNWBehaviorTreeComponent>(TEXT("Behavior"));
	AnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/NW/Creatures/ABP_NW_Creature.ABP_NW_Creature_C")));
}

void ANWCreature::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANWCreature, CreatureId);
	DOREPLIFETIME(ANWCreature, bSkinned);
}

void ANWCreature::Setup(const FNWCreatureDef& D, const TArray<FVector>& InPath, const TArray<float>& InWaits)
{
	CreatureId = D.Id;
	Path = InPath;
	Waits = InWaits;
	Combat->MaxHealth = D.Health;
	Combat->Health = D.Health;
	Combat->WeaponBaseDamage = D.Damage;
	Combat->ArmorMitigation = FMath::Clamp(D.Mitigation, 0.f, 0.9f);
	AggroRange = D.bHostile ? 1200.f : 0.f;
	if (const FSetTuning* T = Tuning(D.Set))
	{
		DummyAttackRange = T->AttackRange;
		DummyAttackActions = T->Attacks;
		WalkSpeed = T->Walk;
		ChaseSpeed = T->Chase;
		RunSpeed = T->Chase;
		// With the game's tree (Data/AI/<set>.json) the tree is in charge; without one, the port's brain.
		bUseBehaviorTree = FNWBTAsset::Get(D.Set) != nullptr;
		if (bUseBehaviorTree)
		{
			bDummyAttacks = false;
		}
		else if (T->Brain.Num())
		{
			UseBrain(static_cast<uint8>(T->Style), T->Brain);
		}
	}
	ApplyAssets();
}

const FNWCreatureDef* ANWCreature::Def() const
{
	return FindDef(CreatureId);
}

const FNWCreatureDef* ANWCreature::FindDef(FName Id)
{
	// The catalog (health, damage, set) applies on any map; the spawners only on the region's map (ANWSpawnField).
	FNWCreatureCatalog::Get().Load(FPaths::ProjectDir() / TEXT("Data/World/guardaventos_spawns.json"));
	return FNWCreatureCatalog::Get().Find(Id);
}

void ANWCreature::OnRep_Creature()
{
	ApplyAssets();
}

void ANWCreature::ApplyAssets()
{
	const FNWCreatureDef* D = Def();
	if (!D || D->Set.IsNone() || bAssetsApplied)
	{
		return;
	}
	bAssetsApplied = true;
	const FString Set = D->Set.ToString();
	FString Title = Set;
	Title[0] = FChar::ToUpper(Title[0]);
	MeshAsset = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(FString::Printf(TEXT("/Game/NW/%s/Mesh/SK_%s.SK_%s"), *Set, *Set, *Set)));
	ActionTableAsset = TSoftObjectPtr<UDataTable>(FSoftObjectPath(FString::Printf(TEXT("/Game/NW/Data/DT_%sActions.DT_%sActions"), *Title, *Title)));
	if (const FSetTuning* T = Tuning(D->Set))
	{
		GetCapsuleComponent()->SetCapsuleSize(T->Radius, T->HalfHeight);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -T->HalfHeight));
	}
	SetActorScale3D(FVector(D->Scale));
	LoadMissingAssets();
	// A quadruped is long and the capsule is an upright cylinder: the body (physics asset from the import) also blocks whoever
	// walks (the player) and takes hits in the beast's shape. The camera passes through.
	USkeletalMeshComponent* Body = GetMesh();
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(ECC_Pawn);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetGenerateOverlapEvents(false);
	// The set's walk blend space in the Locomotion variable of the creature ABP (like the player's weapons).
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
	{
		UBlendSpace* BS = TSoftObjectPtr<UBlendSpace>(FSoftObjectPath(FString::Printf(TEXT("/Game/NW/%s/Anims/BS_Walk.BS_Walk"), *Set))).LoadSynchronous();
		for (TFieldIterator<FObjectProperty> It(Anim->GetClass()); It; ++It)
		{
			if (It->GetFName() == TEXT("Locomotion") && It->PropertyClass->IsChildOf(UBlendSpace::StaticClass()))
			{
				It->SetObjectPropertyValue_InContainer(Anim, BS);
			}
		}
	}
}

void ANWCreature::BeginPlay()
{
	ApplyAssets();
	Super::BeginPlay();
	Home = GetActorLocation();
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	if (HasAuthority() && bUseBehaviorTree)
	{
		const FNWCreatureDef* D = Def();
		bUseBehaviorTree = D && Behavior->Start(D->Set, Home, Path);
	}
	// The newly created mesh may not have the AnimInstance in ApplyAssets: tries again next frame.
	GetWorldTimerManager().SetTimerForNextTick([Weak = TWeakObjectPtr<ANWCreature>(this)]()
	{
		if (ANWCreature* Self = Weak.Get())
		{
			Self->bAssetsApplied = false;
			Self->ApplyAssets();
		}
	});
}

bool ANWCreature::HasTarget() const
{
	if (AggroRange <= 0.f)
	{
		return false;
	}
	bool bFound = false;
	if (UNWPlayerGrid* Grid = UNWPlayerGrid::Get(GetWorld()))
	{
		Grid->ForEachNear(GetActorLocation(), AggroRange, [&bFound](ANWCharacter* P)
		{
			bFound |= P->Combat->State != ENWCombatState::Dead;
		});
	}
	return bFound;
}

void ANWCreature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || Combat->State == ENWCombatState::Dead || bUseBehaviorTree)
	{
		return;   // with the game's tree, it handles patrol, chase and leash
	}
	const FNWCreatureDef* D = Def();
	// Too far from home: gives up, walks back and recovers health.
	if (!bReturning && FVector::Dist2D(GetActorLocation(), Home) > LeashRange)
	{
		bReturning = true;
		AggroRange = 0.f;
	}
	if (bReturning)
	{
		const FVector To = (Home - GetActorLocation()).GetSafeNormal2D();
		GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
		AddMovementInput(To, 1.f);
		SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), FRotator(0.f, To.Rotation().Yaw, 0.f), DeltaSeconds, 360.f));
		if (FVector::Dist2D(GetActorLocation(), Home) < 200.f)
		{
			bReturning = false;
			AggroRange = D && D->bHostile ? 1200.f : 0.f;
			Combat->Health = Combat->MaxHealth;
		}
		return;
	}
	if (HasTarget())
	{
		// Chase and attack: UNWBrainComponent (or ANWCharacter::TickDummy). When circling, walks.
		GetCharacterMovement()->MaxWalkSpeed = bWalking ? WalkSpeed : ChaseSpeed;
		return;
	}
	TickPatrol(DeltaSeconds);
}

void ANWCreature::TickPatrol(float DeltaSeconds)
{
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	if (Path.Num() == 0 || Combat->IsBusy())
	{
		return;
	}
	if (WaitLeft > 0.f)
	{
		WaitLeft -= DeltaSeconds;
		return;
	}
	const FVector Target = Path[PathIndex];
	const FVector To = Target - GetActorLocation();
	if (To.Size2D() < 80.f)
	{
		// Waypoint task (graze, sniff...): stands still for the game's time; without a time, a few seconds.
		const float W = Waits.IsValidIndex(PathIndex) ? Waits[PathIndex] : -1.f;
		WaitLeft = W > 0.f ? W : FMath::FRandRange(4.f, 10.f);
		PathIndex = (PathIndex + 1) % Path.Num();
		return;
	}
	const FVector Dir = To.GetSafeNormal2D();
	AddMovementInput(Dir, 1.f);
	SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), FRotator(0.f, Dir.Rotation().Yaw, 0.f), DeltaSeconds, 180.f));
}

#include "NWLost.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NWBrainComponent.h"
#include "NWCombatComponent.h"
#include "NWTraversalComponent.h"

ANWLost::ANWLost(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MeshAsset = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/NW/lost_sword/Mesh/SK_lost.SK_lost")));
	AnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/NW/lost_sword/ABP_Lost.ABP_Lost_C")));
	SwordAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NW/lost_sword/Mesh/SM_sword.SM_sword")));
	ShieldAsset.Reset();
	ActionTableAsset = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/NW/Data/DT_LostActions.DT_LostActions")));
	Shield->SetVisibility(false);
	// Same correction as the player (bone rotated by the Blender importer).
	Sword->SetRelativeRotation(FRotator(0.f, 0.f, -90.f));

	// The Lost walks (walk blend space, ~1.6 m/s) and runs when chasing.
	RunSpeed = 125.f; // fastest walk speed of the game's blend space (1.25 m/s)
	bDummyAttacks = true;
	AggroRange = 1500.f;
	DummyAttackRange = 220.f;
	DummyAttackInterval = 2.2f;
	DummyAttackActions = {TEXT("Attack1"), TEXT("Attack2"), TEXT("Heavy"), TEXT("Leap")};

	Combat->MaxHealth = 600.f;
	// damned_grunt: stagger resistance 20, knockback received ×1.2, mitigation 0.35 (docs/game/combat.md).
	Combat->StaggerResistRating = 20.f;
	Combat->ImpactDistanceScale = 1.2f;
	Combat->ArmorMitigation = 0.35f;
	Combat->WeaponBaseDamage = 60.f;
	Combat->LightComboStart = TEXT("Attack1");
	// Another skeleton: without the player's jump/traversal montages.
	Traversal->bEnabled = false;
	WeaponSets.Reset();
	Stowed->SetVisibility(false);
}

void ANWLost::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	// Duelist: circles, comes in with one or two hits and backs off; leaps from mid range. No guard or dodge
	// (the Lost set does not have those montages).
	auto Atk = [](const TCHAR* Action, float Min, float Max, float Weight, float Cooldown = 0.f, bool bGap = false, int32 Chain = 1)
	{
		FNWBrainAttack A;
		A.Action = Action; A.MinRange = Min; A.MaxRange = Max; A.Weight = Weight; A.Cooldown = Cooldown; A.bGapCloser = bGap; A.Chain = Chain;
		return A;
	};
	UseBrain(static_cast<uint8>(ENWBrainStyle::Duelist), {Atk(TEXT("Attack1"), 0.f, 240.f, 2.f), Atk(TEXT("Attack2"), 0.f, 240.f, 1.f),
		Atk(TEXT("Heavy"), 0.f, 240.f, 1.f, 3.f), Atk(TEXT("Leap"), 320.f, 700.f, 1.f, 6.f, true)});
	Brain->HoldDistance = 330.f;
}

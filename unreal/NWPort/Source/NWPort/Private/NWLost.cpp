#include "NWLost.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
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
	// Mesma correção do jogador (osso girado pelo importador do Blender).
	Sword->SetRelativeRotation(FRotator(0.f, 0.f, -90.f));

	// O Lost anda (blend space de caminhada, ~1.6 m/s) e corre quando persegue.
	RunSpeed = 125.f; // velocidade da caminhada mais rápida do blend space do jogo (1,25 m/s)
	bDummyAttacks = true;
	AggroRange = 1500.f;
	DummyAttackRange = 220.f;
	DummyAttackInterval = 2.2f;
	DummyAttackActions = {TEXT("Attack1"), TEXT("Attack2"), TEXT("Heavy"), TEXT("Leap")};

	Combat->MaxHealth = 600.f;
	Combat->WeaponBaseDamage = 60.f;
	Combat->LightComboStart = TEXT("Attack1");
	// Outro esqueleto: sem as montagens de pulo/travessia do jogador.
	Traversal->bEnabled = false;
	WeaponSets.Reset();
	Stowed->SetVisibility(false);
}

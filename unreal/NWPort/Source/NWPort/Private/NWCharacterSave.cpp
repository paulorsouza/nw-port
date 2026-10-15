// Part of ANWCharacter for the systems area (docs/workflow.md): save and restore the character (nw-core).
#include "NWCharacter.h"
#include "Misc/App.h"
#include "NWRules.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/DataTable.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/GameViewportClient.h"
#include "Camera/PlayerCameraManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Net/UnrealNetwork.h"
#include "Dom/JsonObject.h"
#include "NWBrainComponent.h"
#include "HAL/IConsoleManager.h"
#include "NWCombatComponent.h"
#include "NWTraversalComponent.h"
#include "NWGatherComponent.h"
#include "NWInteractable.h"
#include "NWProgressionComponent.h"
#include "NWInventoryComponent.h"
#include "NWOptions.h"
#include "NWPlayerController.h"
#include "Misc/ConfigCacheIni.h"
#include "NWAnimInstance.h"
#include "Animation/BlendSpace.h"
#include "AnimationRuntime.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "NWDevRegistry.h"
#include "NWGameMode.h"
#include "GameFramework/PlayerState.h"
#include "NWBackend.h"

void ANWCharacter::RestoreFrom(const FNWCharacterRecord& Record)
{
	for (int32 i = 0; i < WeaponSets.Num(); ++i)
	{
		FNWWeaponSet& Set = WeaponSets[i];
		const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
		if (Record.Loadout.IsValid() && Record.Loadout->TryGetArrayField(Set.Id.ToString(), Names) && Names->Num() == 3)
		{
			Set.Abilities.Reset();
			for (const TSharedPtr<FJsonValue>& N : *Names)
			{
				Set.Abilities.Add(FName(*N->AsString()));
			}
		}
		if (i != ActiveWeapon && Set.Id.ToString().Equals(Record.ActiveWeapon, ESearchCase::IgnoreCase))
		{
			ActiveWeapon = i;
			ApplyWeapon();
		}
	}
	if (WeaponSets.IsValidIndex(ActiveWeapon))
	{
		Abilities = WeaponSets[ActiveWeapon].Abilities;
	}
	if (Record.Health > 0.f)
	{
		Combat->Health = FMath::Min(Record.Health, Combat->MaxHealth);
	}
	Inventory->Load(Record.Items, Record.Coins);
	Progression->Load(Record.Progression);
	if (Record.bFirstJoin)
	{
		Inventory->GiveStarterKit();
	}
	else
	{
		// Characters from before F4: gathering tools that are missing.
		Inventory->GiveTools();
	}
}

void ANWCharacter::CaptureTo(FNWCharacterRecord& Record) const
{
	// Only a position on the ground: while falling (or fallen off the map) the last good one stays.
	if (GetCharacterMovement()->IsMovingOnGround())
	{
		Record.bHasPosition = true;
		Record.Position = GetActorLocation();
		Record.Yaw = GetActorRotation().Yaw;
	}
	Record.Health = Combat->State == ENWCombatState::Dead ? -1.f : Combat->Health;
	if (WeaponSets.IsValidIndex(ActiveWeapon))
	{
		Record.ActiveWeapon = WeaponSets[ActiveWeapon].Id.ToString();
	}
	TSharedRef<FJsonObject> AbilityLoadout = MakeShared<FJsonObject>();
	for (const FNWWeaponSet& Set : WeaponSets)
	{
		TArray<TSharedPtr<FJsonValue>> Names;
		for (const FName& N : Set.Abilities)
		{
			Names.Add(MakeShared<FJsonValueString>(N.ToString()));
		}
		AbilityLoadout->SetArrayField(Set.Id.ToString(), Names);
	}
	Record.Loadout = AbilityLoadout;
	Record.Items = Inventory->Items;
	Record.Coins = Inventory->Coins;
	Record.Progression = Progression->Save();
	Record.StorageCapacity = Interactor ? Interactor->StorageCapacityNow() : -1.f;
	Record.bFirstJoin = false;
}

// F10: switch character (editor Play: the account's next one in place, creating a second one if needed; client with
// login: back to the character list).
NW_REGISTER_DEV(SwitchCharacter, TEXT("sistemas.switch_character"), TEXT("nw.dev.switch_character"),
	[](ANWPlayerController* PC)
	{
		if (ANWGameMode* GM = PC && PC->GetWorld() ? PC->GetWorld()->GetAuthGameMode<ANWGameMode>() : nullptr)
		{
			GM->SwitchCharacter(PC);
		}
	},
	[](const ANWPlayerController* PC)
	{
		const APlayerState* PS = PC ? PC->PlayerState.Get() : nullptr;
		return FText::FromString(PS ? PS->GetPlayerName() : FString());
	});


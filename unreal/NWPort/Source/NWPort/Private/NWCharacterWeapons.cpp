// Part of ANWCharacter for the weapons area (docs/workflow.md): player weapons, swap and loadout, attack, block,
// dodge, Q/R/F abilities, AI brain and training dummy. The rest of the character is in NWCharacter.cpp.
#include "NWCharacter.h"
#include "NWAppearance.h"
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
#include "NWIceGauntlet.h"
#include "NWTraversalComponent.h"
#include "NWGatherComponent.h"
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

void ANWCharacter::AddPlayerWeaponSets()
{
	// Player weapons: sword and shield (1) and Hatchet (2). Paths created by the sword_shield set import.
	FNWWeaponSet SwordSet;
	SwordSet.Id = TEXT("Sword");
	SwordSet.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Mesh/SM_sword.SM_sword")));
	SwordSet.bUsesShield = true;
	SwordSet.Actions = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/NW/Data/DT_NWActions.DT_NWActions")));
	SwordSet.Locomotion = TSoftObjectPtr<UBlendSpace>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Anims/BS_Strafe.BS_Strafe")));
	SwordSet.HudIcon = TEXT("lyshineui/images/icons/items/drawing/1hsword.dds");
	// SwordT2: damage 64 (= 100 in the port), crit 7% ×1.3, stagger 36; block with the round shield (stability 30).
	SwordSet.Abilities = {TEXT("ShieldBash"), TEXT("SpinAttack"), TEXT("SwipeStab")};
	WeaponSets.Add(SwordSet);
	FNWWeaponSet Hatchet;
	Hatchet.Id = TEXT("Hatchet");
	Hatchet.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Mesh/SM_hatchet.SM_hatchet")));
	Hatchet.Actions = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/NW/Data/DT_HatchetActions.DT_HatchetActions")));
	Hatchet.Locomotion = TSoftObjectPtr<UBlendSpace>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Anims/BS_Hatchet_Strafe.BS_Hatchet_Strafe")));
	Hatchet.HudIcon = TEXT("lyshineui/images/icons/items/drawing/1hhatchet.dds");
	// HatchetT2: damage 67 (×100/64), crit 8% ×1.4, stagger 30, stability 21.
	Hatchet.BaseDamage = 105.f; Hatchet.CritChance = 0.08f; Hatchet.CritDamageMultiplier = 1.4f;
	Hatchet.StaggerDamage = 30.f; Hatchet.BlockStaminaDamage = 30.f; Hatchet.BlockStability = 21.f;
	Hatchet.Abilities = {TEXT("Berserk"), TEXT("RagingTorrent"), TEXT("RendingThrow")};
	WeaponSets.Add(Hatchet);
	// Bow (F10 > Swap weapon pair): BowT2 84 (×100/64), crit 8% ×1.3, stagger 38; does not block.
	FNWWeaponSet Bow;
	Bow.Id = TEXT("Bow");
	Bow.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Mesh/SM_bow.SM_bow")));
	Bow.Actions = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/NW/Data/DT_BowActions.DT_BowActions")));
	Bow.Locomotion = TSoftObjectPtr<UBlendSpace>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Anims/BS_Bw_Strafe.BS_Bw_Strafe")));
	Bow.HudIcon = TEXT("lyshineui/images/icons/items/drawing/bowa.dds");
	Bow.Abilities = {TEXT("RapidShot"), TEXT("EvadeShot"), TEXT("PenetratingShot")};
	Bow.Socket = TEXT("left_hand_attach");
	Bow.BaseDamage = 131.f; Bow.CritChance = 0.08f; Bow.CritDamageMultiplier = 1.3f;
	Bow.StaggerDamage = 38.f; Bow.BlockStaminaDamage = 38.f; Bow.BlockStability = 0.f;
	Bow.bAims = true;
	WeaponSets.Add(Bow);
	// Rapier: RapierT2 61 (×100/64), crit 9% ×1.4, stagger 28, stability 17. Charge without an intermediate heavy:
	// release before 0.667 s = light, reaching 0.667 s = charged.
	FNWWeaponSet Rapier;
	Rapier.Id = TEXT("Rapier");
	Rapier.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Mesh/SM_rapier.SM_rapier")));
	Rapier.Actions = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/NW/Data/DT_RapierActions.DT_RapierActions")));
	Rapier.Locomotion = TSoftObjectPtr<UBlendSpace>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Anims/BS_Hatchet_Strafe.BS_Hatchet_Strafe")));
	Rapier.HudIcon = TEXT("lyshineui/images/icons/items/drawing/1hrapier.dds");
	Rapier.Abilities = {TEXT("Tondo"), TEXT("Riposte"), TEXT("Evade")};
	Rapier.BaseDamage = 95.f; Rapier.CritChance = 0.09f; Rapier.CritDamageMultiplier = 1.4f;
	Rapier.StaggerDamage = 28.f; Rapier.BlockStaminaDamage = 28.f; Rapier.BlockStability = 17.f;
	Rapier.HeavyMinHold = 0.667f; Rapier.HeavyFullCharge = 0.667f;
	WeaponSets.Add(Rapier);
	// Ice Gauntlet (sub-area 1a).
	NWIceGauntlet::AddWeaponSet(WeaponSets);
}

void ANWCharacter::UseBrain(uint8 Style, const TArray<FNWBrainAttack>& Attacks)
{
	Brain->ApplyStyle(static_cast<ENWBrainStyle>(Style));
	Brain->Attacks = Attacks;
	Brain->Activate(true);
}

void ANWCharacter::AttackPressed()
{
	// Menu click: neither the press nor the release becomes a hit.
	bAttackPressAccepted = !InMenu() && !IsUnarmed();
	if (!bAttackPressAccepted)
	{
		return;
	}
	if (!CanAct())
	{
		return;
	}
	// Running: sprint attack on click (Attack_Sprint); idle or walking: the heavy charge starts. A weapon without a
	// sprint attack (Ice Gauntlet) follows the normal click.
	if (bSprinting && GetVelocity().Size2D() > RunSpeed && Combat->ActionTable
		&& Combat->ActionTable->FindRow<FNWActionRow>(TEXT("SprintAttack"), TEXT("sprint"), false))
	{
		Combat->RequestAction(TEXT("SprintAttack"));
		bAttackFromSprint = true;
		return;
	}
	bAttackFromSprint = false;
	Combat->AttackPressed();
}

void ANWCharacter::AttackReleased()
{
	if (!bAttackPressAccepted)
	{
		return;   // the press was a menu click: the release does not become a light hit on the server
	}
	bAttackPressAccepted = false;
	if (bAttackFromSprint)
	{
		bAttackFromSprint = false;
		return;
	}
	Combat->AttackReleased();
}

void ANWCharacter::BlockPressed()
{
	if (InMenu() || IsUnarmed())
	{
		return;
	}
	if (Combat->bAimWeapon)
	{
		Combat->RequestAim(true);   // Bow: the right button aims
		return;
	}
	if (CanAct())
	{
		Combat->RequestBlock(true);
	}
}

void ANWCharacter::BlockReleased()
{
	if (Combat->bAimWeapon || Combat->bAiming)
	{
		Combat->RequestAim(false);
		return;
	}
	Combat->RequestBlock(false);
}

void ANWCharacter::Dodge()
{
	if (Inventory && Inventory->IsEncumbered())
	{
		return;
	}
	if (InMenu())
	{
		return;
	}
	// Input in camera space -> character space.
	if (!Controller || !CanAct())
	{
		return;
	}
	const FRotator CamYaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector World = FRotationMatrix(CamYaw).GetUnitAxis(EAxis::X) * LastMoveInput.X + FRotationMatrix(CamYaw).GetUnitAxis(EAxis::Y) * LastMoveInput.Y;
	const FVector Local = GetActorTransform().InverseTransformVectorNoScale(World);
	Combat->RequestDodge(FVector2D(Local.X, Local.Y));
}

void ANWCharacter::SetAbilities(const TArray<FName>& New)
{
	SetWeaponAbilities(ActiveWeapon, New);
}

void ANWCharacter::SetWeaponAbilities(int32 SetIndex, const TArray<FName>& New)
{
	if (!WeaponSets.IsValidIndex(SetIndex))
	{
		Abilities = New;
		return;
	}
	WeaponSets[SetIndex].Abilities = New;
	if (SetIndex == ActiveWeapon)
	{
		Abilities = New;
	}
	if (IsLocallyControlled())
	{
		TArray<FString> Names;
		for (const FName& N : New)
		{
			Names.Add(N.ToString());
		}
		GConfig->SetArray(NWIni::Loadout, *WeaponSets[SetIndex].Id.ToString(), Names, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		if (!HasAuthority())
		{
			ServerSetWeaponAbilities(SetIndex, New);
		}
	}
}

void ANWCharacter::ServerSetWeaponAbilities_Implementation(int32 SetIndex, const TArray<FName>& New)
{
	if (!WeaponSets.IsValidIndex(SetIndex) || New.Num() > 3)
	{
		return;
	}
	WeaponSets[SetIndex].Abilities = New;
	if (SetIndex == ActiveWeapon)
	{
		Abilities = New;
	}
}

void ANWCharacter::LoadAbilityLoadout()
{
	for (FNWWeaponSet& Set : WeaponSets)
	{
		TArray<FString> Names;
		if (GConfig->GetArray(NWIni::Loadout, *Set.Id.ToString(), Names, GGameUserSettingsIni) == 3)
		{
			Set.Abilities.Reset();
			for (const FString& N : Names)
			{
				Set.Abilities.Add(FName(*N));
			}
		}
	}
	if (WeaponSets.IsValidIndex(ActiveWeapon))
	{
		Abilities = WeaponSets[ActiveWeapon].Abilities;
	}
}

// ---------------------------------------------------------------- weapon swap

// Position of a stowed piece, given in mesh space (the mesh faces +Y; +X is the left side),
// converted to relative to the bone in the reference pose.
static FTransform StowRelative(const USkeletalMeshComponent* Body, FName Bone, const FTransform& InMesh)
{
	const USkeletalMesh* Mesh = Body ? Body->GetSkeletalMeshAsset() : nullptr;
	if (!Mesh)
	{
		return FTransform::Identity;
	}
	const int32 Index = Mesh->GetRefSkeleton().FindBoneIndex(Bone);
	if (Index == INDEX_NONE)
	{
		return FTransform::Identity;
	}
	const FTransform BoneRef = FAnimationRuntime::GetComponentSpaceTransformRefPose(Mesh->GetRefSkeleton(), Index);
	return InMesh.GetRelativeTransform(BoneRef);
}

void ANWCharacter::ApplyWeapon()
{
	if (!WeaponSets.IsValidIndex(ActiveWeapon))
	{
		if (WeaponSets.Num() > 0)
		{
			// Unarmed (nothing in Weapon 1/2): no weapon in hand or on the body; the first set's table stays for
			// reactions, dodge and death, but attack, block and abilities do not start (IsUnarmed).
			if (UDataTable* Table = WeaponSets[0].Actions.LoadSynchronous())
			{
				Combat->ActionTable = Table;
			}
			if (HasAuthority())
			{
				Combat->bAimWeapon = false;
				Combat->bAiming = false;
			}
			Sword->SetStaticMesh(nullptr);
			Combat->SetMainWeapon(nullptr);
			NWIceGauntlet::ApplySkin(this, FNWWeaponSet());
			Shield->SetVisibility(false);
			Stowed->SetVisibility(false);
			Abilities.Reset();
			RefreshLocomotion();
		}
		return;
	}
	const FNWWeaponSet& Set = WeaponSets[ActiveWeapon];
	USkeletalMeshComponent* Body = GetMesh();
	if (UDataTable* Table = Set.Actions.LoadSynchronous())
	{
		Combat->ActionTable = Table;
	}
	Combat->WeaponBaseDamage = Set.BaseDamage;
	Combat->CritChance = Set.CritChance;
	Combat->CritDamageMultiplier = Set.CritDamageMultiplier;
	Combat->BaseStaggerDamage = Set.StaggerDamage;
	Combat->BlockStaminaDamage = Set.BlockStaminaDamage;
	Combat->BlockStability = Set.BlockStability;
	Combat->HeavyMinHold = Set.HeavyMinHold;
	Combat->HeavyFullCharge = Set.HeavyFullCharge;
	if (HasAuthority())
	{
		Combat->bAimWeapon = Set.bAims;
		if (!Set.bAims)
		{
			Combat->bAiming = false;
		}
	}
	Sword->SetStaticMesh(Set.Mesh.LoadSynchronous());
	Sword->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Set.Socket);
	Sword->SetRelativeRotation(FRotator(0.f, 0.f, -90.f));
	Combat->SetMainWeapon(Sword);
	NWIceGauntlet::ApplySkin(this, Set);

	// Shield: in hand with the sword; on the back with the Hatchet.
	if (Set.bUsesShield)
	{
		Shield->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("left_hand_attach"));
		Shield->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator(0.f, 0.f, -90.f));
	}
	else
	{
		// Behind (mesh -Y), face out. The combat stance tilts the torso ~29° forward
		// (measured: the shield tipped 29°); the shield front already comes tilted up to compensate.
		const float Lean = FMath::DegreesToRadians(29.f);
		const FVector Out(0.f, -FMath::Cos(Lean), FMath::Sin(Lean));
		const FVector Up(0.f, FMath::Sin(Lean), FMath::Cos(Lean));
		const FTransform Back(FRotationMatrix::MakeFromXZ(Out, Up).Rotator(), FVector(0.f, -26.f, 132.f));
		Shield->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Spine2"));
		Shield->SetRelativeTransform(StowRelative(Body, TEXT("Spine2"), Back));
	}

	// The shield only exists with sword and shield in the weapon pair (in the game it is part of that weapon); without it, it disappears.
	bool bShieldInPair = Set.bUsesShield;
	for (const int32 I : Loadout)
	{
		bShieldInPair |= WeaponSets.IsValidIndex(I) && WeaponSets[I].bUsesShield;
	}
	Shield->SetVisibility(bShieldInPair && (WeaponSets.Num() > 0));

	// The other weapon: Bow on the back (the game's "bow" attachment, on the spine); the others on the left hip, handle
	// up and blade down and back.
	const int32 Other = OtherWeapon();
	if (Other != INDEX_NONE && WeaponSets[Other].bAims)
	{
		Stowed->SetStaticMesh(WeaponSets[Other].Mesh.LoadSynchronous());
		const float Lean = FMath::DegreesToRadians(29.f);
		const FVector Out(0.f, -FMath::Cos(Lean), FMath::Sin(Lean));
		const FVector Up(0.f, FMath::Sin(Lean), FMath::Cos(Lean));
		// Bow upright on the back, tilted ~35° (top tip toward the right shoulder).
		const FVector Along = (Up * FMath::Cos(FMath::DegreesToRadians(35.f)) + FVector(1.f, 0.f, 0.f) * FMath::Sin(FMath::DegreesToRadians(35.f))).GetSafeNormal();
		const FTransform Back(FRotationMatrix::MakeFromZX(Along, Out).Rotator(), FVector(0.f, -20.f, 128.f));
		Stowed->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Spine2"));
		Stowed->SetRelativeTransform(StowRelative(Body, TEXT("Spine2"), Back));
		Stowed->SetVisibility(true);
	}
	else if (Other != INDEX_NONE)
	{
		Stowed->SetStaticMesh(WeaponSets[Other].Mesh.LoadSynchronous());
		const FVector Blade = FVector(0.f, -0.45f, -1.f).GetSafeNormal();
		const FRotator Rot = FRotationMatrix::MakeFromZX(Blade, FVector(1.f, 0.f, 0.f)).Rotator();
		const FTransform Hip(Rot, FVector(17.f, -2.f, 102.f));
		Stowed->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hips"));
		Stowed->SetRelativeTransform(StowRelative(Body, TEXT("Hips"), Hip));
		Stowed->SetVisibility(true);
	}
	else
	{
		Stowed->SetVisibility(false);
	}

	RefreshLocomotion();
	UE_LOG(LogTemp, Log, TEXT("NW weapon %s: shield on %s rel %s | world %s | mesh %s"), *Set.Id.ToString(),
		*Shield->GetAttachSocketName().ToString(), *Shield->GetRelativeTransform().ToString(),
		*Shield->GetComponentRotation().ToString(), *Body->GetComponentRotation().ToString());
	Abilities = Set.Abilities;
	// Equipped items' meshes (players); the blade was measured above with the class mesh, so the reach does not change.
	if (UNWAppearanceComponent* Look = FindComponentByClass<UNWAppearanceComponent>())
	{
		Look->ApplyWeaponLook();
	}
}

void ANWCharacter::SwitchWeapon(int32 Index)
{
	if (InMenu())
	{
		return;
	}
	if (!WeaponSets.IsValidIndex(Index) || Index == ActiveWeapon || !CanAct() || Combat->IsBusy())
	{
		return;
	}
	ServerSwitchWeapon(Index);
}

void ANWCharacter::ServerSwitchWeapon_Implementation(int32 Index)
{
	if (!WeaponSets.IsValidIndex(Index) || Index == ActiveWeapon || Combat->IsBusy())
	{
		return;
	}
	// Only to a weapon of the pair (for players, the weapons equipped in Weapon 1/2).
	if (!Loadout.Contains(Index))
	{
		return;
	}
	if (Combat->State == ENWCombatState::Blocking)
	{
		Combat->RequestBlock(false);
	}
	ActiveWeapon = Index;
	ApplyWeapon();
	Combat->PlayDraw();
}

int32 ANWCharacter::OtherWeapon() const
{
	if (Loadout.Num() == 2)
	{
		const int32 Other = Loadout[0] == ActiveWeapon ? Loadout[1] : Loadout[0];
		return WeaponSets.IsValidIndex(Other) && Other != ActiveWeapon ? Other : INDEX_NONE;
	}
	return WeaponSets.Num() > 1 ? (ActiveWeapon + 1) % WeaponSets.Num() : INDEX_NONE;
}

void ANWCharacter::SetLoadout(int32 First, int32 Second)
{
	if (!WeaponSets.IsValidIndex(First) || !WeaponSets.IsValidIndex(Second) || Combat->IsBusy())
	{
		return;
	}
	if (Combat->State == ENWCombatState::Blocking)
	{
		Combat->RequestBlock(false);
	}
	Loadout = {First, Second};
	ActiveWeapon = First;
	Abilities = WeaponSets[First].Abilities;
	ApplyWeapon();
	Combat->PlayDraw();
}

void ANWCharacter::SyncWeaponsFromInventory()
{
	// Players only (the arena AI and creatures keep the pair they were given).
	if (!HasAuthority() || !Inventory || WeaponSets.Num() == 0 || !Cast<APlayerController>(GetController()))
	{
		return;
	}
	TArray<int32> New;
	for (const int32 Slot : {NWEquip::Weapon1, NWEquip::Weapon2})
	{
		const FName Set = Inventory->WeaponSetAt(Slot);
		New.Add(Set.IsNone() ? INDEX_NONE : WeaponSets.IndexOfByPredicate([Set](const FNWWeaponSet& W) { return W.Id == Set; }));
	}
	int32 Active = ActiveWeapon;
	if (!New.Contains(Active) || Active == INDEX_NONE)
	{
		Active = WeaponSets.IsValidIndex(New[0]) ? New[0] : New[1];
	}
	if (New == Loadout && Active == ActiveWeapon)
	{
		return;
	}
	// Mid action (attack, dodge, reaction): tries again shortly.
	if (Combat->IsBusy())
	{
		GetWorldTimerManager().SetTimer(SyncWeaponsTimer, this, &ANWCharacter::SyncWeaponsFromInventory, 0.25f, false);
		return;
	}
	if (Combat->State == ENWCombatState::Blocking)
	{
		Combat->RequestBlock(false);
	}
	const bool bDraw = Active != ActiveWeapon && WeaponSets.IsValidIndex(Active);
	Loadout = New;
	ActiveWeapon = Active;
	ApplyWeapon();
	if (bDraw)
	{
		Combat->PlayDraw();
	}
}

void ANWCharacter::OnRep_ActiveWeapon()
{
	ApplyWeapon();
}

void ANWCharacter::Ability(int32 Index)
{
	if (InMenu() || IsUnarmed())
	{
		return;
	}
	if (!Abilities.IsValidIndex(Index) || !CanAct())
	{
		return;
	}
	// Evade (Rapier): short step in the movement direction (Evade_F/B/L/R), single cooldown "Evade".
	if (Abilities[Index] == TEXT("Evade") && Controller)
	{
		const FRotator CamYaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector World = FRotationMatrix(CamYaw).GetUnitAxis(EAxis::X) * LastMoveInput.X + FRotationMatrix(CamYaw).GetUnitAxis(EAxis::Y) * LastMoveInput.Y;
		const FVector Local = GetActorTransform().InverseTransformVectorNoScale(World);
		Combat->RequestDirectional(TEXT("Evade_"), FVector2D(Local.X, Local.Y));
		return;
	}
	Combat->RequestAction(Abilities[Index]);
}

void ANWCharacter::TickDummy(float DeltaSeconds)
{
	if (Combat->State == ENWCombatState::Dead)
	{
		DummyHitsLeft = 0;
		return;
	}
	// Target: nearest player within chase range (or attack range, if it does not chase).
	const ANWCharacter* Target = nullptr;
	float Best = FMath::Max(AggroRange, DummyAttackRange);
	for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->IsPlayerControlled() && It->Combat->State != ENWCombatState::Dead)
		{
			const float D = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
			if (D < Best)
			{
				Best = D;
				Target = *It;
			}
		}
	}
	if (!Target)
	{
		DummyHitsLeft = 0;
		DummyTimer = 0.f;
		return;
	}
	const FVector ToTarget = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!Combat->IsBusy())
	{
		SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), FRotator(0.f, ToTarget.Rotation().Yaw, 0.f), DeltaSeconds, 360.f));
		if (Controller)
		{
			Controller->SetControlRotation(GetActorRotation());
		}
		// Chases until within hit range.
		if (AggroRange > 0.f && Best > DummyAttackRange * 0.8f)
		{
			AddMovementInput(ToTarget, 1.f);
		}
	}
	if (Best > DummyAttackRange)
	{
		return;
	}
	DummyTimer += DeltaSeconds;
	if (DummyAttackActions.Num() > 0)
	{
		// Enemy: one random attack from the list every interval.
		if (DummyTimer >= DummyAttackInterval && !Combat->IsBusy())
		{
			Combat->RequestAction(DummyAttackActions[FMath::RandRange(0, DummyAttackActions.Num() - 1)]);
			DummyTimer = FMath::FRandRange(-0.5f, 0.5f);
		}
		return;
	}
	if (DummyHitsLeft == 0 && DummyTimer >= DummyAttackInterval)
	{
		DummyHitsLeft = DummyComboHits;
		DummyTimer = 0.f;
	}
	// One request at a time; the component's buffer fits the next hit into the combo window.
	if (DummyHitsLeft > 0 && DummyTimer > 0.25f)
	{
		Combat->RequestLightAttack();
		--DummyHitsLeft;
		DummyTimer = 0.f;
	}
}

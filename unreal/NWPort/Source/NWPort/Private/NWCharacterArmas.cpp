// Parte do ANWCharacter da esteira de armas (docs/esteiras.md): armas do jogador, troca e loadout, ataque, bloqueio,
// esquiva, habilidades de Q/R/F, cérebro de IA e boneco de treino. O resto do personagem fica em NWCharacter.cpp.
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
	// Armas do jogador: espada e escudo (1) e machadinha (2). Caminhos criados pelo import do conjunto sword_shield.
	FNWWeaponSet SwordSet;
	SwordSet.Id = TEXT("Sword");
	SwordSet.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Mesh/SM_sword.SM_sword")));
	SwordSet.bUsesShield = true;
	SwordSet.Actions = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/NW/Data/DT_NWActions.DT_NWActions")));
	SwordSet.Locomotion = TSoftObjectPtr<UBlendSpace>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Anims/BS_Strafe.BS_Strafe")));
	SwordSet.HudIcon = TEXT("lyshineui/images/icons/items/drawing/1hsword.dds");
	// SwordT2: dano 64 (= 100 do port), crítico 7% ×1,3, stagger 36; bloqueio com o escudo redondo (estabilidade 30).
	SwordSet.Abilities = {TEXT("ShieldBash"), TEXT("SpinAttack"), TEXT("SwipeStab")};
	WeaponSets.Add(SwordSet);
	FNWWeaponSet Hatchet;
	Hatchet.Id = TEXT("Hatchet");
	Hatchet.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Mesh/SM_hatchet.SM_hatchet")));
	Hatchet.Actions = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/NW/Data/DT_HatchetActions.DT_HatchetActions")));
	Hatchet.Locomotion = TSoftObjectPtr<UBlendSpace>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Anims/BS_Hatchet_Strafe.BS_Hatchet_Strafe")));
	Hatchet.HudIcon = TEXT("lyshineui/images/icons/items/drawing/1hhatchet.dds");
	// HatchetT2: dano 67 (×100/64), crítico 8% ×1,4, stagger 30, estabilidade 21.
	Hatchet.BaseDamage = 105.f; Hatchet.CritChance = 0.08f; Hatchet.CritDamageMultiplier = 1.4f;
	Hatchet.StaggerDamage = 30.f; Hatchet.BlockStaminaDamage = 30.f; Hatchet.BlockStability = 21.f;
	Hatchet.Abilities = {TEXT("Berserk"), TEXT("RagingTorrent"), TEXT("RendingThrow")};
	WeaponSets.Add(Hatchet);
	// Arco (F10 > Trocar par de armas): BowT2 84 (×100/64), crítico 8% ×1,3, stagger 38; não bloqueia.
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
	// Rapieira: RapierT2 61 (×100/64), crítico 9% ×1,4, stagger 28, estabilidade 17. Carga sem pesado intermediário:
	// soltar antes de 0,667 s = leve, chegar a 0,667 s = carregado.
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
	
}

void ANWCharacter::UseBrain(uint8 Style, const TArray<FNWBrainAttack>& Attacks)
{
	Brain->ApplyStyle(static_cast<ENWBrainStyle>(Style));
	Brain->Attacks = Attacks;
	Brain->Activate(true);
}

void ANWCharacter::AttackPressed()
{
	// Clique de menu: nem o aperto nem a soltura viram golpe.
	bAttackPressAccepted = !InMenu();
	if (!bAttackPressAccepted)
	{
		return;
	}
	if (!CanAct())
	{
		return;
	}
	// Correndo: golpe de corrida no clique (Attack_Sprint); parado ou andando: começa a carga do pesado.
	if (bSprinting && GetVelocity().Size2D() > RunSpeed)
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
		return;   // aperto foi clique de menu: a soltura não vira golpe leve no servidor
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
	if (InMenu())
	{
		return;
	}
	if (Combat->bAimWeapon)
	{
		Combat->RequestAim(true);   // arco: o botão direito mira
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
	// Input em espaço da câmera -> espaço do personagem.
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

// ---------------------------------------------------------------- troca de arma

// Posição de uma peça guardada, dada no espaço da malha (a malha olha para +Y; +X é o lado esquerdo),
// convertida para relativa ao osso na pose de referência.
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

	// Escudo: na mão com a espada; nas costas com a machadinha.
	if (Set.bUsesShield)
	{
		Shield->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("left_hand_attach"));
		Shield->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator(0.f, 0.f, -90.f));
	}
	else
	{
		// Atrás (-Y da malha), face para fora. A postura de combate inclina o tronco ~29° para a frente
		// (medido: o escudo tombava 29°); a frente do escudo já sai inclinada para cima para compensar.
		const float Lean = FMath::DegreesToRadians(29.f);
		const FVector Out(0.f, -FMath::Cos(Lean), FMath::Sin(Lean));
		const FVector Up(0.f, FMath::Sin(Lean), FMath::Cos(Lean));
		const FTransform Back(FRotationMatrix::MakeFromXZ(Out, Up).Rotator(), FVector(0.f, -26.f, 132.f));
		Shield->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Spine2"));
		Shield->SetRelativeTransform(StowRelative(Body, TEXT("Spine2"), Back));
	}

	// Escudo só existe com espada e escudo no par de armas (no jogo é parte dessa arma); sem ela, some.
	bool bShieldInPair = Set.bUsesShield;
	for (const int32 I : Loadout)
	{
		bShieldInPair |= WeaponSets.IsValidIndex(I) && WeaponSets[I].bUsesShield;
	}
	Shield->SetVisibility(bShieldInPair && (WeaponSets.Num() > 0));

	// A outra arma: arco nas costas (attachment "bow" do jogo, na coluna); as outras no quadril esquerdo, cabo para
	// cima e lâmina para baixo e para trás.
	const int32 Other = OtherWeapon();
	if (Other != INDEX_NONE && WeaponSets[Other].bAims)
	{
		Stowed->SetStaticMesh(WeaponSets[Other].Mesh.LoadSynchronous());
		const float Lean = FMath::DegreesToRadians(29.f);
		const FVector Out(0.f, -FMath::Cos(Lean), FMath::Sin(Lean));
		const FVector Up(0.f, FMath::Sin(Lean), FMath::Cos(Lean));
		// Arco de pé nas costas, inclinado ~35° (ponta de cima para o ombro direito).
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
	UE_LOG(LogTemp, Log, TEXT("NW arma %s: escudo em %s rel %s | mundo %s | malha %s"), *Set.Id.ToString(),
		*Shield->GetAttachSocketName().ToString(), *Shield->GetRelativeTransform().ToString(),
		*Shield->GetComponentRotation().ToString(), *Body->GetComponentRotation().ToString());
	Abilities = Set.Abilities;
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
	// Jogador com inventário: só troca para uma arma equipada.
	if (Inventory && Inventory->Items.Num() > 0)
	{
		// Arco e rapieira ainda sem espaço próprio no equipamento: liberados (arena de testes).
		const FName Id = WeaponSets[Index].Id;
		const int32 Slot = Id == TEXT("Hatchet") ? NWEquip::Hatchet : NWEquip::Sword;
		if ((Id == TEXT("Sword") || Id == TEXT("Hatchet")) && !Inventory->At(ENWItemContainer::Equip, Slot))
		{
			return;
		}
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
	if (Loadout.Num() == 2 && WeaponSets.IsValidIndex(Loadout[0]) && WeaponSets.IsValidIndex(Loadout[1]))
	{
		return Loadout[0] == ActiveWeapon ? Loadout[1] : Loadout[0];
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

void ANWCharacter::OnRep_ActiveWeapon()
{
	ApplyWeapon();
}

void ANWCharacter::Ability(int32 Index)
{
	if (InMenu())
	{
		return;
	}
	if (!Abilities.IsValidIndex(Index) || !CanAct())
	{
		return;
	}
	// Evadir (rapieira): passo curto na direção do movimento (Evade_F/B/L/R), recarga única "Evade".
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
	// Alvo: jogador mais próximo dentro do alcance de perseguição (ou de ataque, se não persegue).
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
		// Persegue até ficar no alcance do golpe.
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
		// Inimigo: um ataque ao acaso da lista a cada intervalo.
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
	// Um pedido por vez; o buffer do componente encaixa o próximo golpe na janela do combo.
	if (DummyHitsLeft > 0 && DummyTimer > 0.25f)
	{
		Combat->RequestLightAttack();
		--DummyHitsLeft;
		DummyTimer = 0.f;
	}
}

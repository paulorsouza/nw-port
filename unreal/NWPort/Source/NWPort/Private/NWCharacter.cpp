#include "NWCharacter.h"
#include "Misc/App.h"
#include "NWRules.h"
#include "NWInputRegistry.h"

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

// Port adjustment on top of the game's c_shakeMult (camera.shake_mult).
static TAutoConsoleVariable<float> CVarNWShakeScale(TEXT("nw.ShakeScale"), 1.f, TEXT("Multiplies the game's camera shakes"));

// camera.shake_mult × nw.ShakeScale.
static float ShakeScale()
{
	return float(FNWRules::Num(TEXT("camera.shake_mult"), 1.0)) * CVarNWShakeScale.GetValueOnGameThread();
}
#include "NWCombatComponent.h"
#include "NWTraversalComponent.h"
#include "NWGatherComponent.h"
#include "NWInteractable.h"
#include "NWProgressionComponent.h"
#include "NWManaComponent.h"
#include "NWInventoryComponent.h"
#include "NWMountComponent.h"
#include "NWOptions.h"
#include "NWPlayerController.h"
#include "Misc/ConfigCacheIni.h"
#include "NWAnimInstance.h"
#include "Animation/BlendSpace.h"
#include "AnimationRuntime.h"
#include "TimerManager.h"
#include "EngineUtils.h"

ANWCharacter::ANWCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 86.f);

	// Strafe: the body follows the camera (in Tick); while sprinting it follows the movement instead.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	Move->MaxWalkSpeed = RunSpeed;
	Move->JumpZVelocity = 450.f;
	// The game's maximum slope (CharacterComponent m_maxslope = 70) and moderate air control.
	Move->SetWalkableFloorAngle(70.f);
	Move->AirControl = 0.2f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	// Initial values of the game's DynamicCamera; UpdateCamera swaps them per state (sprint, inventory).
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 170.f - 86.f));
	CameraBoom->TargetArmLength = 200.f;
	CameraBoom->SocketOffset = FVector(0.f, 50.f, 0.f);
	CameraBoom->bUsePawnControlRotation = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	USkeletalMeshComponent* Body = GetMesh();
	// Mesh exported from Blender facing -Y; in Unreal the actor faces +X. Adjust here if it comes rotated.
	Body->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -86.f), FRotator(0.f, -90.f, 0.f));
	// Characters do not receive ground decals (weapon area effects, as in the game).
	Body->SetReceivesDecals(false);
	// The server needs the pose (window curves, root motion and blade position) only while a montage plays (hit,
	// dodge, reaction); idle or walking, nobody sees the pose on the server (docs/tech/architecture.md, measurements:
	// animation was ~80 µs per character). On the client it applies to whoever is off screen.
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesAndRefreshBonesWhenPlayingMontages;

	NockedArrow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NockedArrow"));
	NockedArrow->SetupAttachment(Body, TEXT("right_hand_attach"));
	NockedArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NockedArrow->SetVisibility(false);
	NockedArrow->SetRelativeRotation(FRotator(0.f, 0.f, -90.f));   // same correction as the weapons (tip at the mesh's +Z)
	Sword = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sword"));
	Sword->SetupAttachment(Body, TEXT("right_hand_attach"));
	Sword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Weapon rotation on the socket: Blender's glTF importer rotates the bones -90° on local X relative
	// to the game's joint; in the game the weapon goes into the joint without rotation (see tools/ue/grip_offsets.md).
	Sword->SetRelativeRotation(FRotator(0.f, 0.f, -90.f));
	Shield = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shield"));
	Shield->SetupAttachment(Body, TEXT("left_hand_attach"));
	Shield->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shield->SetRelativeRotation(FRotator(0.f, 0.f, -90.f));

	Stowed = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stowed"));
	Stowed->SetupAttachment(Body, TEXT("Hips"));
	Stowed->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Player weapons (NWCharacterArmas.cpp, weapons area).
	AddPlayerWeaponSets();

	Combat = CreateDefaultSubobject<UNWCombatComponent>(TEXT("Combat"));
	Traversal = CreateDefaultSubobject<UNWTraversalComponent>(TEXT("Traversal"));
	Inventory = CreateDefaultSubobject<UNWInventoryComponent>(TEXT("Inventory"));
	Gather = CreateDefaultSubobject<UNWGatherComponent>(TEXT("Gather"));
	Progression = CreateDefaultSubobject<UNWProgressionComponent>(TEXT("Progression"));
	Mana = CreateDefaultSubobject<UNWManaComponent>(TEXT("Mana"));
	Mount = CreateDefaultSubobject<UNWMountComponent>(TEXT("Mount"));
	Interactor = CreateDefaultSubobject<UNWInteractorComponent>(TEXT("Interactor"));
	Brain = CreateDefaultSubobject<UNWBrainComponent>(TEXT("Brain"));
	Brain->bAutoActivate = false;

	// Paths created by tools/ue/import_nw.py (sword_shield set).
	MeshAsset = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Mesh/SK_player_male.SK_player_male")));
	AnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/ABP_NW.ABP_NW_C")));
	SwordAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Mesh/SM_sword.SM_sword")));
	ShieldAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Mesh/SM_shield.SM_shield")));
	DamageTableAsset = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/NW/Data/DT_NWDamage.DT_NWDamage")));
	ActionTableAsset = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/NW/Data/DT_NWActions.DT_NWActions")));
}

void ANWCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANWCharacter, bSprinting);
	DOREPLIFETIME(ANWCharacter, bWalking);
	DOREPLIFETIME(ANWCharacter, DevSpeedMultiplier);
	DOREPLIFETIME(ANWCharacter, bDevFlying);
	DOREPLIFETIME(ANWCharacter, ActiveWeapon);
	DOREPLIFETIME(ANWCharacter, Loadout);
}

void ANWCharacter::LoadMissingAssets()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body->GetSkeletalMeshAsset() && !MeshAsset.IsNull()) Body->SetSkeletalMeshAsset(MeshAsset.LoadSynchronous());
	if (!Body->GetAnimClass() && !AnimClass.IsNull()) Body->SetAnimInstanceClass(AnimClass.LoadSynchronous());
	if (!Sword->GetStaticMesh() && !SwordAsset.IsNull()) Sword->SetStaticMesh(SwordAsset.LoadSynchronous());
	if (!Shield->GetStaticMesh() && !ShieldAsset.IsNull()) Shield->SetStaticMesh(ShieldAsset.LoadSynchronous());
	if (!Combat->DamageTable && !DamageTableAsset.IsNull()) Combat->DamageTable = DamageTableAsset.LoadSynchronous();
	if (!Combat->ActionTable && !ActionTableAsset.IsNull()) Combat->ActionTable = ActionTableAsset.LoadSynchronous();
}

void ANWCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	LoadMissingAssets();
}

void ANWCharacter::BeginPlay()
{
	LoadMissingAssets();
	Super::BeginPlay();
	// Client without rendering (test bots, -nullrhi): the pose only advances when drawn, and montages with root motion
	// and window curves stopped. The dedicated server always animates.
	if (!FApp::CanEverRender() && !IsRunningDedicatedServer())
	{
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	// Dedicated server: the mesh that is not a hit target (the hit sweeps the Pawn channel and hits the capsule; creatures use
	// the mesh) has no collision, and its physics bodies stop following the animation every cycle.
	if (GetNetMode() == NM_DedicatedServer && GetMesh()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore)
	{
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetMesh()->SetGenerateOverlapEvents(false);
	}
	Combat->SetMainWeapon(Sword);
	ApplyWeapon();
	GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
	if (HasAuthority() && bDummyBlocks && !IsPlayerControlled())
	{
		// After one frame, so the AnimInstance is already up.
		GetWorldTimerManager().SetTimerForNextTick([this]() { Combat->RequestBlock(true); });
	}
}

void ANWCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bAutoRun && IsLocallyControlled() && Controller && !InMenu() && Combat->State != ENWCombatState::Dead)
	{
		AddMovementInput(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f).Vector(), 1.f);
	}
	if (bDevFlying && IsLocallyControlled() && !InMenu())
	{
		AddMovementInput(FVector::UpVector, (bFlyUp ? 1.f : 0.f) - (bWalking ? 1.f : 0.f));
	}
	// Arrow in the game's right hand (CAGE-Attachment ActiveWeapon) while aiming and drawing; disappears on shot, returns on reload.
	{
		const FName A = Combat->GetCurrentAction();
		const bool bShow = Combat->bAimWeapon && Combat->bAiming && A != TEXT("AimShot") && A != TEXT("ChargedShot") && A != TEXT("PenetratingShot")
			&& A != TEXT("PoisonShot");
		if (bShow && !NockedArrow->GetStaticMesh())
		{
			NockedArrow->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/NW/sword_shield/Mesh/SM_arrow.SM_arrow")));
		}
		NockedArrow->SetVisibility(bShow);
	}
	UpdateAutoSprint();
	ApplySprint();
	// Camera arm (collision trace every cycle) only on the local player's character; on the server and on other
	// characters (including creatures, which on the server are "locally controlled" by the AI) it stays still.
	const bool bLocalView = IsLocallyControlled() && IsPlayerControlled();
	if (CameraBoom && CameraBoom->IsComponentTickEnabled() != bLocalView)
	{
		CameraBoom->SetComponentTickEnabled(bLocalView);
	}
	UpdateCamera(DeltaSeconds);
	UpdateCameraClipHide();
	if (HasAuthority() && bDummyAttacks && !IsPlayerControlled() && !Brain->IsActive())
	{
		TickDummy(DeltaSeconds);
	}

	// The body follows the camera (strafe) outside attack/dodge/sprint; during an attack only in the Homing window.
	// Rotation done here, directly: bUseControllerRotationYaw toggled every frame was not being applied.
	bUseControllerRotationYaw = false;
	const bool bBusy = Combat->IsBusy() || Traversal->IsTraversing();
	if (Controller && IsPlayerControlled())
	{
		const FRotator Target(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const ANWPlayerController* NWPC = Cast<ANWPlayerController>(Controller);
		if (NWPC && NWPC->IsInventoryOpen() && !bBusy && GetVelocity().Size2D() < 10.f)
		{
			// Inventory open: the character turns to face the camera (the game's BlendLookDirection).
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Target.Yaw + 180.f, 0.f), DeltaSeconds, 8.f));
		}
		else if ((!bBusy || Combat->IsCharging() || Combat->bAiming) && !bSprinting)
		{
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), Target, DeltaSeconds, 18.f));
		}
		else if (bBusy && Combat->GetCurve(TEXT("Homing")) > 0.5f)
		{
			// The game's Homing: turns toward the nearest enemy in front of the camera (radius ~2.75 m, 45° cone, Homing
			// windows from the .adb); with nobody there, toward where the camera looks.
			FRotator Aim = Target;
			float Best = 275.f;
			const FVector CamFwd = Target.Vector();
			for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
			{
				if (*It == this || It->IsPlayerLike() || It->Combat->State == ENWCombatState::Dead)
				{
					continue;
				}
				const FVector To = It->GetActorLocation() - GetActorLocation();
				const float D = To.Size2D() - It->GetCapsuleComponent()->GetScaledCapsuleRadius();
				if (D < Best && FVector::DotProduct(To.GetSafeNormal2D(), CamFwd) > FMath::Cos(FMath::DegreesToRadians(45.f)))
				{
					Best = D;
					Aim = FRotator(0.f, To.Rotation().Yaw, 0.f);
				}
			}
			SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Aim, DeltaSeconds, HomingTurnRate));
		}
	}
}

void ANWCharacter::ApplySprint()
{
	// Overloaded (weight > maximum): walk only, as in the game ("cannot run, dodge or jump").
	const bool bEncumbered = Inventory && Inventory->IsEncumbered();
	const bool bCanSprint = bSprinting && !bWalking && !bEncumbered && !Combat->IsBusy() && Combat->State != ENWCombatState::Blocking && !Combat->bAiming;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	// Aiming with the Bow walks at 1.65 m/s (the game's AimCamera).
	const float Speed = bWalking || bEncumbered || Combat->bAiming ? WalkSpeed
		: bCanSprint ? SprintSpeed
		: Combat->State == ENWCombatState::Blocking ? RunSpeed * 0.4f : RunSpeed;
	Move->MaxWalkSpeed = Speed * FMath::Max(1.f, DevSpeedMultiplier) * Combat->GetMoveSpeedMultiplier();
	Move->MaxFlySpeed = SprintSpeed * 2.f * FMath::Max(1.f, DevSpeedMultiplier);
	Move->bOrientRotationToMovement = bCanSprint;
}

void ANWCharacter::PlayCameraShake(FName Id)
{
	if (!IsLocallyControlled() || Id.IsNone())
	{
		return;
	}
	if (!ShakeTable)
	{
		ShakeTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/NW/Data/DT_NWCameraShake.DT_NWCameraShake"));
	}
	// Damage table names sometimes have a trailing space ("Small "); nonexistent ID = no shake (as in the game).
	const FNWCameraShakeRow* R = ShakeTable ? ShakeTable->FindRow<FNWCameraShakeRow>(FName(*Id.ToString().TrimStartAndEnd()), TEXT("shake"), false) : nullptr;
	// camera.shake_min: a negligible total scale ignores the shake (CryEngine's View.cpp).
	if (!R || ShakeScale() < FNWRules::Num(TEXT("camera.shake_min"), 0.001))
	{
		return;
	}
	ActiveShake = FShake();
	ActiveShake.bActive = true;
	ActiveShake.Shift = FVector(R->ShiftX, R->ShiftY, R->ShiftZ);
	ActiveShake.Angle = FVector(R->AngleX, R->AngleY, R->AngleZ);
	ActiveShake.FadeIn = R->FadeIn;
	ActiveShake.Sustain = R->Sustain;
	ActiveShake.FadeOut = R->FadeOut;
	ActiveShake.Period = R->Frequency;
	ActiveShake.Randomness = R->Randomness;
	ActiveShake.Flip = R->FlipVec ? 1.f : -1.f;
}

void ANWCharacter::ApplyDevFlying()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->BrakingDecelerationFlying = 6000.f;
	Move->SetMovementMode(bDevFlying ? MOVE_Flying : MOVE_Falling);
	// Passes through terrain, rocks and buildings; still hits and gets hit (Pawn keeps blocking).
	const ECollisionResponse R = bDevFlying ? ECR_Ignore : ECR_Block;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, R);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, R);
	bFlyUp = false;
}

void ANWCharacter::UpdateAutoSprint()
{
	if (!IsLocallyControlled() || !Controller)
	{
		return;
	}
	// Going toward the camera's front (up to ~45° to the sides), not walking with Ctrl, no attack/block: sprint.
	const FVector Accel = GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	const FVector Forward = FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f).Vector();
	const bool bWant = !Accel.IsNearlyZero() && FVector::DotProduct(Accel, Forward) > 0.7f && !bWalking && !Combat->bAiming
		&& !Combat->IsBusy() && Combat->State != ENWCombatState::Blocking;
	if (bWant != bSprinting)
	{
		bSprinting = bWant;
		ServerSetSprinting(bWant);
	}
}

// ---------------------------------------------------------------- input

static UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
{
	UInputAction* A = NewObject<UInputAction>(Outer, Name);
	A->ValueType = Type;
	return A;
}

void ANWCharacter::BuildInput()
{
	if (Mapping)
	{
		return;
	}
	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_NW"));
	IA_Move = MakeAction(this, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	IA_Look = MakeAction(this, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	IA_Zoom = MakeAction(this, TEXT("IA_Zoom"), EInputActionValueType::Axis1D);
	IA_Attack = MakeAction(this, TEXT("IA_Attack"), EInputActionValueType::Boolean);
	IA_Block = MakeAction(this, TEXT("IA_Block"), EInputActionValueType::Boolean);
	IA_Dodge = MakeAction(this, TEXT("IA_Dodge"), EInputActionValueType::Boolean);
	IA_Walk = MakeAction(this, TEXT("IA_Walk"), EInputActionValueType::Boolean);
	IA_Jump = MakeAction(this, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	IA_SkillMenu = MakeAction(this, TEXT("IA_SkillMenu"), EInputActionValueType::Boolean);
	IA_DevMenu = MakeAction(this, TEXT("IA_DevMenu"), EInputActionValueType::Boolean);
	IA_Weapon1 = MakeAction(this, TEXT("IA_Weapon1"), EInputActionValueType::Boolean);
	IA_Weapon2 = MakeAction(this, TEXT("IA_Weapon2"), EInputActionValueType::Boolean);
	IA_Potion = MakeAction(this, TEXT("IA_Potion"), EInputActionValueType::Boolean);
	IA_Inventory = MakeAction(this, TEXT("IA_Inventory"), EInputActionValueType::Boolean);
	IA_Interact = MakeAction(this, TEXT("IA_Interact"), EInputActionValueType::Boolean);
	for (const TCHAR* N : {TEXT("IA_Ability1"), TEXT("IA_Ability2"), TEXT("IA_Ability3")})
	{
		IA_Abilities.Add(MakeAction(this, N, EInputActionValueType::Boolean));
	}

	IA_AutoRun = MakeAction(this, TEXT("IA_AutoRun"), EInputActionValueType::Boolean);
	IA_Map = MakeAction(this, TEXT("IA_Map"), EInputActionValueType::Boolean);
	// Keys of the areas (NWInputRegistry.h), in registry order.
	for (const FNWInputBinding& B : FNWInputRegistry::All())
	{
		IA_Registered.Add(MakeAction(this, *(TEXT("IA_") + B.Id.ToString()), EInputActionValueType::Boolean));
	}
	MapKeys();
}

void ANWCharacter::MapKeys()
{
	Mapping->UnmapAll();
	const FNWOptions& O = FNWOptions::Get();
	auto Key = [&O](const TCHAR* Id) { return O.KeyFor(Id); };
	// Movement -> 2D axis (X = side, Y = forward), as in the Unreal template.
	auto MapMove = [this](FKey K, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& M = Mapping->MapKey(IA_Move, K);
		if (bSwizzle) M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Mapping));
		if (bNegate) M.Modifiers.Add(NewObject<UInputModifierNegate>(Mapping));
	};
	MapMove(Key(TEXT("MoveForward")), true, false);
	MapMove(Key(TEXT("MoveBack")), true, true);
	MapMove(Key(TEXT("MoveRight")), false, false);
	MapMove(Key(TEXT("MoveLeft")), false, true);
	Mapping->MapKey(IA_Look, EKeys::Mouse2D);
	Mapping->MapKey(IA_Zoom, EKeys::MouseWheelAxis);
	Mapping->MapKey(IA_Attack, Key(TEXT("Attack")));
	Mapping->MapKey(IA_Block, Key(TEXT("Block")));
	Mapping->MapKey(IA_Jump, Key(TEXT("Jump")));
	Mapping->MapKey(IA_SkillMenu, Key(TEXT("Mastery")));
	Mapping->MapKey(IA_DevMenu, EKeys::F10);
	Mapping->MapKey(IA_Weapon1, Key(TEXT("Weapon1")));
	Mapping->MapKey(IA_Weapon2, Key(TEXT("Weapon2")));
	Mapping->MapKey(IA_Potion, Key(TEXT("Consumable1")));
	Mapping->MapKey(IA_Inventory, Key(TEXT("Inventory")));
	Mapping->MapKey(IA_Interact, Key(TEXT("Interact")));
	Mapping->MapKey(IA_Dodge, Key(TEXT("Dodge")));
	Mapping->MapKey(IA_Walk, Key(TEXT("Walk")));
	Mapping->MapKey(IA_AutoRun, Key(TEXT("AutoRun")));
	Mapping->MapKey(IA_Map, Key(TEXT("Map")));
	Mapping->MapKey(IA_Abilities[0], Key(TEXT("Ability1")));
	Mapping->MapKey(IA_Abilities[1], Key(TEXT("Ability2")));
	Mapping->MapKey(IA_Abilities[2], Key(TEXT("Ability3")));
	const TArray<FNWInputBinding>& Registered = FNWInputRegistry::All();
	for (int32 i = 0; i < Registered.Num() && i < IA_Registered.Num(); ++i)
	{
		Mapping->MapKey(IA_Registered[i], O.KeyFor(Registered[i].Id));
	}
}

void ANWCharacter::RebuildKeys()
{
	if (!Mapping)
	{
		return;
	}
	MapKeys();
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->RequestRebuildControlMappings();
		}
	}
}

void ANWCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInput();
	LoadAbilityLoadout();
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->AddMappingContext(Mapping, 0);
		}
	}
	UEnhancedInputComponent* In = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	In->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ANWCharacter::Move);
	In->BindAction(IA_Move, ETriggerEvent::Completed, this, &ANWCharacter::Move);
	In->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ANWCharacter::Look);
	In->BindAction(IA_Zoom, ETriggerEvent::Triggered, this, &ANWCharacter::Zoom);
	In->BindAction(IA_Attack, ETriggerEvent::Started, this, &ANWCharacter::AttackPressed);
	In->BindAction(IA_Attack, ETriggerEvent::Completed, this, &ANWCharacter::AttackReleased);
	In->BindAction(IA_Block, ETriggerEvent::Started, this, &ANWCharacter::BlockPressed);
	In->BindAction(IA_Block, ETriggerEvent::Completed, this, &ANWCharacter::BlockReleased);
	In->BindAction(IA_Dodge, ETriggerEvent::Started, this, &ANWCharacter::Dodge);
	In->BindAction(IA_Jump, ETriggerEvent::Started, this, &ANWCharacter::JumpPressed);
	In->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ANWCharacter::JumpReleased);
	In->BindAction(IA_SkillMenu, ETriggerEvent::Started, this, &ANWCharacter::SkillMenuPressed);
	In->BindAction(IA_DevMenu, ETriggerEvent::Started, this, &ANWCharacter::DevMenuPressed);
	In->BindAction(IA_Weapon1, ETriggerEvent::Started, this, &ANWCharacter::Weapon1);
	In->BindAction(IA_Weapon2, ETriggerEvent::Started, this, &ANWCharacter::Weapon2);
	In->BindAction(IA_Potion, ETriggerEvent::Started, this, &ANWCharacter::UsePotion);
	In->BindAction(IA_Inventory, ETriggerEvent::Started, this, &ANWCharacter::InventoryPressed);
	In->BindAction(IA_Interact, ETriggerEvent::Started, this, &ANWCharacter::InteractPressed);
	In->BindAction(IA_Walk, ETriggerEvent::Started, this, &ANWCharacter::WalkPressed);
	In->BindAction(IA_AutoRun, ETriggerEvent::Started, this, &ANWCharacter::AutoRunPressed);
	In->BindAction(IA_Map, ETriggerEvent::Started, this, &ANWCharacter::MapPressed);
	In->BindAction(IA_Walk, ETriggerEvent::Completed, this, &ANWCharacter::WalkReleased);
	In->BindAction(IA_Abilities[0], ETriggerEvent::Started, this, &ANWCharacter::Ability1);
	In->BindAction(IA_Abilities[1], ETriggerEvent::Started, this, &ANWCharacter::Ability2);
	In->BindAction(IA_Abilities[2], ETriggerEvent::Started, this, &ANWCharacter::Ability3);
	const TArray<FNWInputBinding>& Registered = FNWInputRegistry::All();
	for (int32 i = 0; i < Registered.Num() && i < IA_Registered.Num(); ++i)
	{
		const FNWInputBinding& B = Registered[i];
		if (B.Pressed)
		{
			In->BindActionValueLambda(IA_Registered[i], ETriggerEvent::Started, [this, F = B.Pressed](const FInputActionValue&) { if (!InMenu()) F(this); });
		}
		if (B.Released)
		{
			In->BindActionValueLambda(IA_Registered[i], ETriggerEvent::Completed, [this, F = B.Released](const FInputActionValue&) { F(this); });
		}
	}
}

void ANWCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	LastMoveInput = FVector2D(V.Y, V.X); // (forward, right) for the dodge
	if (!FMath::IsNearlyZero(V.Y))
	{
		bAutoRun = false;   // forward or back cancels auto-run, as in the game
	}
	if (!Controller || V.IsNearlyZero())
	{
		return;
	}
	if (bDevFlying)
	{
		const FRotationMatrix View(Controller->GetControlRotation());
		AddMovementInput(View.GetUnitAxis(EAxis::X), V.Y);
		AddMovementInput(View.GetUnitAxis(EAxis::Y), V.X);
		return;
	}
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), V.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), V.X);
}

void ANWCharacter::Zoom(const FInputActionValue& Value)
{
	if (InMenu())
	{
		return;
	}
	// The game's zoomAmount: 1 m per wheel click; up zooms in.
	CameraZoom = FMath::Clamp(CameraZoom - Value.Get<float>() * 100.f, CameraZoomMin, CameraZoomMax);
}

void ANWCharacter::UpdateCamera(float DeltaSeconds)
{
	if (!IsLocallyControlled() || !IsPlayerControlled() || !CameraBoom || !Camera)
	{
		return;
	}
	const ANWPlayerController* PC = Cast<ANWPlayerController>(Controller);
	const bool bInventory = PC && PC->IsInventoryOpen();
	// gamecamera.xml states: (right, height above the feet, distance, min/max pitch).
	float Right = 50.f, Height = 170.f, Distance = CameraZoom, PitchMin = -70.f, PitchMax = 60.f;
	if (bInventory)
	{
		Right = 77.f; Height = 95.f; Distance = 275.f; PitchMin = -70.f; PitchMax = -10.f;   // UI_Inventory
	}
	else if (Combat->bAiming)
	{
		Distance = 100.f;                                                                        // AimCamera (Bow)
	}
	else if (bSprinting && GetVelocity().Size2D() > RunSpeed)
	{
		Right = 70.f; Distance = FMath::Max(Distance, 400.f);                                   // MidCameraSprint
	}
	// 0.2 s transition (the game's SmoothTime).
	const float Speed = 1.f / 0.2f;
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector Loc = CameraBoom->GetRelativeLocation();
	Loc.Z = FMath::FInterpTo(Loc.Z, Height - HalfHeight, DeltaSeconds, Speed);
	CameraBoom->SetRelativeLocation(Loc);
	CameraBoom->SocketOffset.Y = FMath::FInterpTo(CameraBoom->SocketOffset.Y, Right, DeltaSeconds, Speed);
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, Distance, DeltaSeconds, Speed);
	FVector Shake = FVector::ZeroVector;
	if (CameraKick > 0.f)
	{
		CameraKickTime += DeltaSeconds;
		const float A = CameraKick * 5.f;
		Shake = FVector(0.f, FMath::Sin(CameraKickTime * 60.f) * A, FMath::Sin(CameraKickTime * 47.f + 1.f) * A * 0.7f);
		CameraKick = FMath::Max(0.f, CameraKick - DeltaSeconds * 4.f);
	}
	// The game's shake: random target every period (alternating sign), interpolated and in the smoothed envelope.
	FRotator ShakeRot = FRotator::ZeroRotator;
	if (ActiveShake.bActive)
	{
		FShake& S = ActiveShake;
		S.Time += DeltaSeconds;
		const float Total = S.FadeIn + S.Sustain + S.FadeOut;
		if (S.Time >= Total)
		{
			S.bActive = false;
		}
		else
		{
			if (S.Time >= S.NextTarget)
			{
				S.FromShift = S.ToShift;
				S.FromAngle = S.ToAngle;
				S.Flip = -S.Flip;
				auto Rnd = [&S](const FVector& V) { const float N = S.Randomness * (FMath::Abs(V.X) + FMath::Abs(V.Y) + FMath::Abs(V.Z)) / 3.f;
					return FVector(V.X * FMath::FRandRange(0.5f, 1.f) + FMath::FRandRange(-N, N), V.Y * FMath::FRandRange(0.5f, 1.f) + FMath::FRandRange(-N, N),
						V.Z * FMath::FRandRange(0.5f, 1.f) + FMath::FRandRange(-N, N)); };
				S.ToShift = Rnd(S.Shift) * S.Flip;
				S.ToAngle = Rnd(S.Angle) * S.Flip;
				S.NextTarget = S.Time + FMath::Max(S.Period, 0.005f);
			}
			const float A = FMath::Clamp(1.f - (S.NextTarget - S.Time) / FMath::Max(S.Period, 0.005f), 0.f, 1.f);
			const float Sm = A * A * (3.f - 2.f * A);
			float Env = S.Time < S.FadeIn ? S.Time / FMath::Max(S.FadeIn, 0.001f) : S.Time > S.FadeIn + S.Sustain ? (Total - S.Time) / FMath::Max(S.FadeOut, 0.001f) : 1.f;
			Env = Env * Env * (3.f - 2.f * Env);
			const FVector Sh = FMath::Lerp(S.FromShift, S.ToShift, Sm) * Env * 100.f * ShakeScale();
			const FVector An = FMath::Lerp(S.FromAngle, S.ToAngle, Sm) * Env * ShakeScale();
			// CryEngine camera axes: X right, Y forward, Z up → Unreal X forward, Y right, Z up.
			Shake += FVector(Sh.Y, Sh.X, Sh.Z);
			ShakeRot = FRotator(FMath::RadiansToDegrees(An.X), FMath::RadiansToDegrees(An.Z), FMath::RadiansToDegrees(An.Y));
		}
	}
	Camera->SetRelativeLocation(Shake);
	Camera->SetRelativeRotation(ShakeRot);
	if (APlayerCameraManager* PCM = PC ? PC->PlayerCameraManager.Get() : nullptr)
	{
		PCM->ViewPitchMin = PitchMin;
		PCM->ViewPitchMax = PitchMax;
	}
	// The game's FOV is vertical; Unreal uses horizontal: converts by the screen aspect ratio.
	FVector2D Viewport(16.f, 9.f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(Viewport);
	}
	const float Aspect = Viewport.Y > 0.f ? Viewport.X / Viewport.Y : 16.f / 9.f;
	// Field of view from the player option; in the inventory the game ignores the option (UseUserDefinedFov = false).
	const float Vertical = bInventory ? CameraFovVertical : FNWOptions::Get().FovVertical;
	Camera->SetFieldOfView(FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Vertical) * 0.5f) * Aspect)));
}

void ANWCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	const FNWOptions& O = FNWOptions::Get();
	AddControllerYawInput(V.X * O.SensitivityX);
	AddControllerPitchInput(-V.Y * O.SensitivityY * (O.bInvertY ? -1.f : 1.f));
}

bool ANWCharacter::InMenu() const
{
	const ANWPlayerController* PC = Cast<ANWPlayerController>(Controller);
	return PC && (PC->IsMenuOpen() || PC->MenuClickFrame == GFrameCounter);
}

bool ANWCharacter::CanAct() const
{
	return !Traversal->IsTraversing() && !GetCharacterMovement()->IsFalling();
}

void ANWCharacter::JumpPressed()
{
	if (bDevFlying)
	{
		bFlyUp = true;
		return;
	}
	if (Inventory && Inventory->IsEncumbered())
	{
		return;
	}
	if (InMenu())
	{
		return;
	}
	// As in the game: jumps outside attack/dodge; with the guard up, lowers it and jumps.
	if (Combat->IsBusy() || Traversal->IsTraversing())
	{
		return;
	}
	if (Combat->State == ENWCombatState::Blocking)
	{
		Combat->RequestBlock(false);
	}
	Traversal->JumpPressed();
}

void ANWCharacter::JumpReleased()
{
	bFlyUp = false;
	StopJumping();
}

void ANWCharacter::SkillMenuPressed()
{
	if (ANWPlayerController* PC = Cast<ANWPlayerController>(Controller))
	{
		PC->ToggleSkillMenu();
	}
}

void ANWCharacter::UsePotion()
{
	if (!InMenu() && Inventory)
	{
		Inventory->ServerUseBestPotion();
	}
}

void ANWCharacter::MapPressed()
{
	if (ANWPlayerController* PC = Cast<ANWPlayerController>(Controller))
	{
		PC->ToggleMap();
	}
}

void ANWCharacter::InventoryPressed()
{
	if (ANWPlayerController* PC = Cast<ANWPlayerController>(Controller))
	{
		PC->ToggleInventory();
	}
}

void ANWCharacter::InteractPressed()
{
	if (!Interactor->InteractPressed())
	{
		Gather->InteractPressed();
	}
}

void ANWCharacter::DevMenuPressed()
{
	if (ANWPlayerController* PC = Cast<ANWPlayerController>(Controller))
	{
		PC->ToggleDevMenu();
	}
}

// Ability choice stored in Saved/Config/<platform>/GameUserSettings.ini; with nw-core, the server
// keeps the choice and sends it back on join (ANWPlayerController::OnRep_Progress).

void ANWCharacter::WalkPressed()
{
	bWalking = true;
	ServerSetWalking(true);
}

void ANWCharacter::WalkReleased()
{
	bWalking = false;
	ServerSetWalking(false);
}

void ANWCharacter::ServerSetWalking_Implementation(bool bNew)
{
	bWalking = bNew;
}

void ANWCharacter::ServerSetSprinting_Implementation(bool bNew)
{
	bSprinting = bNew;
}

static TAutoConsoleVariable<int32> CVarDebugCamera(TEXT("nw.DebugCamera"), 0, TEXT("Logs the camera (pitch, arm, distance) every second."));

// Low ceiling/wall behind: the camera arm shrinks to inside the body. In that case the player stops
// seeing their own character (others still see it), as in third-person games.
void ANWCharacter::UpdateCameraClipHide()
{
	if (!IsLocallyControlled() || !IsPlayerControlled() || !Camera)
	{
		return;
	}
	const FVector Chest = GetActorLocation() + FVector(0.f, 0.f, 40.f);
	const float Dist = FVector::Dist(Camera->GetComponentLocation(), Chest);
	const bool bHide = Dist < CameraHideDistance;
	if (CVarDebugCamera.GetValueOnGameThread() != 0 && GFrameCounter % 30 == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("NW camera: pitch %.1f  arm %.0f/%.0f  dist %.0f  pos %s  mov %d"), GetControlRotation().Pitch,
			FVector::Dist(CameraBoom->GetComponentLocation(), Camera->GetComponentLocation()), CameraBoom->TargetArmLength, Dist,
			*GetActorLocation().ToCompactString(), int32(GetCharacterMovement()->MovementMode));
	}
	if (bHide == bHiddenForCamera)
	{
		return;
	}
	bHiddenForCamera = bHide;
	TArray<UPrimitiveComponent*> Parts;
	GetComponents<UPrimitiveComponent>(Parts);
	for (UPrimitiveComponent* P : Parts)
	{
		if (P != GetCapsuleComponent())
		{
			P->SetOwnerNoSee(bHide);
		}
	}
}

void ANWCharacter::SetLocomotionOverride(FName OverrideOwner, UBlendSpace* Space, int32 Priority, bool bNoSprint)
{
	FNWLocomotionOverride& O = LocomotionOverrides.FindOrAdd(OverrideOwner);
	O.Space = Space;
	O.Priority = Priority;
	O.bNoSprint = bNoSprint;
	RefreshLocomotion();
}

void ANWCharacter::ClearLocomotionOverride(FName OverrideOwner)
{
	if (LocomotionOverrides.Remove(OverrideOwner) > 0)
	{
		RefreshLocomotion();
	}
}

bool ANWCharacter::LocomotionBlocksSprint() const
{
	for (const TPair<FName, FNWLocomotionOverride>& P : LocomotionOverrides)
	{
		if (P.Value.bNoSprint)
		{
			return true;
		}
	}
	return false;
}

void ANWCharacter::RefreshLocomotion()
{
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return;
	}
	UBlendSpace* BS = nullptr;
	FName From = NAME_None;
	int32 Best = MIN_int32;
	for (const TPair<FName, FNWLocomotionOverride>& P : LocomotionOverrides)
	{
		if (P.Value.Space && P.Value.Priority > Best)
		{
			Best = P.Value.Priority;
			BS = P.Value.Space;
			From = P.Key;
		}
	}
	if (!BS && WeaponSets.IsValidIndex(ActiveWeapon))
	{
		BS = WeaponSets[ActiveWeapon].Locomotion.LoadSynchronous();
		From = WeaponSets[ActiveWeapon].Id;
	}
	// By name: works for the C++ variable (NWAnimInstance) and for one created in the ABP itself.
	int32 Assigned = 0;
	for (TFieldIterator<FObjectProperty> It(Anim->GetClass()); It; ++It)
	{
		if (It->GetFName() == TEXT("Locomotion") && It->PropertyClass->IsChildOf(UBlendSpace::StaticClass()))
		{
			It->SetObjectPropertyValue_InContainer(Anim, BS);
			++Assigned;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("NW locomotion %s -> %s (%d variable(s) in %s)"), *From.ToString(),
		BS ? *BS->GetName() : TEXT("none"), Assigned, *Anim->GetClass()->GetName());
}

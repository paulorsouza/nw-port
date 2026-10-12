#include "NWCharacter.h"
#include "Misc/App.h"

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
#include "NWCombatComponent.h"
#include "NWTraversalComponent.h"
#include "NWGatherComponent.h"
#include "NWInventoryComponent.h"
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

	// Strafe: o corpo segue a câmera (no Tick); no sprint passa a seguir o movimento.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	Move->MaxWalkSpeed = RunSpeed;
	Move->JumpZVelocity = 450.f;
	// Rampa máxima do jogo (CharacterComponent m_maxslope = 70) e controle no ar moderado.
	Move->SetWalkableFloorAngle(70.f);
	Move->AirControl = 0.2f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	// Valores iniciais da DynamicCamera do jogo; UpdateCamera troca por estado (sprint, inventário).
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 170.f - 86.f));
	CameraBoom->TargetArmLength = 200.f;
	CameraBoom->SocketOffset = FVector(0.f, 50.f, 0.f);
	CameraBoom->bUsePawnControlRotation = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	USkeletalMeshComponent* Body = GetMesh();
	// Malha exportada do Blender olhando para -Y; no Unreal o ator olha para +X. Ajustar aqui se vier girada.
	Body->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -86.f), FRotator(0.f, -90.f, 0.f));
	// O servidor precisa da pose (curvas e posição da lâmina) mesmo sem renderizar.
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;


	Sword = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sword"));
	Sword->SetupAttachment(Body, TEXT("right_hand_attach"));
	Sword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Rotação da arma no socket: o importador de glTF do Blender gira os ossos -90° no X local em relação
	// à junta do jogo; no jogo a arma entra na junta sem rotação (ver tools/ue/grip_offsets.md).
	Sword->SetRelativeRotation(FRotator(0.f, 0.f, -90.f));
	Shield = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shield"));
	Shield->SetupAttachment(Body, TEXT("left_hand_attach"));
	Shield->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shield->SetRelativeRotation(FRotator(0.f, 0.f, -90.f));

	Stowed = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stowed"));
	Stowed->SetupAttachment(Body, TEXT("Hips"));
	Stowed->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Armas do jogador: espada e escudo (1) e machadinha (2). Caminhos criados pelo import do conjunto sword_shield.
	{
		FNWWeaponSet SwordSet;
		SwordSet.Id = TEXT("Sword");
		SwordSet.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Mesh/SM_sword.SM_sword")));
		SwordSet.bUsesShield = true;
		SwordSet.Actions = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/NW/Data/DT_NWActions.DT_NWActions")));
		SwordSet.Locomotion = TSoftObjectPtr<UBlendSpace>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Anims/BS_Strafe.BS_Strafe")));
		SwordSet.HudIcon = TEXT("lyshineui/images/icons/items/drawing/1hsword.dds");
		SwordSet.Abilities = {TEXT("ShieldBash"), TEXT("SpinAttack"), TEXT("SwipeStab")};
		WeaponSets.Add(SwordSet);
		FNWWeaponSet Hatchet;
		Hatchet.Id = TEXT("Hatchet");
		Hatchet.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Mesh/SM_hatchet.SM_hatchet")));
		Hatchet.Actions = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/NW/Data/DT_HatchetActions.DT_HatchetActions")));
		Hatchet.Locomotion = TSoftObjectPtr<UBlendSpace>(FSoftObjectPath(TEXT("/Game/NW/sword_shield/Anims/BS_Hatchet_Strafe.BS_Hatchet_Strafe")));
		Hatchet.HudIcon = TEXT("lyshineui/images/icons/items/drawing/1hhatchet.dds");
		Hatchet.Abilities = {TEXT("Berserk"), TEXT("RagingTorrent"), TEXT("RendingThrow")};
		WeaponSets.Add(Hatchet);
	}

	Combat = CreateDefaultSubobject<UNWCombatComponent>(TEXT("Combat"));
	Traversal = CreateDefaultSubobject<UNWTraversalComponent>(TEXT("Traversal"));
	Inventory = CreateDefaultSubobject<UNWInventoryComponent>(TEXT("Inventory"));
	Gather = CreateDefaultSubobject<UNWGatherComponent>(TEXT("Gather"));

	// Caminhos criados pelo tools/ue/import_nw.py (conjunto sword_shield).
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
	DOREPLIFETIME(ANWCharacter, ActiveWeapon);
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
	// Cliente sem render (bots de teste, -nullrhi): a pose só anda quando desenhada, e as montagens com root motion
	// e as curvas de janela paravam. O servidor dedicado já anima sempre.
	if (!FApp::CanEverRender() && !IsRunningDedicatedServer())
	{
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	Combat->SetMainWeapon(Sword);
	ApplyWeapon();
	GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
	if (HasAuthority() && bDummyBlocks && !IsPlayerControlled())
	{
		// Depois de um quadro, para a AnimInstance já estar de pé.
		GetWorldTimerManager().SetTimerForNextTick([this]() { Combat->RequestBlock(true); });
	}
}

void ANWCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateAutoSprint();
	ApplySprint();
	UpdateCamera(DeltaSeconds);
	UpdateCameraClipHide();
	if (HasAuthority() && bDummyAttacks && !IsPlayerControlled())
	{
		TickDummy(DeltaSeconds);
	}

	// O corpo segue a câmera (strafe) fora de ataque/esquiva/sprint; em ataque só na janela Homing.
	// Giro feito aqui, direto: o bUseControllerRotationYaw alternado a cada quadro não estava aplicando.
	bUseControllerRotationYaw = false;
	const bool bBusy = Combat->IsBusy() || Traversal->IsTraversing();
	if (Controller && IsPlayerControlled())
	{
		const FRotator Target(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const ANWPlayerController* NWPC = Cast<ANWPlayerController>(Controller);
		if (NWPC && NWPC->IsInventoryOpen() && !bBusy && GetVelocity().Size2D() < 10.f)
		{
			// Inventário aberto: o personagem vira de frente para a câmera (BlendLookDirection do jogo).
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Target.Yaw + 180.f, 0.f), DeltaSeconds, 8.f));
		}
		else if ((!bBusy || Combat->IsCharging()) && !bSprinting)
		{
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), Target, DeltaSeconds, 18.f));
		}
		else if (bBusy && Combat->GetCurve(TEXT("Homing")) > 0.5f)
		{
			SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Target, DeltaSeconds, HomingTurnRate));
		}
	}
}

void ANWCharacter::ApplySprint()
{
	// Sobrecarregado (peso > máximo): só anda, como no jogo ("não é possível correr, se esquivar ou pular").
	const bool bEncumbered = Inventory && Inventory->IsEncumbered();
	const bool bCanSprint = bSprinting && !bWalking && !bEncumbered && !Combat->IsBusy() && Combat->State != ENWCombatState::Blocking;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const float Speed = bWalking || bEncumbered ? WalkSpeed
		: bCanSprint ? SprintSpeed
		: Combat->State == ENWCombatState::Blocking ? RunSpeed * 0.4f : RunSpeed;
	Move->MaxWalkSpeed = Speed * FMath::Max(1.f, DevSpeedMultiplier);
	Move->bOrientRotationToMovement = bCanSprint;
}

void ANWCharacter::UpdateAutoSprint()
{
	if (!IsLocallyControlled() || !Controller)
	{
		return;
	}
	// Indo para a frente da câmera (até ~45° para os lados), sem andar com Ctrl, sem ataque/bloqueio: sprint.
	const FVector Accel = GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	const FVector Forward = FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f).Vector();
	const bool bWant = !Accel.IsNearlyZero() && FVector::DotProduct(Accel, Forward) > 0.7f && !bWalking
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

	// WASD -> eixo 2D (X = lado, Y = frente), como no template do Unreal.
	auto MapMove = [this](FKey Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& M = Mapping->MapKey(IA_Move, Key);
		if (bSwizzle) M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Mapping));
		if (bNegate) M.Modifiers.Add(NewObject<UInputModifierNegate>(Mapping));
	};
	MapMove(EKeys::W, true, false);
	MapMove(EKeys::S, true, true);
	MapMove(EKeys::D, false, false);
	MapMove(EKeys::A, false, true);
	Mapping->MapKey(IA_Look, EKeys::Mouse2D);
	Mapping->MapKey(IA_Zoom, EKeys::MouseWheelAxis);
	Mapping->MapKey(IA_Attack, EKeys::LeftMouseButton);
	Mapping->MapKey(IA_Block, EKeys::RightMouseButton);
	Mapping->MapKey(IA_Jump, EKeys::SpaceBar);
	Mapping->MapKey(IA_SkillMenu, EKeys::K);
	Mapping->MapKey(IA_DevMenu, EKeys::F10);
	Mapping->MapKey(IA_Weapon1, EKeys::One);
	Mapping->MapKey(IA_Weapon2, EKeys::Two);
	// Teclas do jogo: quickslot-consumable-1 = 3; inventário = Tab.
	Mapping->MapKey(IA_Potion, EKeys::Three);
	Mapping->MapKey(IA_Inventory, EKeys::Tab);
	Mapping->MapKey(IA_Interact, EKeys::E);
	Mapping->MapKey(IA_Dodge, EKeys::LeftShift);
	Mapping->MapKey(IA_Walk, EKeys::LeftControl);
	Mapping->MapKey(IA_Abilities[0], EKeys::Q);
	Mapping->MapKey(IA_Abilities[1], EKeys::R);
	Mapping->MapKey(IA_Abilities[2], EKeys::F);
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
	In->BindAction(IA_Walk, ETriggerEvent::Completed, this, &ANWCharacter::WalkReleased);
	In->BindAction(IA_Abilities[0], ETriggerEvent::Started, this, &ANWCharacter::Ability1);
	In->BindAction(IA_Abilities[1], ETriggerEvent::Started, this, &ANWCharacter::Ability2);
	In->BindAction(IA_Abilities[2], ETriggerEvent::Started, this, &ANWCharacter::Ability3);
}

void ANWCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	LastMoveInput = FVector2D(V.Y, V.X); // (frente, direita) para a esquiva
	if (!Controller || V.IsNearlyZero())
	{
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
	// zoomAmount do jogo: 1 m por clique da roda; para cima aproxima.
	CameraZoom = FMath::Clamp(CameraZoom - Value.Get<float>() * 100.f, CameraZoomMin, CameraZoomMax);
}

void ANWCharacter::UpdateCamera(float DeltaSeconds)
{
	if (!IsLocallyControlled() || !CameraBoom || !Camera)
	{
		return;
	}
	const ANWPlayerController* PC = Cast<ANWPlayerController>(Controller);
	const bool bInventory = PC && PC->IsInventoryOpen();
	// Estados do gamecamera.xml: (direita, altura acima dos pés, distância, inclinação mín./máx.).
	float Right = 50.f, Height = 170.f, Distance = CameraZoom, PitchMin = -70.f, PitchMax = 60.f;
	if (bInventory)
	{
		Right = 77.f; Height = 95.f; Distance = 275.f; PitchMin = -70.f; PitchMax = -10.f;   // UI_Inventory
	}
	else if (bSprinting && GetVelocity().Size2D() > RunSpeed)
	{
		Right = 70.f; Distance = FMath::Max(Distance, 400.f);                                   // MidCameraSprint
	}
	// Transição de 0,2 s (SmoothTime do jogo).
	const float Speed = 1.f / 0.2f;
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector Loc = CameraBoom->GetRelativeLocation();
	Loc.Z = FMath::FInterpTo(Loc.Z, Height - HalfHeight, DeltaSeconds, Speed);
	CameraBoom->SetRelativeLocation(Loc);
	CameraBoom->SocketOffset.Y = FMath::FInterpTo(CameraBoom->SocketOffset.Y, Right, DeltaSeconds, Speed);
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, Distance, DeltaSeconds, Speed);
	if (APlayerCameraManager* PCM = PC ? PC->PlayerCameraManager.Get() : nullptr)
	{
		PCM->ViewPitchMin = PitchMin;
		PCM->ViewPitchMax = PitchMax;
	}
	// FOV do jogo é vertical; a Unreal usa horizontal: converte pela proporção da tela.
	FVector2D Viewport(16.f, 9.f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(Viewport);
	}
	const float Aspect = Viewport.Y > 0.f ? Viewport.X / Viewport.Y : 16.f / 9.f;
	// Campo de visão da opção do jogador; no inventário o jogo ignora a opção (UseUserDefinedFov = false).
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

void ANWCharacter::AttackPressed()
{
	if (InMenu())
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
	if (CanAct())
	{
		Combat->RequestBlock(true);
	}
}
void ANWCharacter::BlockReleased() { Combat->RequestBlock(false); }

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

bool ANWCharacter::InMenu() const
{
	const ANWPlayerController* PC = Cast<ANWPlayerController>(Controller);
	return PC && PC->IsMenuOpen();
}

bool ANWCharacter::CanAct() const
{
	return !Traversal->IsTraversing() && !GetCharacterMovement()->IsFalling();
}

void ANWCharacter::JumpPressed()
{
	if (Inventory && Inventory->IsEncumbered())
	{
		return;
	}
	if (InMenu())
	{
		return;
	}
	// Como no jogo: pula fora de ataque/esquiva; com a guarda levantada, abaixa e pula.
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

void ANWCharacter::InventoryPressed()
{
	if (ANWPlayerController* PC = Cast<ANWPlayerController>(Controller))
	{
		PC->ToggleInventory();
	}
}

void ANWCharacter::InteractPressed()
{
	Gather->InteractPressed();
}

void ANWCharacter::DevMenuPressed()
{
	if (ANWPlayerController* PC = Cast<ANWPlayerController>(Controller))
	{
		PC->ToggleDevMenu();
	}
}

// Escolha de habilidades gravada em Saved/Config/<plataforma>/GameUserSettings.ini; com nw-core, o servidor
// guarda a escolha e manda de volta ao entrar (ANWPlayerController::OnRep_Progress).

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
	if (Record.bFirstJoin)
	{
		Inventory->GiveStarterKit();
	}
	else
	{
		// Personagens de antes da F4: ferramentas de coleta que faltarem.
		Inventory->GiveTools();
	}
}

void ANWCharacter::CaptureTo(FNWCharacterRecord& Record) const
{
	// Só posição no chão: caindo (ou caído para fora do mapa) fica a última boa.
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
	TSharedRef<FJsonObject> Loadout = MakeShared<FJsonObject>();
	for (const FNWWeaponSet& Set : WeaponSets)
	{
		TArray<TSharedPtr<FJsonValue>> Names;
		for (const FName& N : Set.Abilities)
		{
			Names.Add(MakeShared<FJsonValueString>(N.ToString()));
		}
		Loadout->SetArrayField(Set.Id.ToString(), Names);
	}
	Record.Loadout = Loadout;
	Record.Items = Inventory->Items;
	Record.Coins = Inventory->Coins;
	Record.bFirstJoin = false;
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
	Sword->SetStaticMesh(Set.Mesh.LoadSynchronous());
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

	// A outra arma no quadril esquerdo, cabo para cima e lâmina para baixo e para trás.
	const int32 Other = WeaponSets.Num() > 1 ? (ActiveWeapon + 1) % WeaponSets.Num() : INDEX_NONE;
	if (Other != INDEX_NONE)
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

	if (UAnimInstance* Anim = Body->GetAnimInstance())
	{
		// Pelo nome: vale para a variável do C++ (NWAnimInstance) e para uma criada no próprio ABP.
		UBlendSpace* BS = Set.Locomotion.LoadSynchronous();
		int32 Assigned = 0;
		for (TFieldIterator<FObjectProperty> It(Anim->GetClass()); It; ++It)
		{
			if (It->GetFName() == TEXT("Locomotion") && It->PropertyClass->IsChildOf(UBlendSpace::StaticClass()))
			{
				It->SetObjectPropertyValue_InContainer(Anim, BS);
				++Assigned;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("NW locomoção %s -> %s (%d variável/is em %s)"), *Set.Id.ToString(),
			BS ? *BS->GetName() : TEXT("nenhuma"), Assigned, *Anim->GetClass()->GetName());
	}
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
		const int32 Slot = WeaponSets[Index].Id == TEXT("Hatchet") ? NWEquip::Hatchet : NWEquip::Sword;
		if (!Inventory->At(ENWItemContainer::Equip, Slot))
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

void ANWCharacter::OnRep_ActiveWeapon()
{
	ApplyWeapon();
}

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

void ANWCharacter::Ability(int32 Index)
{
	if (InMenu())
	{
		return;
	}
	if (Abilities.IsValidIndex(Index) && CanAct())
	{
		Combat->RequestAction(Abilities[Index]);
	}
}

static TAutoConsoleVariable<int32> CVarDebugCamera(TEXT("nw.DebugCamera"), 0, TEXT("Registra a câmera (pitch, braço, distância) a cada segundo."));

// Teto baixo/parede atrás: o braço da câmera encolhe até dentro do corpo. Nesse caso o próprio jogador deixa de
// ver o personagem (os outros continuam vendo), como nos jogos em terceira pessoa.
void ANWCharacter::UpdateCameraClipHide()
{
	if (!IsLocallyControlled() || !Camera)
	{
		return;
	}
	const FVector Chest = GetActorLocation() + FVector(0.f, 0.f, 40.f);
	const float Dist = FVector::Dist(Camera->GetComponentLocation(), Chest);
	const bool bHide = Dist < CameraHideDistance;
	if (CVarDebugCamera.GetValueOnGameThread() != 0 && GFrameCounter % 30 == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("NW câmera: pitch %.1f  braço %.0f/%.0f  dist %.0f  pos %s  mov %d"), GetControlRotation().Pitch,
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

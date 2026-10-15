#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "NWCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UNWCombatComponent;
class UNWTraversalComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class USkeletalMesh;
class UStaticMesh;
class UDataTable;
class UAnimInstance;
class UBlendSpace;
struct FNWCharacterRecord;

// Base locomotion replaced by another area (mount, gathering) while it applies: see SetLocomotionOverride.
USTRUCT()
struct FNWLocomotionOverride
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UBlendSpace> Space;
	UPROPERTY() int32 Priority = 0;
	UPROPERTY() bool bNoSprint = true;
};

// One player weapon (swapped with 1/2): mesh, action table, locomotion and Q/R/F abilities.
USTRUCT(BlueprintType)
struct FNWWeaponSet
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName Id;                     // "Sword", "Hatchet" (key of the loadout and of abilities.json)
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> Mesh;
	// Skinned mesh on the player skeleton (Ice Gauntlet glove; NWArmaManopla.cpp).
	UPROPERTY(EditAnywhere) TSoftObjectPtr<USkeletalMesh> SkinMesh;
	UPROPERTY(EditAnywhere) bool bUsesShield = false;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UDataTable> Actions;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UBlendSpace> Locomotion;
	UPROPERTY(EditAnywhere) FString HudIcon;              // the game's "drawing" icon
	UPROPERTY(EditAnywhere) TArray<FName> Abilities;
	// Bone of the holding hand (the Bow goes on the left).
	UPROPERTY(EditAnywhere) FName Socket = TEXT("right_hand_attach");
	// Weapon numbers (javelindata_itemdefinitions_weapons, T2). Damage on the port's scale: the game's sword 64 = 100.
	UPROPERTY(EditAnywhere) float BaseDamage = 100.f;
	UPROPERTY(EditAnywhere) float CritChance = 0.07f;
	UPROPERTY(EditAnywhere) float CritDamageMultiplier = 1.3f;
	UPROPERTY(EditAnywhere) float StaggerDamage = 36.f;
	UPROPERTY(EditAnywhere) float BlockStaminaDamage = 36.f;
	// Stability of whoever blocks with this weapon (with a shield, the shield's).
	UPROPERTY(EditAnywhere) float BlockStability = 30.f;
	// Aiming weapon (Bow): right button aims, left shoots.
	UPROPERTY(EditAnywhere) bool bAims = false;
	// Heavy charge (Attack_Start_Charge): release before MinHold = light; FullCharge = the charged attack fires by itself.
	UPROPERTY(EditAnywhere) float HeavyMinHold = 0.333f;
	UPROPERTY(EditAnywhere) float HeavyFullCharge = 0.667f;
};

/**
 * Player character (and base of the Lost and creatures). The implementation is split by area (docs/workflow.md):
 * NWCharacter.cpp (movement, camera, input, menus), NWCharacterArmas.cpp (weapons, combat, abilities, dummy),
 * NWCharacterSave.cpp (save/restore). Gathering and mount live in components (Gather, Mount); new keys through
 * NW_REGISTER_INPUT (NWInputRegistry.h), F10 commands through NW_REGISTER_DEV (NWDevRegistry.h).
 *
 * Over-the-shoulder camera, strafe with the camera (as in the game with the weapon
 * in hand) and sprint oriented to movement. Without a controller, it serves as a training dummy.
 *
 * Controls (game default, libs/config/defaultprofile.xml): WASD, mouse, Space jumps (or steps up,
 * vault, mantle), Shift dodges, left Alt runs, left button attacks (hold = heavy), right blocks,
 * Q/R/F abilities, 1/2 swap weapon, E gathers, K abilities menu, F10 dev menu. Runs forward on its own
 * (sprint); holding Ctrl walks.
 */
UCLASS()
class NWPORT_API ANWCharacter : public ACharacter
{
	GENERATED_BODY()
	friend class UNWGatherComponent;

public:
	ANWCharacter(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<UCameraComponent> Camera;
	// Camera closer than this to the chest (cm): the player stops seeing their own character.
	UPROPERTY(EditAnywhere, Category = "NW|Câmera") float CameraHideDistance = 75.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<UStaticMeshComponent> Sword;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<UStaticMeshComponent> Shield;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<UNWCombatComponent> Combat;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<UNWTraversalComponent> Traversal;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<class UNWInventoryComponent> Inventory;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<class UNWGatherComponent> Gather;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<class UNWProgressionComponent> Progression;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<class UNWManaComponent> Mana;
	// Mount (area 3, NWMountComponent).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<class UNWMountComponent> Mount;
	/** Focus and key to interact with NPC, station, Storage (area 4, NWInteractable.h). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<class UNWInteractorComponent> Interactor;
	// Combat AI (enemies). Created disabled; once enabled (UseBrain), replaces TickDummy.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<class UNWBrainComponent> Brain;
	/** Turns on the brain with the given style and attacks (server). */
	void UseBrain(uint8 Style, const TArray<struct FNWBrainAttack>& Attacks);
	// Arrow on the string while aiming with the Bow.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<UStaticMeshComponent> NockedArrow;
	// Weapon stowed on the hip (the one not in use).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<UStaticMeshComponent> Stowed;

	// Player weapons (empty = single-weapon character, like the Lost).
	UPROPERTY(EditAnywhere, Category = "NW|Armas") TArray<FNWWeaponSet> WeaponSets;
	UPROPERTY(ReplicatedUsing = OnRep_ActiveWeapon, BlueprintReadOnly, Category = "NW|Armas") int32 ActiveWeapon = 0;
	// The two weapons in use (keys 1 and 2), indices into WeaponSets; as in the game, two at a time.
	UPROPERTY(ReplicatedUsing = OnRep_ActiveWeapon, BlueprintReadOnly, Category = "NW|Armas") TArray<int32> Loadout = {0, 1};
	/** The weapon of the pair that is not in hand (INDEX_NONE with a single weapon). */
	int32 OtherWeapon() const;
	/** Server: swaps the weapon pair (dev menu) and draws the first. */
	void SetLoadout(int32 First, int32 Second);
	// Weapon swap (1/2): plays the draw of the new weapon.
	void SwitchWeapon(int32 Index);
	void SetWeaponAbilities(int32 SetIndex, const TArray<FName>& New);

	/**
	 * Replaces the ABP's base locomotion (the Locomotion pin) without touching the graph: mount, gathering. The one with the
	 * highest priority applies; with none, the one of the weapon in use. bNoSprint removes the sprint animation while it
	 * applies. Local on each machine: whoever replicates the state (e.g. mounted) calls it on the clients too.
	 */
	void SetLocomotionOverride(FName OverrideOwner, UBlendSpace* Space, int32 Priority = 0, bool bNoSprint = true);
	void ClearLocomotionOverride(FName OverrideOwner);
	bool LocomotionBlocksSprint() const;
	/** Menu open (Weapon Mastery, inventory, dev): the character does not act. */
	bool InMenu() const;

	// Character assets (the default is the sword-and-shield player; the Lost swaps them in the constructor).
	UPROPERTY(EditAnywhere, Category = "NW|Assets") TSoftObjectPtr<USkeletalMesh> MeshAsset;
	UPROPERTY(EditAnywhere, Category = "NW|Assets") TSoftClassPtr<UAnimInstance> AnimClass;
	UPROPERTY(EditAnywhere, Category = "NW|Assets") TSoftObjectPtr<UStaticMesh> SwordAsset;
	UPROPERTY(EditAnywhere, Category = "NW|Assets") TSoftObjectPtr<UStaticMesh> ShieldAsset;
	UPROPERTY(EditAnywhere, Category = "NW|Assets") TSoftObjectPtr<UDataTable> DamageTableAsset;
	UPROPERTY(EditAnywhere, Category = "NW|Assets") TSoftObjectPtr<UDataTable> ActionTableAsset;

	// Speeds of the game's locomotion animations: walk 1.65 m/s, run (combat) 4 m/s, sprint.
	UPROPERTY(EditAnywhere, Category = "NW|Movimento") float WalkSpeed = 165.f;
	UPROPERTY(EditAnywhere, Category = "NW|Movimento") float RunSpeed = 400.f;
	UPROPERTY(EditAnywhere, Category = "NW|Movimento") float SprintSpeed = 650.f;

	// Game camera (DynamicCamera): focus 1.7 m above the feet and 0.5 m to the right, 2 m behind; zoom from 0.2 to 6 m
	// (1 m per wheel click); 50° vertical FOV. Player options change the FOV (NW.Options).
	UPROPERTY(EditAnywhere, Category = "NW|Câmera") float CameraFovVertical = 50.f;
	UPROPERTY(EditAnywhere, Category = "NW|Câmera") float CameraZoom = 200.f;
	UPROPERTY(EditAnywhere, Category = "NW|Câmera") float CameraZoomMin = 20.f;
	UPROPERTY(EditAnywhere, Category = "NW|Câmera") float CameraZoomMax = 600.f;
	// Turn speed during the attack's Homing window (degrees/s).
	UPROPERTY(EditAnywhere, Category = "NW|Movimento") float HomingTurnRate = 540.f;
	// Abilities on Q, R and F (row names of the action table).
	UPROPERTY(EditAnywhere, Category = "NW|Combate") TArray<FName> Abilities = {TEXT("ShieldBash"), TEXT("SpinAttack"), TEXT("SwipeStab")};
	// Swaps the Q/R/F abilities (abilities menu) and stores the choice for the next session.
	void SetAbilities(const TArray<FName>& New);
	// Re-reads the Q/R/F choices from GameUserSettings.ini.
	void LoadAbilityLoadout();

	// Server with nw-core: applies saved weapon, health and abilities / copies the current state for saving.
	void RestoreFrom(const FNWCharacterRecord& Record);
	void CaptureTo(FNWCharacterRecord& Record) const;

	// Automatic sprint when going forward (as in the game); holding Ctrl walks.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW") bool bSprinting = false;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW") bool bWalking = false;
	// Dev menu: multiplies all speeds (run faster to cross the map).
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW") float DevSpeedMultiplier = 1.f;
	// Dev menu: free flight, passes through the scenery. Forward follows the camera (with pitch); Space goes up, Ctrl down.
	UPROPERTY(ReplicatedUsing = OnRep_DevFlying, BlueprintReadOnly, Category = "NW") bool bDevFlying = false;
	void ApplyDevFlying();
	/** Game camera shake (javelindata_camerashake, DT_NWCameraShake) on this character's camera, if local. */
	void PlayCameraShake(FName Id);
	// Short jolt on the camera (hit dealt/received): 1 = ~5 cm for ~0.25 s.
	void AddCameraKick(float Amount) { CameraKick = FMath::Max(CameraKick, Amount); CameraKickTime = 0.f; }

	// Training dummy (no player): keeps its shield raised.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Boneco") bool bDummyBlocks = false;
	// Training dummy: attacks the nearest player within range, one combo per interval.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Boneco") bool bDummyAttacks = false;
	/** Simulated player of the load test (ANWLoadTest): AI controller, but creature spawning and AI treat it as a
	 *  player. Use IsPlayerLike() where the rule is "is it a player?". */
	UPROPERTY(Transient) bool bSimPlayer = false;
	bool IsPlayerLike() const { return bSimPlayer || IsPlayerControlled(); }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Boneco") float DummyAttackInterval = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Boneco") float DummyAttackRange = 260.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Boneco") int32 DummyComboHits = 3;
	// Enemy: chases the player within this radius (0 = stays still) and picks an attack from the list.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Boneco") float AggroRange = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Boneco") TArray<FName> DummyAttackActions;

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	/** Rebuilds the mapping with the keys from the options (Shortcuts tab). */
	void RebuildKeys();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;
	// Assets imported after the editor opens do not come in through the constructor; loads whatever is missing here.
	void LoadMissingAssets();

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void AttackPressed();
	void AttackReleased();
	void BlockPressed();
	void BlockReleased();
	void Dodge();
	void SkillMenuPressed();
	void DevMenuPressed();
	void UpdateCameraClipHide();
	/** Camera with the game's states (libs/camera/gamecamera.xml, docs/game/ui/original.md): normal, sprint, inventory. */
	void UpdateCamera(float DeltaSeconds);
	void Zoom(const FInputActionValue& Value);
	bool bHiddenForCamera = false;
	void JumpPressed();
	void JumpReleased();
	// Jump, traversal or combat action in progress: accepts no other action.
	bool CanAct() const;
	void WalkPressed();
	void WalkReleased();
	/** Owner: sprints when the requested movement goes toward the camera's front and nothing prevents it. */
	void UpdateAutoSprint();
	void Ability(int32 Index);
	void Ability1() { Ability(0); }
	void Ability2() { Ability(1); }
	void Ability3() { Ability(2); }

	UFUNCTION(Server, Reliable) void ServerSetSprinting(bool bNew);
	UFUNCTION(Server, Reliable) void ServerSetWalking(bool bNew);
	UFUNCTION(Server, Reliable) void ServerSwitchWeapon(int32 Index);
	UFUNCTION(Server, Reliable) void ServerSetWeaponAbilities(int32 SetIndex, const TArray<FName>& New);
	UFUNCTION() void OnRep_ActiveWeapon();
	UFUNCTION() void OnRep_DevFlying() { ApplyDevFlying(); }
	float CameraKick = 0.f;
	float CameraKickTime = 0.f;
	// Shake in progress (row parameters, time, current and previous target, next target change).
	struct FShake
	{
		bool bActive = false;
		FVector Shift = FVector::ZeroVector, Angle = FVector::ZeroVector;
		float FadeIn = 0.f, Sustain = 0.f, FadeOut = 0.f, Period = 0.03f, Randomness = 0.f, Time = 0.f, NextTarget = 0.f;
		float Flip = 1.f;
		FVector FromShift = FVector::ZeroVector, ToShift = FVector::ZeroVector, FromAngle = FVector::ZeroVector, ToAngle = FVector::ZeroVector;
	} ActiveShake;
	UPROPERTY() TObjectPtr<UDataTable> ShakeTable;
	bool bFlyUp = false;
	void ApplyWeapon();
	/** Fills WeaponSets with the player's weapons (NWCharacterArmas.cpp). */
	void AddPlayerWeaponSets();
	/** Puts into the ABP the locomotion that applies now (highest-priority override or the weapon's). */
	void RefreshLocomotion();
	UPROPERTY() TMap<FName, FNWLocomotionOverride> LocomotionOverrides;
	void Weapon1() { if (Loadout.IsValidIndex(0)) SwitchWeapon(Loadout[0]); }
	void Weapon2() { if (Loadout.IsValidIndex(1)) SwitchWeapon(Loadout[1]); }
	void ApplySprint();

private:
	void BuildInput();
	void MapKeys();
	/** Auto-run (the game's = key): goes toward the camera's front until forward/back is pressed. */
	void AutoRunPressed() { bAutoRun = !bAutoRun; }
	bool bAutoRun = false;
	UPROPERTY() TObjectPtr<UInputAction> IA_AutoRun;
	UPROPERTY() TObjectPtr<UInputAction> IA_Map;
	void MapPressed();

	UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY() TObjectPtr<UInputAction> IA_Move;
	UPROPERTY() TObjectPtr<UInputAction> IA_Look;
	UPROPERTY() TObjectPtr<UInputAction> IA_Zoom;
	UPROPERTY() TObjectPtr<UInputAction> IA_Attack;
	UPROPERTY() TObjectPtr<UInputAction> IA_Block;
	UPROPERTY() TObjectPtr<UInputAction> IA_Dodge;
	UPROPERTY() TObjectPtr<UInputAction> IA_Walk;
	UPROPERTY() TObjectPtr<UInputAction> IA_Jump;
	UPROPERTY() TObjectPtr<UInputAction> IA_SkillMenu;
	UPROPERTY() TObjectPtr<UInputAction> IA_DevMenu;
	UPROPERTY() TObjectPtr<UInputAction> IA_Weapon1;
	UPROPERTY() TObjectPtr<UInputAction> IA_Weapon2;
	UPROPERTY() TObjectPtr<UInputAction> IA_Potion;
	TObjectPtr<UInputAction> IA_Inventory;
	TObjectPtr<UInputAction> IA_Interact;
	void UsePotion();
	void InventoryPressed();
	void InteractPressed();
	UPROPERTY() TArray<TObjectPtr<UInputAction>> IA_Abilities;
	// Keys of the areas (FNWInputRegistry), in the same order as the registry.
	UPROPERTY() TArray<TObjectPtr<UInputAction>> IA_Registered;

	void TickDummy(float DeltaSeconds);

	FVector2D LastMoveInput = FVector2D::ZeroVector;
	float DummyTimer = 0.f;
	int32 DummyHitsLeft = 0;
	bool bAttackFromSprint = false;
	// The attack press was not a menu click: only then does the release go to the server. Releasing without charge
	// becomes a light hit on the server (ServerAttackInput); a menu click cannot become an attack on release.
	bool bAttackPressAccepted = false;
};

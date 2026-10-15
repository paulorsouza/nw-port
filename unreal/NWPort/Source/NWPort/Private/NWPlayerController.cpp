#include "NWPlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "NWDevRegistry.h"
#include "NWCreature.h"
#include "NWBrainComponent.h"
#include "NWBehaviorTreeComponent.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NWProgressionComponent.h"
#include "NWSpawnField.h"
#include "NWText.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NWCharacter.h"
#include "NWCombatComponent.h"
#include "NWLost.h"
#include "SNWDevMenu.h"
#include "NWHUD.h"
#include "NWAmbienceComponent.h"
#include "NWGatherComponent.h"
#include "NWOptions.h"
#include "NWGatherField.h"
#include "NWInventoryComponent.h"
#include "NWGameMode.h"
#include "NWOps.h"
#include "NWBackend.h"
#include "Engine/GameInstance.h"
#include "Misc/ScopeExit.h"
#include "GameFramework/PlayerState.h"
#include "Dom/JsonObject.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Widgets/SWeakWidget.h"

static const FName DevTag(TEXT("NWDev"));

ANWHUD* ANWPlayerController::GetNWHUD() const
{
	return Cast<ANWHUD>(GetHUD());
}

bool ANWPlayerController::IsInventoryOpen() const
{
	const ANWHUD* HUD = Cast<ANWHUD>(GetHUD());
	return HUD && HUD->Inventory.IsOpen();
}

ANWPlayerController::ANWPlayerController()
{
	Ambience = CreateDefaultSubobject<UNWAmbienceComponent>(TEXT("Ambience"));
}

// F10 options remembered between sessions in development (GameUserSettings.ini, [NW.Dev]); Shipping neither writes nor reads.
namespace NWDevPrefs
{
	static const TCHAR* Section = TEXT("NW.Dev");
	static const TCHAR* CVars[] = {TEXT("nw.DebugHits"), TEXT("nw.DebugTraversal"), TEXT("nw.HudDebug")};
	static void Set(const TCHAR* Key, bool bValue)
	{
#if !UE_BUILD_SHIPPING
		GConfig->SetBool(Section, Key, bValue, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
#endif
	}
	static bool Get(const TCHAR* Key)
	{
		bool b = false;
#if !UE_BUILD_SHIPPING
		GConfig->GetBool(Section, Key, b, GGameUserSettingsIni);
#endif
		return b;
	}
}

void ANWPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
#if !UE_BUILD_SHIPPING
	if (ANWCharacter* Me = Cast<ANWCharacter>(InPawn))
	{
		Me->Combat->bGodMode = NWDevPrefs::Get(TEXT("GodMode"));
		Me->Combat->bNoCooldowns = NWDevPrefs::Get(TEXT("NoCooldowns"));
		Me->Combat->bInfiniteStamina = NWDevPrefs::Get(TEXT("InfiniteStamina"));
		Me->DevSpeedMultiplier = NWDevPrefs::Get(TEXT("FastRun")) ? 4.f : 1.f;
	}
#endif
}

void ANWPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		FNWOptions::Get().Apply(GetWorld());
#if !UE_BUILD_SHIPPING
		for (const TCHAR* Name : NWDevPrefs::CVars)
		{
			if (NWDevPrefs::Get(Name))
			{
				if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name))
				{
					V->Set(1, ECVF_SetByConsole);
				}
			}
		}
#endif
	}
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(BreadcrumbTimer, this, &ANWPlayerController::RecordBreadcrumb, 1.f, true);
		UpdateDevAllowed();
	}
}

void ANWPlayerController::UpdateDevAllowed()
{
	// Without nw-core (nothing is saved) or editor Play: allowed. With nw-core: admin account only.
	const UGameInstance* GI = GetGameInstance();
	const UNWBackend* B = GI ? GI->GetSubsystem<UNWBackend>() : nullptr;
	const bool bBackend = B && B->IsServerEnabled();
	bDevAllowed = !bBackend || GetWorld()->WorldType == EWorldType::PIE || (Record && Record->bAdmin);
}

void ANWPlayerController::ClientWarning_Implementation(const FString& Text)
{
	if (ANWHUD* HUD = GetNWHUD())
	{
		HUD->ShowWarning(NWText::Resolve(Text));
	}
}

void ANWPlayerController::RecordBreadcrumb()
{
	const ACharacter* Me = Cast<ACharacter>(GetPawn());
	if (!Me || !Me->GetCharacterMovement() || !Me->GetCharacterMovement()->IsMovingOnGround())
	{
		return;
	}
	const FVector L = Me->GetActorLocation();
	if (Breadcrumbs.Num() == 0 || FVector::DistSquared(Breadcrumbs.Last(), L) > FMath::Square(300.f))
	{
		Breadcrumbs.Add(L);
		if (Breadcrumbs.Num() > 20)
		{
			Breadcrumbs.RemoveAt(0);
		}
	}
}

void ANWPlayerController::ServerUnstuck_Implementation()
{
	const ANWCharacter* Me = Cast<ANWCharacter>(GetPawn());
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Me || Me->Combat->State == ENWCombatState::Dead)
	{
		Warning(NWText::Key(TEXT("ui_unstuck_request_denied_player_dead")));
		return;
	}
	if (GetWorldTimerManager().IsTimerActive(UnstuckTimer))
	{
		Warning(NWText::Key(TEXT("ui_unstuck_request_denied_request_already_active")));
		return;
	}
	if (Now < UnstuckReadyAt)
	{
		Warning(NWText::Key(TEXT("ui_unstuck_request_denied_on_cooldown"), {FString::FromInt(FMath::CeilToInt(UnstuckReadyAt - Now))}));
		return;
	}
	if (Breadcrumbs.Num() == 0)
	{
		Warning(NWText::Key(TEXT("ui_unstuck_request_denied_no_breadcrumbs")));
		return;
	}
	SystemMessage(NWText::Key(TEXT("ui_unstuck_request_accepted"), {TEXT("5")}));
	GetWorldTimerManager().SetTimer(UnstuckTimer, this, &ANWPlayerController::FinishUnstuck, 5.f, false);
}

void ANWPlayerController::FinishUnstuck()
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetPawn());
	if (!Me || Breadcrumbs.Num() == 0)
	{
		return;
	}
	// The last breadcrumb may be the place where it got stuck: uses the previous one when there is one.
	const FVector To = Breadcrumbs[FMath::Max(0, Breadcrumbs.Num() - 2)] + FVector(0.f, 0.f, 50.f);
	if (!Me->TeleportTo(To, Me->GetActorRotation()))
	{
		Warning(NWText::Key(TEXT("ui_unstuck_request_denied_teleport_failed")));
		return;
	}
	Breadcrumbs.SetNum(FMath::Max(0, Breadcrumbs.Num() - 1));
	UnstuckReadyAt = GetWorld()->GetTimeSeconds() + 60.0;
}

void ANWPlayerController::OnEscape()
{
	ANWHUD* HUD = GetNWHUD();
	if (!HUD || HUD->Login.IsActive() || HUD->Chat.IsOpen())
	{
		return;
	}
	if (IsMenuOpen())
	{
		CloseMenus();
		return;
	}
	HUD->GameMenu.Open();
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	EnterMenuInput();
}

bool ANWPlayerController::IsMenuOpen() const
{
	const ANWHUD* HUD = GetNWHUD();
	return OpenMenu.IsValid() || (HUD && (HUD->Mastery.IsOpen() || HUD->Tradeskills.IsOpen() || HUD->Attributes.IsOpen() || HUD->Login.IsActive() || HUD->Chat.IsOpen() || HUD->Inventory.IsOpen()
		|| HUD->GameMenu.IsOpen() || HUD->WorldMap.IsOpen() || HUD->IsStationMenuOpen()));
}

void ANWPlayerController::EnterCanvasMenu()
{
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	EnterMenuInput();
}

void ANWPlayerController::EnterMenuInput()
{
	SetShowMouseCursor(true);
	SetIgnoreLookInput(true);
	SetIgnoreMoveInput(true);
}

void ANWPlayerController::LeaveMenuInput()
{
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
	ResetIgnoreLookInput();
	ResetIgnoreMoveInput();
}

void ANWPlayerController::ShowMenu(TSharedRef<SWidget> Menu, TSharedRef<SWidget> Focus)
{
	CloseMenus();
	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		return;
	}
	OpenMenu = Menu;
	OpenMenuHost = SNew(SWeakWidget).PossiblyNullContent(Menu);
	Viewport->AddViewportWidgetContent(OpenMenuHost.ToSharedRef(), 50);
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(Focus);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	EnterMenuInput();
}

void ANWPlayerController::CloseMenus()
{
	if (const ANWHUD* HUD = GetNWHUD(); HUD && HUD->Login.IsActive())
	{
		return;
	}
	const bool bWasOpen = IsMenuOpen();
	if (OpenMenuHost.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(OpenMenuHost.ToSharedRef());
	}
	OpenMenu.Reset();
	OpenMenuHost.Reset();
	if (ANWHUD* HUD = GetNWHUD())
	{
		HUD->Mastery.Close();
		HUD->Tradeskills.Close();
		HUD->Attributes.Close();
		HUD->Inventory.Close();
		HUD->GameMenu.Close();
		HUD->WorldMap.Close();
		HUD->CloseStationMenus();
	}
	if (bWasOpen)
	{
		LeaveMenuInput();
	}
}

void ANWPlayerController::ToggleSkillMenu()
{
	ANWHUD* HUD = GetNWHUD();
	if (!HUD || HUD->Login.IsActive())
	{
		return;
	}
	if (HUD->Mastery.IsOpen() || HUD->Tradeskills.IsOpen() || HUD->Attributes.IsOpen())
	{
		CloseMenus();
		return;
	}
	CloseMenus();
	HUD->Mastery.Open(Cast<ANWCharacter>(GetPawn()));
	if (HUD->Mastery.IsOpen())
	{
		// No Slate widget: clicks arrive through the controller's InputComponent (OnMenuClick).
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		EnterMenuInput();
	}
}

void ANWPlayerController::ToggleInventory()
{
	ANWHUD* HUD = GetNWHUD();
	if (!HUD || HUD->Login.IsActive() || HUD->IsDeathScreen())
	{
		return;
	}
	if (HUD->Inventory.IsOpen())
	{
		CloseMenus();
		return;
	}
	CloseMenus();
	HUD->Inventory.Open(Cast<ANWCharacter>(GetPawn()));
	if (HUD->Inventory.IsOpen())
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		EnterMenuInput();
	}
}

void ANWPlayerController::ToggleMap()
{
	ANWHUD* HUD = GetNWHUD();
	if (!HUD || HUD->Login.IsActive() || HUD->IsDeathScreen())
	{
		return;
	}
	if (HUD->WorldMap.IsOpen())
	{
		CloseMenus();
		return;
	}
	CloseMenus();
	HUD->WorldMap.Open(GetPawn());
	if (HUD->WorldMap.IsOpen())
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		EnterMenuInput();
	}
}

void ANWPlayerController::OnWheelUp()
{
	ANWHUD* HUD = GetNWHUD();
	float X = 0.f, Y = 0.f;
	if (HUD && HUD->StationMenuWheel(1.f))
	{
		return;
	}
	if (HUD && HUD->WorldMap.IsOpen() && GetMousePosition(X, Y))
	{
		HUD->WorldMap.Wheel(1.f, FVector2D(X, Y));
	}
}

void ANWPlayerController::OnWheelDown()
{
	ANWHUD* HUD = GetNWHUD();
	float X = 0.f, Y = 0.f;
	if (HUD && HUD->StationMenuWheel(-1.f))
	{
		return;
	}
	if (HUD && HUD->WorldMap.IsOpen() && GetMousePosition(X, Y))
	{
		HUD->WorldMap.Wheel(-1.f, FVector2D(X, Y));
	}
}

void ANWPlayerController::OnMenuRightClick()
{
	ANWHUD* HUD = GetNWHUD();
	if (HUD && HUD->Inventory.IsOpen())
	{
		float X = 0.f, Y = 0.f;
		GetMousePosition(X, Y);
		HUD->Inventory.Click(FVector2D(X, Y), true);
	}
	else if (HUD && HUD->WorldMap.IsOpen())
	{
		float X = 0.f, Y = 0.f;
		GetMousePosition(X, Y);
		HUD->WorldMap.RightClick(FVector2D(X, Y));
	}
}

void ANWPlayerController::ToggleDevMenu()
{
	if (!bDevAllowed && !OpenMenu.IsValid())
	{
		return;   // account without admin: the server would refuse the commands
	}
	if (const ANWHUD* HUD = GetNWHUD(); HUD && HUD->Login.IsActive())
	{
		return;
	}
	if (OpenMenu.IsValid())
	{
		CloseMenus();
		return;
	}
	TSharedRef<SNWDevMenu> Menu = SNew(SNWDevMenu).Owner(this);
	ShowMenu(Menu, Menu);
}

bool ANWPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	ANWHUD* HUD = GetNWHUD();
	// Only the "press" becomes the new key; release and axes stay normal (otherwise the release of the click that opened the
	// wait never arrives and the button stays stuck as pressed).
	if (HUD && HUD->GameMenu.IsWaitingKey() && Params.Event == IE_Pressed
		&& !Params.Key.IsAxis1D() && !Params.Key.IsAxis2D() && !Params.Key.IsGamepadKey())
	{
		if (Params.Key == EKeys::Escape)
		{
			HUD->GameMenu.CancelKey();
		}
		else
		{
			HUD->GameMenu.AssignKey(Params.Key);
		}
		ApplyKeyChanges();
		return true;
	}
	return Super::InputKey(Params);
}

void ANWPlayerController::ApplyKeyChanges()
{
	ANWHUD* HUD = GetNWHUD();
	if (HUD && HUD->GameMenu.ConsumeKeysChanged())
	{
		if (ANWCharacter* Me = Cast<ANWCharacter>(GetPawn()))
		{
			Me->RebuildKeys();
		}
	}
}

void ANWPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// Without consuming: with the menu closed the click goes on to the character's attack.
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ANWPlayerController::OnMenuClick).bConsumeInput = false;
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ANWPlayerController::OnEscape).bConsumeInput = false;
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ANWPlayerController::OpenChat).bConsumeInput = false;
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ANWPlayerController::OnMenuRightClick).bConsumeInput = false;
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ANWPlayerController::OnWheelUp).bConsumeInput = false;
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ANWPlayerController::OnWheelDown).bConsumeInput = false;
}

void ANWPlayerController::OnMenuClick()
{
	ANWHUD* HUD = GetNWHUD();
	if (IsMenuOpen() || (HUD && HUD->IsDeathScreen()))
	{
		MenuClickFrame = GFrameCounter;
	}
	float X = 0.f, Y = 0.f;
	GetMousePosition(X, Y);
	if (HUD && HUD->Login.IsActive())
	{
		HUD->Login.Click(FVector2D(X, Y));
		return;
	}
	if (HUD && HUD->StationMenuClick(FVector2D(X, Y)))
	{
		return;   // area 4 screens (refining, ...): NWHUD
	}
	if (HUD && HUD->GameMenu.IsOpen())
	{
		const FString LangBefore = NWText::Lang();
		switch (HUD->GameMenu.Click(FVector2D(X, Y)))
		{
		case FNWGameMenu::EAction::Resume: CloseMenus(); break;
		case FNWGameMenu::EAction::Unstuck: CloseMenus(); ServerUnstuck(); break;
		case FNWGameMenu::EAction::ExitToMenu: CloseMenus(); ConsoleCommand(TEXT("disconnect")); break;
		case FNWGameMenu::EAction::ExitToDesktop: ConsoleCommand(TEXT("quit")); break;
		default: break;
		}
		FNWOptions::Get().Apply(GetWorld());
		ApplyKeyChanges();
		if (NWText::Lang() != LangBefore)
		{
			HUD->ReloadLanguage();
		}
		return;
	}
	// Top navigation bar (inventory, character, map): switches screen.
	if (HUD && (HUD->Inventory.IsOpen() || HUD->Mastery.IsOpen() || HUD->Tradeskills.IsOpen() || HUD->Attributes.IsOpen() || HUD->WorldMap.IsOpen()))
	{
		const int32 Tab = HUD->NavTabAt(FVector2D(X, Y));
		const int32 Current = HUD->Inventory.IsOpen() ? 0 : HUD->WorldMap.IsOpen() ? 2 : 1;
		if (Tab != INDEX_NONE && Tab != Current)
		{
			Tab == 0 ? ToggleInventory() : Tab == 1 ? ToggleSkillMenu() : ToggleMap();
			return;
		}
	}
	if (HUD && HUD->Inventory.IsOpen())
	{
		HUD->Inventory.Click(FVector2D(X, Y), false);
		return;
	}
	if (HUD && HUD->WorldMap.IsOpen())
	{
		HUD->WorldMap.Click(FVector2D(X, Y));
		return;
	}
	if (HUD && HUD->IsDeathScreen())
	{
		const ANWCharacter* Me = Cast<ANWCharacter>(GetPawn());
		if (Me && HUD->Respawn.Click(FVector2D(X, Y), *Me->Combat))
		{
			Me->Combat->ServerRequestRespawn();
		}
		return;
	}
	// K window tabs: Weapon Mastery / Trade Skills / Attributes.
	if (HUD && (HUD->Mastery.IsOpen() || HUD->Tradeskills.IsOpen() || HUD->Attributes.IsOpen()))
	{
		const int32 Tab = FNWTradeskillMenu::TabAt(FVector2D(X, Y));
		const int32 Current = HUD->Mastery.IsOpen() ? 0 : HUD->Tradeskills.IsOpen() ? 1 : 2;
		if (Tab != INDEX_NONE && Tab != Current)
		{
			HUD->Mastery.Close();
			HUD->Tradeskills.Close();
			HUD->Attributes.Close();
			ANWCharacter* Me = Cast<ANWCharacter>(GetPawn());
			Tab == 0 ? HUD->Mastery.Open(Me) : Tab == 1 ? HUD->Tradeskills.Open(Me) : HUD->Attributes.Open(Me);
			return;
		}
		if (HUD->Attributes.IsOpen())
		{
			HUD->Attributes.Click(FVector2D(X, Y));
			return;
		}
	}
	if (!HUD || !HUD->Mastery.IsOpen())
	{
		return;
	}
	bool bClose = false;
	HUD->Mastery.Click(FVector2D(X, Y), bClose);
	if (bClose)
	{
		CloseMenus();
	}
}

void ANWPlayerController::PawnLeavingGame()
{
	if (HasAuthority())
	{
		if (ANWGameMode* GM = GetWorld()->GetAuthGameMode<ANWGameMode>())
		{
			GM->SavePlayer(this, TEXT("leave"));
		}
	}
	Super::PawnLeavingGame();
}

void ANWPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	CloseMenus();
	Super::EndPlay(Reason);
}

void ANWPlayerController::LocalConsole(const FString& Command)
{
	ConsoleCommand(Command, true);
}

namespace
{
	/** F10 refused (account without admin) or logged; after use, saves with the reason "dev:<command>". */
	bool DevGate(ANWPlayerController* PC, const FString& Name)
	{
		const FString Who = PC->Record ? PC->Record->AccountName + TEXT("/") + PC->Record->Name : PC->GetName();
		if (!PC->bDevAllowed)
		{
			UE_LOG(LogTemp, Warning, TEXT("F10 refused: %s asked for %s"), *Who, *Name);
			return false;
		}
		UE_LOG(LogTemp, Display, TEXT("F10: %s used %s"), *Who, *Name);
		return true;
	}

	void DevSave(ANWPlayerController* PC, const FString& Name)
	{
		if (ANWGameMode* GM = PC->GetWorld() ? PC->GetWorld()->GetAuthGameMode<ANWGameMode>() : nullptr)
		{
			GM->SavePlayer(PC, TEXT("dev:") + Name);
		}
	}
}

void ANWPlayerController::ServerDevNamed_Implementation(FName Id)
{
	const FString Name = Id.ToString();
	if (!DevGate(this, Name))
	{
		return;
	}
	const NWOps::FReason Reason(TEXT("dev:") + Name);   // whatever F10 gives becomes an operation with this reason
	if (const FNWDevEntry* E = FNWDevRegistry::Find(Id); E && E->Server)
	{
		E->Server(this);
	}
	DevSave(this, Name);
}

void ANWPlayerController::ServerDev_Implementation(ENWDevCommand Command)
{
	const FString DevName = StaticEnum<ENWDevCommand>()->GetNameStringByValue(int64(Command));
	if (!DevGate(this, DevName))
	{
		return;
	}
	ON_SCOPE_EXIT { DevSave(this, DevName); };
	const NWOps::FReason Reason(TEXT("dev:") + DevName);
	ANWCharacter* Me = Cast<ANWCharacter>(GetPawn());
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Summon: 5 m ahead, facing the player.
	auto Spawn = [&](TSubclassOf<ANWCharacter> Class, TFunction<void(ANWCharacter*)> Setup)
	{
		if (!Me)
		{
			return;
		}
		const FVector Fwd = FRotator(0.f, GetControlRotation().Yaw, 0.f).Vector();
		const FTransform Where(FRotator(0.f, (-Fwd).Rotation().Yaw, 0.f), Me->GetActorLocation() + Fwd * 500.f + FVector(0.f, 0.f, 50.f));
		ANWCharacter* New = World->SpawnActorDeferred<ANWCharacter>(Class, Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (New)
		{
			New->Tags.Add(DevTag);
			if (Setup)
			{
				Setup(New);
			}
			New->FinishSpawning(Where);
		}
	};
	// Arena: creature from the game's catalog (level health and damage) or character, around the player at ~14 m.
	auto Around = [&](int32 Index, int32 Count) -> FTransform
	{
		const float Yaw = GetControlRotation().Yaw + (Count > 1 ? (Index - (Count - 1) * 0.5f) * 35.f : 0.f);
		const FVector At = Me->GetActorLocation() + FRotator(0.f, Yaw, 0.f).Vector() * 1400.f + FVector(0.f, 0.f, 80.f);
		return FTransform(FRotator(0.f, Yaw + 180.f, 0.f), At);
	};
	auto SpawnCreature = [&](FName Id, const FTransform& Where)
	{
		const FNWCreatureDef* D = ANWCreature::FindDef(Id);
		ANWCreature* C = D ? World->SpawnActorDeferred<ANWCreature>(ANWCreature::StaticClass(), Where, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn) : nullptr;
		if (!C)
		{
			return;
		}
		C->Tags.Add(DevTag);
		C->Setup(*D, {}, {});
		C->AggroRange = 3000.f;
		C->FinishSpawning(Where);
		// Arena wave: enters combat right away with whoever called it (the game's sight is 12 m; the wave spawns at 14 m).
		if (C->Behavior && C->Behavior->IsActive())
		{
			C->Behavior->Aggro(Me);
		}
	};
	// Player character with another weapon (Bow = 2, Rapier = 3 in WeaponSets) and its fighting style.
	auto SpawnArmed = [&](const FTransform& Where, int32 Weapon, ENWBrainStyle Style, TArray<FNWBrainAttack> Attacks, float Block, float Dodge)
	{
		ANWCharacter* New = World->SpawnActorDeferred<ANWCharacter>(ANWCharacter::StaticClass(), Where, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!New || !New->WeaponSets.IsValidIndex(Weapon))
		{
			if (New) New->Destroy();
			return;
		}
		New->Tags.Add(DevTag);
		New->AggroRange = 4000.f;
		New->Loadout = {Weapon, Weapon == 2 ? 3 : 2};
		New->ActiveWeapon = Weapon;
		New->Combat->MaxHealth = 1200.f;
		New->UseBrain(static_cast<uint8>(Style), Attacks);
		New->Brain->BlockChance = Block;
		New->Brain->DodgeChance = Dodge;
		New->FinishSpawning(Where);
		New->SetLoadout(Weapon, Weapon == 2 ? 3 : 2);
		New->Combat->WeaponBaseDamage *= 0.7f;
	};
	auto Atk2 = [](const TCHAR* A, float Min, float Max, float W, float Cd = 0.f, int32 Chain = 1, bool bGap = false)
	{
		FNWBrainAttack R; R.Action = A; R.MinRange = Min; R.MaxRange = Max; R.Weight = W; R.Cooldown = Cd; R.Chain = Chain; R.bGapCloser = bGap;
		return R;
	};
	auto SpawnDuelist = [&](const FTransform& Where)
	{
		ANWCharacter* New = World->SpawnActorDeferred<ANWCharacter>(ANWCharacter::StaticClass(), Where, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!New)
		{
			return;
		}
		New->Tags.Add(DevTag);
		New->AggroRange = 3000.f;
		New->Combat->MaxHealth = 1500.f;
		New->Combat->WeaponBaseDamage = 70.f;
		auto Atk = [](const TCHAR* A, float Min, float Max, float W, float Cd = 0.f, int32 Chain = 1, bool bGap = false)
		{
			FNWBrainAttack R; R.Action = A; R.MinRange = Min; R.MaxRange = Max; R.Weight = W; R.Cooldown = Cd; R.Chain = Chain; R.bGapCloser = bGap;
			return R;
		};
		// Sword and shield like the player: 1 to 3 combo, heavy, shield bash; raises the guard and dodges.
		New->UseBrain(static_cast<uint8>(ENWBrainStyle::Duelist), {Atk(TEXT("Light1"), 0.f, 260.f, 2.f, 0.f, 3), Atk(TEXT("Light1"), 0.f, 260.f, 1.f, 0.f, 1),
			Atk(TEXT("Heavy"), 0.f, 260.f, 1.f, 2.5f), Atk(TEXT("ShieldBash"), 0.f, 220.f, 0.6f, 9.f), Atk(TEXT("SprintAttack"), 400.f, 800.f, 0.6f, 6.f, 1, true)});
		New->Brain->BlockChance = 0.35f;
		New->Brain->DodgeChance = 0.2f;
		New->Brain->HoldDistance = 360.f;
		New->FinishSpawning(Where);
	};
	switch (Command)
	{
	case ENWDevCommand::CycleLoadout:
		if (Me)
		{
			static const int32 Pairs[][2] = {{0, 1}, {2, 3}, {2, 0}, {3, 1}};
			int32 Cur = 0;
			for (int32 i = 0; i < UE_ARRAY_COUNT(Pairs); ++i)
			{
				if (Me->Loadout.Num() == 2 && Me->Loadout[0] == Pairs[i][0] && Me->Loadout[1] == Pairs[i][1]) Cur = i;
			}
			for (int32 Step = 1; Step <= UE_ARRAY_COUNT(Pairs); ++Step)
			{
				const int32* P = Pairs[(Cur + Step) % UE_ARRAY_COUNT(Pairs)];
				if (Me->WeaponSets.IsValidIndex(P[0]) && Me->WeaponSets.IsValidIndex(P[1]))
				{
					Me->SetLoadout(P[0], P[1]);
					break;
				}
			}
		}
		break;
	case ENWDevCommand::ArenaBoars:
		if (Me) for (int32 i = 0; i < 2; ++i) SpawnCreature(TEXT("Boar_6"), Around(i, 2));
		break;
	case ENWDevCommand::ArenaWolves:
		if (Me) for (int32 i = 0; i < 3; ++i) SpawnCreature(i == 1 ? FName(TEXT("Wolf_Timber_8")) : FName(TEXT("Wolf_Grey_8")), Around(i, 3));
		break;
	case ENWDevCommand::ArenaLost:
		if (Me) for (int32 i = 0; i < 2; ++i) { const FTransform W = Around(i, 2); ANWCharacter* L = World->SpawnActorDeferred<ANWCharacter>(ANWLost::StaticClass(), W, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn); if (L) { L->Tags.Add(DevTag); L->AggroRange = 3000.f; L->FinishSpawning(W); } }
		break;
	case ENWDevCommand::ArenaArcher:
		// Archer: stays at ~14 m, backs off if you get close, quick shot, Rapid Shot and Penetrating Shot; dodges.
		if (Me) SpawnArmed(Around(0, 1), 2, ENWBrainStyle::Archer, {Atk2(TEXT("Light1"), 300.f, 3000.f, 3.f, 0.8f),
			Atk2(TEXT("RapidShot"), 500.f, 3000.f, 1.f, 14.f), Atk2(TEXT("PenetratingShot"), 800.f, 4000.f, 1.f, 18.f),
			Atk2(TEXT("EvadeShot"), 0.f, 450.f, 2.f, 15.f)}, 0.f, 0.35f);
		break;
	case ENWDevCommand::ArenaRapier:
		// Rapier duelist: combos, Tondo (bleeds), Flourish and Finish, lunge from afar; quick dodge, guard.
		if (Me) SpawnArmed(Around(0, 1), 3, ENWBrainStyle::Duelist, {Atk2(TEXT("Light1"), 0.f, 250.f, 3.f, 0.f, 3),
			Atk2(TEXT("Tondo"), 0.f, 450.f, 1.f, 6.f), Atk2(TEXT("Flourish"), 0.f, 300.f, 1.f, 20.f), Atk2(TEXT("Light1"), 0.f, 250.f, 1.f, 0.f, 2),
			Atk2(TEXT("Fleche"), 500.f, 1000.f, 1.f, 20.f, 1, true)}, 0.2f, 0.4f);
		break;
	case ENWDevCommand::ArenaDuelist:
		if (Me) SpawnDuelist(Around(0, 1));
		break;
	case ENWDevCommand::ArenaMixed:
		if (Me)
		{
			SpawnCreature(TEXT("Wolf_Grey_8"), Around(0, 4));
			SpawnCreature(TEXT("Wolf_Grey_8"), Around(1, 4));
			SpawnCreature(TEXT("Boar_6"), Around(2, 4));
			SpawnDuelist(Around(3, 4));
		}
		break;
	case ENWDevCommand::ToggleNoCooldowns:
		if (Me) { Me->Combat->bNoCooldowns = !Me->Combat->bNoCooldowns; if (Me->Combat->bNoCooldowns) Me->Combat->ClearCooldowns();
			NWDevPrefs::Set(TEXT("NoCooldowns"), Me->Combat->bNoCooldowns); }
		break;
	case ENWDevCommand::ToggleGodMode:
		if (Me) { Me->Combat->bGodMode = !Me->Combat->bGodMode; NWDevPrefs::Set(TEXT("GodMode"), Me->Combat->bGodMode); }
		break;
	case ENWDevCommand::ToggleInfiniteStamina:
		if (Me) { Me->Combat->bInfiniteStamina = !Me->Combat->bInfiniteStamina; NWDevPrefs::Set(TEXT("InfiniteStamina"), Me->Combat->bInfiniteStamina); }
		break;
	case ENWDevCommand::HealSelf:
		if (Me) Me->Combat->HealFull();
		break;
	case ENWDevCommand::ClearCooldowns:
		if (Me) Me->Combat->ClearCooldowns();
		break;
	case ENWDevCommand::SpawnLost:
		Spawn(ANWLost::StaticClass(), nullptr);
		break;
	case ENWDevCommand::SpawnDummy:
		Spawn(ANWCharacter::StaticClass(), nullptr);
		break;
	case ENWDevCommand::SpawnBlockingDummy:
		Spawn(ANWCharacter::StaticClass(), [](ANWCharacter* C) { C->bDummyBlocks = true; });
		break;
	case ENWDevCommand::SpawnAttackingDummy:
		Spawn(ANWCharacter::StaticClass(), [](ANWCharacter* C) { C->bDummyAttacks = true; });
		break;
	case ENWDevCommand::KillEnemies:
		for (TActorIterator<ANWCharacter> It(World); It; ++It)
		{
			if (!It->IsPlayerControlled())
			{
				It->Combat->Kill(Me);   // counts as a kill by the player (loot)
			}
		}
		break;
	case ENWDevCommand::RemoveSpawned:
		for (TActorIterator<ANWCharacter> It(World); It; ++It)
		{
			if (It->ActorHasTag(DevTag))
			{
				It->Destroy();
			}
		}
		break;
	case ENWDevCommand::ToggleSlowMotion:
		UGameplayStatics::SetGlobalTimeDilation(World, UGameplayStatics::GetGlobalTimeDilation(World) < 0.99f ? 1.f : 0.3f);
		break;
	case ENWDevCommand::TravelArena:
		UGameplayStatics::OpenLevel(this, TEXT("/Game/NW/Maps/L_Arena"));
		break;
	case ENWDevCommand::TravelCity:
		UGameplayStatics::OpenLevel(this, TEXT("/Game/NW/Maps/L_SouthWindsward"));
		break;
	case ENWDevCommand::KillSelf:
		if (Me) Me->Combat->Kill();
		break;
	case ENWDevCommand::ToggleFastRun:
		// Cross the map: all speeds × 4 (again returns to normal).
		if (Me) { Me->DevSpeedMultiplier = Me->DevSpeedMultiplier > 1.f ? 1.f : 4.f; NWDevPrefs::Set(TEXT("FastRun"), Me->DevSpeedMultiplier > 1.f); }
		break;
	case ENWDevCommand::ToggleFly:
		if (Me)
		{
			Me->bDevFlying = !Me->bDevFlying;
			Me->ApplyDevFlying();
		}
		break;
	case ENWDevCommand::GiveTools:
		if (Me && Me->Inventory) Me->Inventory->GiveTools();
		break;
	case ENWDevCommand::GoToCreature:
		// Nearest creature spawner (more than 30 m away): to see boars and wolves without searching.
		if (const ANWSpawnField* Spawns = ANWSpawnField::Get(World); Spawns && Me)
		{
			FVector At;
			if (Spawns->NearestSpawn(Me->GetActorLocation(), 3000.f, At))
			{
				const FVector Dir = (Me->GetActorLocation() - At).GetSafeNormal2D();
				Me->TeleportTo(At + Dir * 2500.f + FVector(0.f, 0.f, 300.f), FRotator(0.f, (-Dir).Rotation().Yaw, 0.f));
			}
		}
		break;
	case ENWDevCommand::GiveTradeLevels:
		if (Me && Me->Progression)
		{
			for (int32 t = 0; t < NWTradeskill::Num; ++t)
			{
				Me->Progression->AddTradeXp(NWTradeskill::Name(t), Me->Progression->TradeXpToNext(t));
			}
		}
		break;
	case ENWDevCommand::GiveCraftLevels:
		// +50 levels in the crafting trade skills (test crafting and spectrum, docs/game/crafting.md).
		if (Me && Me->Progression)
		{
			for (const TCHAR* Skill : {TEXT("Weaponsmithing"), TEXT("Engineering"), TEXT("Arcana")})
			{
				const int32 t = NWTradeskill::Index(Skill);
				for (int32 i = 0; i < 50; ++i)
				{
					Me->Progression->AddTradeXp(NWTradeskill::Name(t), FMath::Max(1, Me->Progression->TradeXpToNext(t)));
				}
			}
		}
		break;
	case ENWDevCommand::GiveCraftMaterials:
		// Refined materials T2–T5 for the crafting recipes and coins for the fee (docs/game/crafting.md).
		if (Me && Me->Inventory)
		{
			TArray<FNWItem> Gift;
			for (int32 Tier = 2; Tier <= 5; ++Tier)
			{
				for (const TPair<const TCHAR*, int32>& M : {TPair<const TCHAR*, int32>(TEXT("IngotT"), 60), TPair<const TCHAR*, int32>(TEXT("TimberT"), 20),
					TPair<const TCHAR*, int32>(TEXT("LeatherT"), 20)})
				{
					FNWItem I;
					I.ItemId = FName(*(FString(M.Key) + FString::FromInt(Tier)));
					I.Quantity = M.Value;
					Gift.Add(I);
				}
			}
			Me->Inventory->Grant(Gift, 100000);
		}
		break;
	case ENWDevCommand::GiveLevel:
		// XP missing to the next level (test the XP bar and the attribute points).
		if (Me && Me->Progression) Me->Progression->AddXp(FMath::Max<int64>(1, UNWProgressionComponent::XpForLevel(Me->Progression->Level + 1) - Me->Progression->Xp));
		break;
	case ENWDevCommand::GoToOre:
		// Nearest available ore vein: puts the character 1.5 m away, facing it.
		if (const ANWGatherField* Field = ANWGatherField::Get(World); Field && Me)
		{
			int32 Best = INDEX_NONE;
			float BestDist = TNumericLimits<float>::Max();
			for (int32 i = 0; i < Field->Num(); ++i)
			{
				const FVector L = Field->GetActorTransform().TransformPosition(Field->Node(i).Transform.GetLocation());
				const float D = FVector::DistSquared2D(L, Me->GetActorLocation());
				if (!Field->IsDepleted(i) && Field->VariantOf(i).Action == TEXT("Mining") && D < BestDist && D > FMath::Square(300.f))
				{
					Best = i;
					BestDist = D;
				}
			}
			if (Best != INDEX_NONE)
			{
				const FVector L = Field->GetActorTransform().TransformPosition(Field->Node(Best).Transform.GetLocation());
				const FVector Dir = (Me->GetActorLocation() - L).GetSafeNormal2D();
				const FVector To = L + Dir * (Field->VariantOf(Best).Radius * 0.5f) + FVector(0.f, 0.f, 120.f);
				Me->TeleportTo(To, FRotator(0.f, (-Dir).Rotation().Yaw, 0.f));
				SetControlRotation(FRotator(-10.f, (-Dir).Rotation().Yaw, 0.f));
				ClientSetRotation(GetControlRotation());
			}
		}
		break;
	}
}

// ---------------------------------------------------------------- progress saved in nw-core

void ANWPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ANWPlayerController, ProgressJson, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ANWPlayerController, bDevAllowed, COND_OwnerOnly);
}

void ANWPlayerController::PublishProgress()
{
	if (!Record)
	{
		return;
	}
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetObjectField(TEXT("loadout"), Record->Loadout.IsValid() ? Record->Loadout : MakeShared<FJsonObject>());
	Obj->SetObjectField(TEXT("mastery"), Record->Mastery.IsValid() ? Record->Mastery : MakeShared<FJsonObject>());
	FString Out;
	FJsonSerializer::Serialize(Obj, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out));
	ProgressJson = Out;
	ForceNetUpdate();
}

void ANWPlayerController::OnRep_Progress()
{
	TSharedPtr<FJsonObject> Obj;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ProgressJson), Obj) || !Obj.IsValid())
	{
		return;
	}
	auto Store = [](const TSharedPtr<FJsonObject>* Section, const TCHAR* IniSection)
	{
		for (const auto& [Key, Value] : (*Section)->Values)
		{
			TArray<FString> Names;
			const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
			if (Value->TryGetArray(Items))
			{
				for (const TSharedPtr<FJsonValue>& Item : *Items)
				{
					Names.Add(Item->AsString());
				}
			}
			GConfig->SetArray(IniSection, *Key, Names, GGameUserSettingsIni);
		}
	};
	const TSharedPtr<FJsonObject>* Section = nullptr;
	if (Obj->TryGetObjectField(TEXT("loadout"), Section))
	{
		Store(Section, NWIni::Loadout);
	}
	if (Obj->TryGetObjectField(TEXT("mastery"), Section))
	{
		Store(Section, NWIni::Mastery);
	}
	GConfig->Flush(false, GGameUserSettingsIni);
	if (ANWCharacter* Ch = Cast<ANWCharacter>(GetPawn()))
	{
		Ch->LoadAbilityLoadout();
	}
}

void ANWPlayerController::ServerSetMastery_Implementation(const FString& Weapon, const TArray<FString>& Ids)
{
	if (!Record || Weapon.Len() > 32 || Ids.Num() > 64)
	{
		return;
	}
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FString& Id : Ids)
	{
		Values.Add(MakeShared<FJsonValueString>(Id.Left(64)));
	}
	if (!Record->Mastery.IsValid())
	{
		Record->Mastery = MakeShared<FJsonObject>();
	}
	Record->Mastery->SetArrayField(Weapon, Values);
}

// ---------------------------------------------------------------- load test bot

void ANWPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (BotMode < 0)
	{
		BotMode = FParse::Param(FCommandLine::Get(), TEXT("nwbot")) ? 1 : 0;
		FParse::Value(FCommandLine::Get(), TEXT("-nwtestdie="), TestDieIn);
		FParse::Value(FCommandLine::Get(), TEXT("-nwtestchat="), TestChat, false);
		FParse::Value(FCommandLine::Get(), TEXT("-nwtestdev="), TestDev, false);
		FParse::Value(FCommandLine::Get(), TEXT("-nwtestinv="), TestInv, false);
		FParse::Value(FCommandLine::Get(), TEXT("-nwtestgather="), TestGather);
		TestInv.TrimQuotesInline();
		TestDev.TrimQuotesInline();
		TestChat.TrimQuotesInline();
		bTestRespawn = FParse::Param(FCommandLine::Get(), TEXT("nwtestrespawn"));
	}
	if (GetNetMode() == NM_Client && GetPawn())
	{
		if (!TestChat.IsEmpty() && (TestChatIn -= DeltaTime) <= 0.f)
		{
			// Several messages separated by |, one at a time.
			FString First, Rest;
			if (!TestChat.Split(TEXT("|"), &First, &Rest))
			{
				First = TestChat;
			}
			ServerSendChat(First.Replace(TEXT("_"), TEXT(" ")));   // _ = space (the test script splits arguments by space)
			TestChat = Rest;
			TestChatIn = 1.5f;
		}
		if (!TestDev.IsEmpty() && (TestDevIn -= DeltaTime) <= 0.f)
		{
			// Dev menu commands by enum name, separated by |, one every 2 s.
			FString First, Rest;
			if (!TestDev.Split(TEXT("|"), &First, &Rest))
			{
				First = TestDev;
			}
			const int64 Value = StaticEnum<ENWDevCommand>()->GetValueByNameString(First);
			if (Value != INDEX_NONE)
			{
				ServerDev(ENWDevCommand(Value));
			}
			TestDev = Rest;
			TestDevIn = 2.f;
		}
		if (!TestInv.IsEmpty() && (TestInvIn -= DeltaTime) <= 0.f)
		{
			FString First, Rest;
			if (!TestInv.Split(TEXT("|"), &First, &Rest))
			{
				First = TestInv;
			}
			TestInv = Rest;
			TestInvIn = 1.5f;
			TArray<FString> P;
			First.ParseIntoArray(P, TEXT(":"));
			UNWInventoryComponent* Inv = Cast<ANWCharacter>(GetPawn()) ? Cast<ANWCharacter>(GetPawn())->Inventory : nullptr;
			if (Inv && P.Num() > 0)
			{
				UE_LOG(LogTemp, Display, TEXT("NW inventory test: %s"), *First);
				if (P[0] == TEXT("potion")) Inv->ServerUseBestPotion();
				else if (P[0] == TEXT("move") && P.Num() == 3) Inv->ServerMove(ENWItemContainer::Bag, FCString::Atoi(*P[1]), ENWItemContainer::Bag, FCString::Atoi(*P[2]));
				else if (P[0] == TEXT("unequip") && P.Num() == 3) Inv->ServerMove(ENWItemContainer::Equip, FCString::Atoi(*P[1]), ENWItemContainer::Bag, FCString::Atoi(*P[2]));
				else if (P[0] == TEXT("equip") && P.Num() == 3) Inv->ServerMove(ENWItemContainer::Bag, FCString::Atoi(*P[1]), ENWItemContainer::Equip, FCString::Atoi(*P[2]));
			}
		}
		if (TestGather > 0 && (TestGatherIn -= DeltaTime) <= 0.f)
		{
			TestGatherIn = 1.f;
			ANWCharacter* G = Cast<ANWCharacter>(GetPawn());
			if (G && G->Gather && !G->Gather->IsGathering() && G->Gather->FocusIndex != INDEX_NONE)
			{
				UE_LOG(LogTemp, Display, TEXT("NW gathering test: %s (%d)%s"), *G->Gather->FocusText, G->Gather->FocusIndex,
					G->Gather->bFocusBlocked ? TEXT(" blocked") : TEXT(""));
				G->Gather->InteractPressed();
				--TestGather;
				TestGatherIn = 3.f;
			}
		}
		if (TestDieIn > 0.f && (TestDieIn -= DeltaTime) <= 0.f)
		{
			ServerDev(ENWDevCommand::KillSelf);
		}
		const ANWCharacter* Dead = Cast<ANWCharacter>(GetPawn());
		if (bTestRespawn && Dead && Dead->Combat->State == ENWCombatState::Dead && Dead->Combat->GetRespawnRemaining() <= 0.f
			&& Dead->Combat->RespawnAt > 0.f)
		{
			if (const ANWHUD* HUD = GetNWHUD(); HUD && HUD->IsDeathScreen())
			{
				UE_LOG(LogTemp, Display, TEXT("NW test: Respawn"));
				Dead->Combat->ServerRequestRespawn();
				bTestRespawn = false;
			}
		}
	}
	ANWCharacter* Me = Cast<ANWCharacter>(GetPawn());
	if (BotMode == 0 || !Me || !IsLocalController() || GetNetMode() != NM_Client)
	{
		return;
	}
	if (BotReleaseIn > 0.f)
	{
		BotReleaseIn -= DeltaTime;
		if (BotReleaseIn <= 0.f)
		{
			Me->Combat->AttackReleased();
		}
	}
	BotTimer -= DeltaTime;
	if (BotTimer <= 0.f)
	{
		BotTimer = FMath::FRandRange(1.5f, 3.f);
		BotDir = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
		const float Roll = FMath::FRand();
		if (Roll < 0.2f)
		{
			Me->Combat->RequestLightAttack();
		}
		else if (Roll < 0.4f && BotReleaseIn <= 0.f)
		{
			// Holds the attack (charge) and releases later: light or heavy depending on the time.
			Me->Combat->AttackPressed();
			BotReleaseIn = FMath::FRandRange(0.1f, 0.6f);
		}
		else if (Roll < 0.6f && Me->Abilities.Num() > 0)
		{
			Me->Combat->RequestAction(Me->Abilities[FMath::RandHelper(Me->Abilities.Num())]);
		}
		else if (Roll < 0.7f)
		{
			Me->Combat->RequestDodge(FVector2D(1.f, 0.f));
		}
	}
	Me->AddMovementInput(BotDir, 1.f);
}

// ---------------------------------------------------------------- chat

DEFINE_LOG_CATEGORY_STATIC(LogNWChat, Log, All);

static constexpr int32 ChatMaxLength = 225;   // same limit as the game (RemainingTextCount)

void ANWPlayerController::OpenChat()
{
	ANWHUD* HUD = GetNWHUD();
	if (!HUD || HUD->Login.IsActive() || HUD->Mastery.IsOpen() || OpenMenu.IsValid() || HUD->Chat.IsOpen())
	{
		return;
	}
	HUD->Chat.Open(this);
}

bool ANWPlayerController::IsChatOpen() const
{
	const ANWHUD* HUD = GetNWHUD();
	return HUD && HUD->Chat.IsOpen();
}

void ANWPlayerController::ServerSendChat_Implementation(const FString& InText)
{
	FString Text = InText.TrimStartAndEnd().Left(ChatMaxLength);
	Text.ReplaceInline(TEXT("\n"), TEXT(" "));
	if (Text.IsEmpty())
	{
		return;
	}
	// Up to 5 messages every 5 s.
	const double Now = GetWorld()->GetTimeSeconds();
	ChatTimes.RemoveAll([Now](double T) { return T < Now - 5.0; });
	if (ChatTimes.Num() >= 5)
	{
		SystemMessage(NWText::Key(TEXT("nw.chat.too_many")));
		return;
	}
	ChatTimes.Add(Now);
	if (Text.StartsWith(TEXT("/")))
	{
		HandleChatCommand(Text);
		return;
	}
	const FString Sender = PlayerState ? PlayerState->GetPlayerName() : TEXT("?");
	UE_LOG(LogNWChat, Display, TEXT("[Area] %s: %s"), *Sender, *Text);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ANWPlayerController* PC = Cast<ANWPlayerController>(It->Get()))
		{
			PC->ClientChat(ENWChatChannel::Area, Sender, Text);
		}
	}
}

void ANWPlayerController::HandleChatCommand(const FString& Text)
{
	FString Command, Rest;
	if (!Text.Mid(1).Split(TEXT(" "), &Command, &Rest))
	{
		Command = Text.Mid(1);
	}
	Command.ToLowerInline();
	Rest.TrimStartAndEndInline();
	if (Command == TEXT("ajuda") || Command == TEXT("help"))
	{
		SystemMessage(NWText::Key(TEXT("nw.chat.help")));
		return;
	}
	ANWGameMode* GM = GetWorld()->GetAuthGameMode<ANWGameMode>();
	if (!GM)
	{
		return;
	}
	// Commands in Portuguese and in English.
	if (Command == TEXT("convidar") || Command == TEXT("invite"))
	{
		GM->GroupInvite(this, Rest);
	}
	else if (Command == TEXT("aceitar") || Command == TEXT("accept"))
	{
		GM->GroupAccept(this);
	}
	else if (Command == TEXT("recusar") || Command == TEXT("decline"))
	{
		GM->GroupDecline(this);
	}
	else if (Command == TEXT("sair") || Command == TEXT("leave"))
	{
		GM->GroupLeave(this);
	}
	else if (Command == TEXT("g") || Command == TEXT("grupo") || Command == TEXT("group"))
	{
		if (!Rest.IsEmpty())
		{
			GM->GroupChat(this, Rest);
		}
	}
	else
	{
		SystemMessage(NWText::Key(TEXT("nw.chat.unknown_command"), {Command}));
	}
}

void ANWPlayerController::ClientChat_Implementation(ENWChatChannel Channel, const FString& Sender, const FString& Text)
{
	if (ANWHUD* HUD = GetNWHUD())
	{
		HUD->Chat.Add(Channel, Sender, Text);
	}
}

#include "NWPlayerController.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NWProgressionComponent.h"
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

void ANWPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		FNWOptions::Get().Apply(GetWorld());
	}
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(BreadcrumbTimer, this, &ANWPlayerController::RecordBreadcrumb, 1.f, true);
	}
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
	// A última migalha pode ser o lugar onde prendeu: usa a anterior quando há.
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
	return OpenMenu.IsValid() || (HUD && (HUD->Mastery.IsOpen() || HUD->Tradeskills.IsOpen() || HUD->Login.IsActive() || HUD->Chat.IsOpen() || HUD->Inventory.IsOpen()
		|| HUD->GameMenu.IsOpen() || HUD->WorldMap.IsOpen()));
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
		HUD->Inventory.Close();
		HUD->GameMenu.Close();
		HUD->WorldMap.Close();
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
	if (HUD->Mastery.IsOpen() || HUD->Tradeskills.IsOpen())
	{
		CloseMenus();
		return;
	}
	CloseMenus();
	HUD->Mastery.Open(Cast<ANWCharacter>(GetPawn()));
	if (HUD->Mastery.IsOpen())
	{
		// Sem widget Slate: os cliques chegam pelo InputComponent do controlador (OnMenuClick).
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
	if (HUD && HUD->WorldMap.IsOpen() && GetMousePosition(X, Y))
	{
		HUD->WorldMap.Wheel(1.f, FVector2D(X, Y));
	}
}

void ANWPlayerController::OnWheelDown()
{
	ANWHUD* HUD = GetNWHUD();
	float X = 0.f, Y = 0.f;
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
	// Só o "apertar" vira a tecla nova; soltar e eixos seguem normais (senão o soltar do clique que abriu a espera
	// nunca chega e o botão fica preso como apertado).
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
	// Sem consumir: com o menu fechado o clique segue para o ataque do personagem.
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
	float X = 0.f, Y = 0.f;
	GetMousePosition(X, Y);
	if (HUD && HUD->Login.IsActive())
	{
		HUD->Login.Click(FVector2D(X, Y));
		return;
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
	// Barra de navegação do topo (inventário, personagem, mapa): troca de tela.
	if (HUD && (HUD->Inventory.IsOpen() || HUD->Mastery.IsOpen() || HUD->Tradeskills.IsOpen() || HUD->WorldMap.IsOpen()))
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
	// Abas da janela K: Maestria em Armas / Hab. Mercantes.
	if (HUD && (HUD->Mastery.IsOpen() || HUD->Tradeskills.IsOpen()))
	{
		const int32 Tab = FNWTradeskillMenu::TabAt(FVector2D(X, Y));
		if (Tab == 1 && HUD->Mastery.IsOpen())
		{
			HUD->Mastery.Close();
			HUD->Tradeskills.Open(Cast<ANWCharacter>(GetPawn()));
			return;
		}
		if (Tab == 0 && HUD->Tradeskills.IsOpen())
		{
			HUD->Tradeskills.Close();
			HUD->Mastery.Open(Cast<ANWCharacter>(GetPawn()));
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
			GM->SavePlayer(this);
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

void ANWPlayerController::ServerDev_Implementation(ENWDevCommand Command)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetPawn());
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Invocação: 5 m à frente, virado para o jogador.
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
	switch (Command)
	{
	case ENWDevCommand::ToggleNoCooldowns:
		if (Me) { Me->Combat->bNoCooldowns = !Me->Combat->bNoCooldowns; if (Me->Combat->bNoCooldowns) Me->Combat->ClearCooldowns(); }
		break;
	case ENWDevCommand::ToggleGodMode:
		if (Me) Me->Combat->bGodMode = !Me->Combat->bGodMode;
		break;
	case ENWDevCommand::ToggleInfiniteStamina:
		if (Me) Me->Combat->bInfiniteStamina = !Me->Combat->bInfiniteStamina;
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
				It->Combat->Kill(Me);   // conta como morte pelo jogador (saque)
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
		// Atravessar o mapa: todas as velocidades × 4 (de novo volta ao normal).
		if (Me) Me->DevSpeedMultiplier = Me->DevSpeedMultiplier > 1.f ? 1.f : 4.f;
		break;
	case ENWDevCommand::GiveTools:
		if (Me && Me->Inventory) Me->Inventory->GiveTools();
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
	case ENWDevCommand::GiveLevel:
		// XP que falta para o próximo nível (testar a barra de XP e os pontos de atributo).
		if (Me && Me->Progression) Me->Progression->AddXp(FMath::Max<int64>(1, UNWProgressionComponent::XpForLevel(Me->Progression->Level + 1) - Me->Progression->Xp));
		break;
	case ENWDevCommand::GoToOre:
		// Veio de minério disponível mais perto: põe o personagem a 1,5 m, de frente para ele.
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

// ---------------------------------------------------------------- progresso salvo no nw-core

void ANWPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ANWPlayerController, ProgressJson, COND_OwnerOnly);
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

// ---------------------------------------------------------------- bot de teste de carga

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
			// Várias mensagens separadas por |, uma por vez.
			FString First, Rest;
			if (!TestChat.Split(TEXT("|"), &First, &Rest))
			{
				First = TestChat;
			}
			ServerSendChat(First.Replace(TEXT("_"), TEXT(" ")));   // _ = espaço (o script de teste separa argumentos por espaço)
			TestChat = Rest;
			TestChatIn = 1.5f;
		}
		if (!TestDev.IsEmpty() && (TestDevIn -= DeltaTime) <= 0.f)
		{
			// Comandos do menu de dev pelo nome do enum, separados por |, um a cada 2 s.
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
				UE_LOG(LogTemp, Display, TEXT("NW teste inventário: %s"), *First);
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
				UE_LOG(LogTemp, Display, TEXT("NW teste coleta: %s (%d)%s"), *G->Gather->FocusText, G->Gather->FocusIndex,
					G->Gather->bFocusBlocked ? TEXT(" bloqueado") : TEXT(""));
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
				UE_LOG(LogTemp, Display, TEXT("NW teste: Renascer"));
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
			// Segura o ataque (carga) e solta depois: leve ou pesado conforme o tempo.
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

static constexpr int32 ChatMaxLength = 225;   // mesmo limite do jogo (RemainingTextCount)

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
	// Até 5 mensagens a cada 5 s.
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
	UE_LOG(LogNWChat, Display, TEXT("[Área] %s: %s"), *Sender, *Text);
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
	// Comandos em português e em inglês.
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

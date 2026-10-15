#include "NWHUD.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "NWManaComponent.h"
#include "NWShop.h"
#include "SNWCity.h"
#include "SNWMarket.h"
#include "NWProgressionComponent.h"
#include "NWCreature.h"
#include "NWText.h"

#include "NWBackend.h"
#include "NWItems.h"
#include "NWGatherComponent.h"
#include "NWGatherField.h"
#include "NWInteractable.h"
#include "NWNpc.h"
#include "NWCrafting.h"
#include "NWNpcRewards.h"
#include "NWOptions.h"
#include "NWInventoryComponent.h"
#include "GameFramework/GameStateBase.h"
#include "NWPlayerState.h"
#include "GameFramework/PlayerState.h"
#include "NWLost.h"

#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
#include "NWAnimInstance.h"
#include "NWCharacter.h"
#include "NWCombatComponent.h"
#include "NWPlayerController.h"

static TAutoConsoleVariable<int32> CVarHudDebug(TEXT("nw.HudDebug"), 0, TEXT("Shows the movement/animation diagnostic line on the HUD."));

static const TCHAR* VitalsCanvas = TEXT("vitals2024");
static const TCHAR* WeaponCanvas = TEXT("weaponquickslots");
static const TCHAR* ConsumableCanvas = TEXT("consumablequickslots");
static const TCHAR* XpCanvas = TEXT("xp");

static const TCHAR* SlotNames[3] = {TEXT("WeaponAbility"), TEXT("WeaponAbility2"), TEXT("WeaponAbility3")};
static const TCHAR* SetNames[2] = {TEXT("WeaponQuickslots/WeaponAbilitySet"), TEXT("WeaponQuickslots/WeaponAbilitySet2")};
static const TCHAR* SlotKeys[3] = {TEXT("Q"), TEXT("R"), TEXT("F")};

void ANWHUD::BeginPlay()
{
	Super::BeginPlay();
	InitLayouts();
}

void ANWHUD::InitLayouts()
{
	// Outside Content: the editor watches Content and would try to import the .json and the .otf as assets.
	Layout.Load(NWText::DataPath(TEXT("Data/UI/hud_layout.json")));
	Layout.bScaleChildren = true;   // the inactive weapon row (0.5) and the consumable item (1.15) scale with their children
	Layout.bExactText = true;       // LyShine text sizes (the health value was ~1.5x too large)
	Layout.ResolveTexture = [this](const FString& P) { return ResolveTexture(P); };
	// Icon and background of each ability (tools/make_abilities.py), for both weapons.
	FString Raw;
	TSharedPtr<FJsonObject> Root;
	if (FFileHelper::LoadFileToString(Raw, *NWText::DataPath(TEXT("Data/UI/abilities.json")))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) && Root.IsValid())
	{
		for (const auto& Weapon : Root->Values)
		{
			for (const TSharedPtr<FJsonValue>& V : Weapon.Value->AsObject()->GetArrayField(TEXT("abilities")))
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				AbilityArt.Add(FName(O->GetStringField(TEXT("action"))), TPair<FString, FString>(O->GetStringField(TEXT("icon")), O->GetStringField(TEXT("background"))));
			}
		}
	}
	for (int32 i = 0; i < 3; ++i)
	{
		Fonts.Add(FNWUILayout::GameFont(i));
		Layout.Fonts[i] = Fonts[i];
	}
	if (GroupLayout.Load(NWText::DataPath(TEXT("Data/UI/group_layout.json"))))
	{
		GroupLayout.ResolveTexture = [this](const FString& P) { return ResolveTexture(P); };
		for (int32 i = 0; i < 3; ++i)
		{
			GroupLayout.Fonts[i] = Fonts[i];
		}
		// The game's bar fills by clipping; here, linear fill from the left.
		if (FNWUIElement* Fill = GroupLayout.FindOne(TEXT("member"), TEXT("Health/Fill")))
		{
			Fill->FillType = 1;
			Fill->FillEdgeOrigin = 0;
		}
	}
}

void ANWHUD::ReloadLanguage()
{
	Layout.Load(NWText::DataPath(TEXT("Data/UI/hud_layout.json")));
	Layout.bScaleChildren = true;
	Layout.bExactText = true;
	bStaticDone = false;
	Mastery.Close();
	Mastery = FNWMasteryMenu();
	Tradeskills.Close();
	Tradeskills = FNWTradeskillMenu();
	Attributes.Close();
	Attributes = FNWAttributesMenu();
	Inventory.Close();
	Inventory = FNWInventoryMenu();
	FNWItemCatalog::Get().Reload();
	Respawn = FNWRespawnScreen();
	Login.ReloadLanguage();
}

UTexture2D* ANWHUD::ResolveTexture(const FString& GameTexture)
{
	return FNWUILayout::GameTexture(GameTexture);
}

void ANWHUD::SetupStatic()
{
	bStaticDone = true;
	// Quick slots start empty (UpdateConsumables fills the equipped ones).
	for (FNWUIElement* E : Layout.Find(ConsumableCanvas, TEXT("ItemLayoutConsumable"))) E->Visible = false;
	for (FNWUIElement* E : Layout.Find(ConsumableCanvas, TEXT("CooldownTimer"))) E->Visible = false;
	// Game keys (libs/config/defaultprofile.xml: quickslot-consumable-1..4 = 3..6); no controller icon.
	const TCHAR* ConsumableSlots[4] = {TEXT("ConsumableQuickslot"), TEXT("ConsumableQuickslot2"), TEXT("ConsumableQuickslot3"), TEXT("ConsumableQuickslot4")};
	for (int32 i = 0; i < 4; ++i)
	{
		if (FNWUIElement* E = Layout.FindOne(ConsumableCanvas, FString(ConsumableSlots[i]) + TEXT("/Hint/HintText"))) E->TextOverride = FString::FromInt(3 + i);
	}
	for (FNWUIElement* E : Layout.Find(ConsumableCanvas, TEXT("ModifierDivider/HintModifier"))) E->Visible = false;
	// Colors the Lua applies to the bars (uistyle.lua, in sRGB): decrease color of each meter; the main fill changes
	// with the state in UpdateVitals.
	auto Tint = [this](const TCHAR* Path, const FLinearColor& C) { for (FNWUIElement* E : Layout.Find(VitalsCanvas, Path)) E->Tint = C; };
	Tint(TEXT("HealthMeter/MultiFillMeter/UnderFill"), FLinearColor(FColor(221, 221, 221)));      // COLOR_GRAY_90
	Tint(TEXT("ManaMeter/MultiFillMeter/MainFill"), FLinearColor(FColor(47, 97, 251)));          // COLOR_MANA
	Tint(TEXT("ManaMeter/MultiFillMeter/UnderFill"), FLinearColor(FColor(168, 178, 197)));       // COLOR_BLUE_PALE
	Tint(TEXT("StaminaMeter/MultiFillMeter/UnderFill"), FLinearColor(FColor(158, 148, 126)));    // COLOR_TAN
	// Default HUD layout (hudlayoutdatadefault.lua): the canvas has other positions for mana and XP. Lua x/y = center
	// from the bottom middle of the screen.
	auto Place = [this](const FString& CanvasName, const TCHAR* Path, float Y, float W, float H)
	{
		if (FNWUIElement* E = Layout.FindOne(CanvasName, Path))
		{
			E->Offsets = FVector4f(-W * 0.5f, Y - H * 0.5f, W * 0.5f, Y + H * 0.5f);
		}
	};
	Place(VitalsCanvas, TEXT("Window/ManaMeterLayoutElement"), -382.f, 174.f, 18.f);
	Place(XpCanvas, TEXT("Window/XpMeterLayoutElement"), -41.f, 334.f, 58.f);
	// Consumable item: square layout (itemlayout.lua SetLayoutByItemType for consumables), 52 x 52 at the canvas'
	// 1.15 scale = the 60 px of the slot; the icon square too, and no tier.
	for (FNWUIElement* E : Layout.Find(ConsumableCanvas, TEXT("ItemLayoutConsumable"))) E->Offsets = FVector4f(1.f, 1.f, 53.f, 53.f);
	for (FNWUIElement* E : Layout.Find(ConsumableCanvas, TEXT("ItemLayoutConsumable/ItemIcon"))) E->Offsets = FVector4f(-26.f, -26.f, 26.f, 26.f);
	for (FNWUIElement* E : Layout.Find(ConsumableCanvas, TEXT("ItemLayoutConsumable/ItemTier"))) E->Visible = false;
	for (const TCHAR* Cover : {TEXT("WarningCover"), TEXT("RefillCover")})
	{
		for (FNWUIElement* E : Layout.Find(VitalsCanvas, Cover)) E->Visible = false;
	}
}

void ANWHUD::UpdateVitals(const UNWCombatComponent* C, float DeltaSeconds)
{
	const float H = C->MaxHealth > 0.f ? C->Health / C->MaxHealth : 0.f;
	const float S = C->MaxStamina > 0.f ? C->Stamina / C->MaxStamina : 0.f;
	const double Now = FPlatformTime::Seconds();
	if ((LastHealthSeen >= 0.f && C->Health < LastHealthSeen) || (LastStaminaSeen >= 0.f && C->Stamina < LastStaminaSeen - 0.5f))
	{
		CombatUntil = Now + 10.0;
	}
	LastHealthSeen = C->Health;
	LastStaminaSeen = C->Stamina;
	const bool bInCombat = ShotPawn ? bShotCombat : Now < CombatUntil;
	// The back bar follows the front one with a delay when it drops; when it rises, together.
	HealthLag = H >= HealthLag ? H : FMath::FInterpConstantTo(HealthLag, H, DeltaSeconds, 0.35f);
	StaminaLag = S >= StaminaLag ? S : FMath::FInterpConstantTo(StaminaLag, S, DeltaSeconds, 0.6f);
	// Opacity (healthmeter.lua UpdateOpacity): 1 in combat or not full, otherwise opacityAtFull of the default layout
	// (health and stamina 0.4, mana 0); the value text never below textMinOpacity 0.8.
	auto Meter = [this](const TCHAR* Root, float Ratio, float AtFull, bool bCombat)
	{
		const float Alpha = bCombat || Ratio < 0.999f ? 1.f : AtFull;
		for (FNWUIElement* E : Layout.Find(VitalsCanvas, FString(Root) + TEXT("/MultiFillMeter"))) E->AlphaMul = Alpha;
		for (FNWUIElement* E : Layout.Find(VitalsCanvas, FString(Root) + TEXT("/ValueText"))) E->AlphaMul = Alpha > 0.f ? FMath::Max(Alpha, 0.8f) : 0.f;
	};
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("HealthMeter/MultiFillMeter/MainFill")))
	{
		E->Fill = H;
		// SetColorThreshold: full = COLOR_GRAY_50, below = COLOR_HEALTH (COLOR_RED_DARK).
		E->Tint = H >= 0.999f ? FLinearColor(FColor(128, 128, 128)) : FLinearColor(FColor(180, 47, 14));
	}
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("HealthMeter/MultiFillMeter/UnderFill"))) E->Fill = HealthLag;
	// Text (UpdateText): full = the max; otherwise @ui_vitals_value ("value/max"), numbers with the language separators.
	const FCulturePtr Culture = FInternationalization::Get().GetCulture(NWText::Lang() == TEXT("pt-br") ? TEXT("pt-BR") : TEXT("en-US"));
	auto Num = [&Culture](float V) { return FText::AsNumber(FMath::Max(0, FMath::CeilToInt(V)), nullptr, Culture).ToString(); };
	const FString HealthText = H >= 0.999f ? Num(C->MaxHealth) : Num(C->Health) + TEXT("/") + Num(C->MaxHealth);
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("HealthMeter/ValueText"))) E->TextOverride = HealthText;
	Meter(TEXT("HealthMeter"), H, 0.4f, bInCombat);
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("StaminaMeter/MultiFillMeter/MainFill")))
	{
		E->Fill = S;
		// COLOR_STAMINA; exhausted (StaminaLocked) = COLOR_STAMINA_LOCKED (COLOR_GRAY_50).
		E->Tint = C->bExhausted ? FLinearColor(FColor(128, 128, 128)) : FLinearColor(FColor(255, 207, 42));
	}
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("StaminaMeter/MultiFillMeter/UnderFill"))) E->Fill = StaminaLag;
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("StaminaMeter/ValueText"))) E->Visible = false;
	Meter(TEXT("StaminaMeter"), S, 0.4f, bInCombat);
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("ManaMeter/ValueText"))) E->Visible = false;
	// Mana (UNWManaComponent, sub-area 1a): only with a mana weapon; full = hidden (opacityAtFull 0).
	const UNWManaComponent* Mana = C->GetOwner()->FindComponentByClass<UNWManaComponent>();
	const ANWCharacter* Owner = Cast<ANWCharacter>(C->GetOwner());
	const bool bManaWeapon = Mana && Owner && Owner->WeaponSets.IsValidIndex(Owner->ActiveWeapon) && Owner->WeaponSets[Owner->ActiveWeapon].Id == TEXT("IceGauntlet");
	const float M = Mana ? Mana->GetRatio() : 1.f;
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("ManaMeter/MultiFillMeter/MainFill"))) E->Fill = M;
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("ManaMeter/MultiFillMeter/UnderFill"))) E->Fill = M;
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("Window/ManaMeterLayoutElement"))) E->Visible = bManaWeapon;
	Meter(TEXT("ManaMeter"), M, 0.f, false);
}

void ANWHUD::UpdateAbilities(const ANWCharacter* Me, float DeltaSeconds)
{
	const UNWCombatComponent* C = Me->Combat;
	ReadyFlash.SetNumZeroed(6);
	WasCooling.SetNumZeroed(6);
	// Swap hint (weaponquickslots.lua UpdateSwapHint): the key of the weapon that is not in hand, beside the inactive row.
	const int32 Other = Me->OtherWeapon();
	const bool bOtherIsFirst = Me->Loadout.IsValidIndex(0) && Me->Loadout[0] == Other;
	for (FNWUIElement* E : Layout.Find(WeaponCanvas, TEXT("WeaponQuickslots/SwapHint"))) E->Visible = Me->WeaponSets.IsValidIndex(Other);
	if (FNWUIElement* E = Layout.FindOne(WeaponCanvas, TEXT("WeaponQuickslots/SwapHint/HintText")))
	{
		E->TextOverride = FNWOptions::Get().KeyFor(bOtherIsFirst ? TEXT("Weapon1") : TEXT("Weapon2")).GetDisplayName().ToString();
	}
	for (int32 Row = 0; Row < 2; ++Row)
	{
		// Row 0 (bottom, larger) = active weapon; row 1 = the other one, with the game's "inactive" background.
		const FString Set = SetNames[Row];
		const int32 SetIndex = Me->WeaponSets.Num() > 0 ? (Row == 0 ? Me->ActiveWeapon : Me->OtherWeapon()) : INDEX_NONE;
		// Row 1 only with a second weapon in the pair (Weapon 2 empty or unarmed: hidden).
		const bool bShown = Row == 0 || Me->WeaponSets.IsValidIndex(SetIndex);
		if (FNWUIElement* Root = Layout.FindOne(WeaponCanvas, Set))
		{
			Root->Visible = bShown;
			Root->TextureOverride = Row == 0 ? FString() : FString(TEXT("lyshineui/images/hud2024/weaponquickslots/weaponsetbginactive.dds"));
		}
		if (!bShown)
		{
			continue;
		}
		const FNWWeaponSet* WS = Me->WeaponSets.IsValidIndex(SetIndex) ? &Me->WeaponSets[SetIndex] : nullptr;
		const TArray<FName>& Abilities = WS ? WS->Abilities : Me->Abilities;
		if (FNWUIElement* Icon = Layout.FindOne(WeaponCanvas, Set + TEXT("/WeaponIcon")))
		{
			Icon->TextureOverride = WS ? WS->HudIcon : FString();
			Icon->Visible = WS != nullptr;
		}
		if (FNWUIElement* ShieldIcon = Layout.FindOne(WeaponCanvas, Set + TEXT("/ShieldIcon")))
		{
			ShieldIcon->Visible = WS && WS->bUsesShield;
		}
		for (int32 i = 0; i < 3; ++i)
		{
			const FString Slot = Set + TEXT("/") + SlotNames[i];
			const FName Ability = Abilities.IsValidIndex(i) ? Abilities[i] : NAME_None;
			const TPair<FString, FString>* Art = AbilityArt.Find(Ability);
			if (FNWUIElement* Icon = Layout.FindOne(WeaponCanvas, Slot + TEXT("/AbilityIcon")))
			{
				Icon->TextureOverride = Art ? Art->Key : FString();
				Icon->Visible = Art != nullptr;
			}
			if (FNWUIElement* Bg = Layout.FindOne(WeaponCanvas, Slot + TEXT("/Bg")))
			{
				Bg->TextureOverride = Art ? Art->Value : FString();
				Bg->Visible = Art != nullptr;
			}
			if (FNWUIElement* Hint = Layout.FindOne(WeaponCanvas, Slot + TEXT("/Hint")))
			{
				Hint->Visible = Row == 0;
			}
			if (FNWUIElement* Key = Layout.FindOne(WeaponCanvas, Slot + TEXT("/Hint/HintText")))
			{
				Key->TextOverride = FString(SlotKeys[i]);
			}
			float Duration = 0.f;
			const float Remaining = Ability.IsNone() ? 0.f : C->GetCooldownRemaining(Ability, &Duration);
			const bool bCooling = Remaining > 0.f;
			if (FNWUIElement* Timer = Layout.FindOne(WeaponCanvas, Slot + TEXT("/CooldownTimer"))) Timer->Visible = bCooling;
			// boxtimer.lua SetTimerStyle: COLOR_ABILITY_COOLDOWN (COLOR_AMAZON_ORANGE) at 0.8; the last second with a decimal.
			if (FNWUIElement* Bar = Layout.FindOne(WeaponCanvas, Slot + TEXT("/CooldownTimer/BarFill")))
			{
				Bar->Fill = Duration > 0.f ? Remaining / Duration : 0.f;
				Bar->Tint = FLinearColor(FColor(255, 153, 0)).CopyWithNewOpacity(0.8f);
			}
			if (FNWUIElement* Text = Layout.FindOne(WeaponCanvas, Slot + TEXT("/CooldownTimer/TimerText")))
			{
				Text->TextOverride = Remaining < 1.f ? FString::Printf(TEXT("%.1f"), Remaining) : FString::FromInt(FMath::CeilToInt(Remaining));
			}
			if (FNWUIElement* Dim = Layout.FindOne(WeaponCanvas, Slot + TEXT("/Dimmer")))
			{
				Dim->Visible = bCooling || Row == 1;
				Dim->Fill = 1.f;
			}
			// Glow when the cooldown ends.
			const int32 F = Row * 3 + i;
			if (WasCooling[F] && !bCooling)
			{
				ReadyFlash[F] = 0.5f;
			}
			WasCooling[F] = bCooling;
			ReadyFlash[F] = FMath::Max(0.f, ReadyFlash[F] - DeltaSeconds);
			if (FNWUIElement* Flash = Layout.FindOne(WeaponCanvas, Slot + TEXT("/ReadyFlash")))
			{
				Flash->Fade = FMath::Sin(ReadyFlash[F] / 0.5f * PI);
			}
		}
	}
}

void ANWHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	Login.Deactivate();
	Chat.Close();
	Inventory.Close();
	Super::EndPlay(Reason);
}

void ANWHUD::DrawHUD()
{
	DrawGameHUD();
	if (Canvas && FNWOptions::Get().bShowFps)
	{
		// FPS in the top-right corner (@ui_show_fps option).
		const float Fps = 1.f / FMath::Max(GetWorld()->GetDeltaSeconds(), 0.001f);
		if (UFont* Font = Fonts.IsValidIndex(1) ? Fonts[1].Get() : nullptr)
		{
			const float Scale = 18.f * Canvas->ClipY / 1080.f / FMath::Max(1, Font->LegacyFontSize);
			DrawText(FString::Printf(TEXT("%.0f FPS"), Fps), FLinearColor(0.92f, 0.89f, 0.84f), Canvas->ClipX - 120.f * Canvas->ClipY / 1080.f, 12.f, Font, Scale);
		}
	}
	if (Canvas && GameMenu.IsOpen())
	{
		float MX = 0.f, MY = 0.f;
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->GetMousePosition(MX, MY);
		}
		GameMenu.Draw(Canvas, FVector2D(MX, MY));
	}
}

void ANWHUD::DrawGameHUD()
{
	Super::DrawHUD();
	const ANWCharacter* Me = Cast<ANWCharacter>(GetOwningPawn());
	if (!Me || !Canvas)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	const float Dt = LastDrawTime > 0.0 ? FMath::Clamp(float(Now - LastDrawTime), 0.f, 0.1f) : 0.f;
	LastDrawTime = Now;

	// -nwshot[=file]: after a few seconds takes a screenshot and closes (check without a person).
	static double FirstDraw = Now;
	static int32 ShotState = 0;
	FString ShotFile;
	if (FParse::Value(FCommandLine::Get(), TEXT("-nwshot="), ShotFile) || FParse::Param(FCommandLine::Get(), TEXT("nwshot")))
	{
		if (ShotFile.IsEmpty())
		{
			ShotFile = FPaths::ProjectSavedDir() / TEXT("nwshot.png");
		}
		// -nwmenu=skill|dev: opens the menu before the screenshot.
		// Per HUD (reopens after joining the server); 1.5 s after this HUD starts drawing.
		if (HudFirstDraw <= 0.0)
		{
			HudFirstDraw = Now;
		}
		FString MenuName;
		if (!bMenuOpened && Now - FirstDraw > 4.5 && Now - HudFirstDraw > 1.5 && FParse::Value(FCommandLine::Get(), TEXT("-nwmenu="), MenuName))
		{
			bMenuOpened = true;
			if (ANWPlayerController* PC = Cast<ANWPlayerController>(GetOwningPlayerController()))
			{
				MenuName == TEXT("dev") ? PC->ToggleDevMenu() : MenuName == TEXT("inventory") ? PC->ToggleInventory() : PC->ToggleSkillMenu();
			}
		}
		// -nwweapon=2: swaps to the second weapon first.
		// Repeats until the swap takes effect (right after spawning the character is still falling).
		int32 WeaponArg = 0;
		if (Now - FirstDraw > 1.0 && FParse::Value(FCommandLine::Get(), TEXT("-nwweapon="), WeaponArg) && Me->ActiveWeapon != WeaponArg - 1)
		{
			const_cast<ANWCharacter*>(Me)->SwitchWeapon(WeaponArg - 1);
		}
		// Uses Q and R at 3 s so the screenshot shows the cooldown.
		static bool bUsedAbilities = false;
		if (!bUsedAbilities && Now - FirstDraw > 3.0 && Me->Abilities.Num() >= 2)
		{
			bUsedAbilities = true;
			Me->Combat->RequestAction(Me->Abilities[0]);
			FTimerHandle H;
			// Weak pointer: the map may change (joining the server) before the timer.
			GetWorldTimerManager().SetTimer(H, [Weak = TWeakObjectPtr<const ANWCharacter>(Me)]()
			{
				if (const ANWCharacter* C = Weak.Get(); C && C->Abilities.IsValidIndex(1))
				{
					C->Combat->RequestAction(C->Abilities[1]);
				}
			}, 1.6f, false);
		}
		// -nwshotdelay=N: waits longer (client that connects to a server before the screenshot).
		double ShotDelay = 6.0;
		FParse::Value(FCommandLine::Get(), TEXT("-nwshotdelay="), ShotDelay);
		if (ShotState == 0 && Now - FirstDraw > ShotDelay)
		{
			FScreenshotRequest::RequestScreenshot(ShotFile, true, false);
			ShotState = 1;
		}
		else if (ShotState >= 1 && ++ShotState > 30)
		{
			FPlatformMisc::RequestExit(false, TEXT("nwshot"));
		}
	}
	const UNWCombatComponent* C = Me->Combat;

	if (UNWBackend* Backend = GetGameInstance()->GetSubsystem<UNWBackend>(); Backend && Backend->WantsFrontEnd(GetWorld()))
	{
		APlayerController* PC = GetOwningPlayerController();
		if (!Login.IsActive())
		{
			Login.Activate(PC, Backend);
		}
		float MX = 0.f, MY = 0.f;
		if (PC)
		{
			PC->GetMousePosition(MX, MY);
		}
		Login.Draw(Canvas, FVector2D(MX, MY));
		return;
	}
	// Dead: only the death screen, with the mouse free for Respawn.
	const bool bDead = C->State == ENWCombatState::Dead && Me->IsLocallyControlled();
	if (bDead != bDeathScreen)
	{
		SetDeathScreen(bDead);
	}
	if (bDead)
	{
		float MX = 0.f, MY = 0.f;
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->GetMousePosition(MX, MY);
		}
		Respawn.Draw(Canvas, FVector2D(MX, MY), *C);
		Chat.Draw(Canvas);
		return;
	}
	if (WorldMap.IsOpen())
	{
		float MX = 0.f, MY = 0.f;
		bool bDown = false;
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->GetMousePosition(MX, MY);
			bDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);
		}
		WorldMap.Draw(Canvas, FVector2D(MX, MY), bDown, Me);
		DrawNavbar(2, FVector2D(MX, MY));
		return;
	}
	if (Inventory.IsOpen())
	{
		float MX = 0.f, MY = 0.f;
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->GetMousePosition(MX, MY);
			// Drag and drop: the button release arrives here (the press comes through the controller's click).
			Inventory.Hold(FVector2D(MX, MY), PC->IsInputKeyDown(EKeys::LeftMouseButton));
		}
		Inventory.Draw(Canvas, FVector2D(MX, MY));
		DrawLootTicker();
		DrawNavbar(0, FVector2D(MX, MY));
		return;
	}
	if (Mastery.IsOpen())
	{
		// With the Weapon Mastery screen open, the combat HUD goes away (as in the game).
		float MX = 0.f, MY = 0.f;
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->GetMousePosition(MX, MY);
		}
		Mastery.Draw(Canvas, FVector2D(MX, MY));
		FNWTradeskillMenu::DrawTabs(Canvas, FVector2D(MX, MY), 0);
		DrawNavbar(1, FVector2D(MX, MY));
		return;
	}
	if (Tradeskills.IsOpen())
	{
		float MX = 0.f, MY = 0.f;
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->GetMousePosition(MX, MY);
		}
		Tradeskills.Draw(Canvas, FVector2D(MX, MY));
		DrawNavbar(1, FVector2D(MX, MY));
		return;
	}
	if (Shop.IsOpen())
	{
		float MX = 0.f, MY = 0.f;
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->GetMousePosition(MX, MY);
			Shop.Hold(FVector2D(MX, MY), PC->IsInputKeyDown(EKeys::LeftMouseButton));
		}
		Shop.Draw(Canvas, FVector2D(MX, MY));
		return;
	}
	if (Refining.IsOpen())
	{
		float MX = 0.f, MY = 0.f;
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->GetMousePosition(MX, MY);
			if (Refining.IsOpen())
			{
				// Drag the bar and repeat −/+ while holding; Space is the game's tryskip action (jump already stops with a screen open).
				Refining.Hold(FVector2D(MX, MY), PC->IsInputKeyDown(EKeys::LeftMouseButton));
				if (PC->WasInputKeyJustPressed(EKeys::SpaceBar))
				{
					Refining.Space();
				}
			}
		}
		Refining.Draw(Canvas, FVector2D(MX, MY));
		return;
	}
	if (Attributes.IsOpen())
	{
		float MX = 0.f, MY = 0.f;
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->GetMousePosition(MX, MY);
		}
		Attributes.Draw(Canvas, FVector2D(MX, MY));
		FNWTradeskillMenu::DrawTabs(Canvas, FVector2D(MX, MY), 2);
		DrawNavbar(1, FVector2D(MX, MY));
		return;
	}
	DrawCombatHUD(Me, Dt);
	if (bShowDiagnostics || CVarHudDebug.GetValueOnGameThread())
	{
		DrawDiagnostics(Me);
	}
}

void ANWHUD::DrawCombatHUD(const ANWCharacter* Me, float Dt)
{
	const UNWCombatComponent* C = Me->Combat;
	if (Layout.IsLoaded())
	{
		if (!bStaticDone)
		{
			SetupStatic();
		}
		UpdateVitals(C, Dt);
		UpdateAbilities(Me, Dt);
		UpdateConsumables(Me);
		UpdateXp(Me);
		Layout.Draw(Canvas);
	}

	// Heavy charge, below the reticle: normal heavy mark; full = charged.
	if (C->IsCharging())
	{
		const float S = Canvas->ClipY / 1080.f;
		const float CW = 120.f * S;
		const float CX = (Canvas->ClipX - CW) * 0.5f;
		const float CY = Canvas->ClipY * 0.5f + 60.f * S;
		const float Ratio = C->GetChargeRatio();
		DrawBar(CX, CY, CW, 5.f * S, Ratio, Ratio >= 1.f ? FLinearColor(FColor(237, 198, 62)) : FLinearColor(FColor(221, 221, 221)));
		DrawRect(FLinearColor::Black, CX + CW * (C->HeavyMinHold / C->HeavyFullCharge) - 1.f, CY - 2.f * S, 2.f, 9.f * S);
	}
	DrawCompass();
	DrawLevelUp();
	DrawWarning();
	DrawEncumbrance(Me);
	DrawLootTicker();
	if (ShotPawn)
	{
		return;   // offline capture: no world to project names and cards onto
	}
	DrawHitIndicators();
	DrawDamageNumbers();
	DrawNameplates(Me);
	DrawGroup(Me);
	DrawNpcNames(Me);
	DrawGather(Me);
	DrawInteract(Me);
	DrawTarget(Me);
	Chat.Draw(Canvas);
}

float ANWHUD::ViewYaw() const
{
	const APlayerController* PC = GetOwningPlayerController();
	return ShotPawn ? ShotYaw : PC ? PC->GetControlRotation().Yaw : 0.f;
}

void ANWHUD::DrawShot(UCanvas* InCanvas, const ANWCharacter* Who, float CameraYaw, bool bInCombat, const FString& MapName)
{
	bShotCombat = bInCombat;
	ShotMap = MapName;
	if (!Layout.IsLoaded())
	{
		InitLayouts();
	}
	Canvas = InCanvas;
	ShotPawn = Who;
	ShotYaw = CameraYaw;
	DrawCombatHUD(Who, 0.f);
	Canvas = nullptr;
}

void ANWHUD::DrawGather(const APawn* Me)
{
	// The game's gathering card (markers/unifiedinteract, Gatherable_UnifiedInteractCard): ring on the object, name on a
	// ribbon to the right and the line with the key and action; during gathering the progress ring fills with the time inside.
	const ANWCharacter* Char = Cast<ANWCharacter>(Me);
	const UNWGatherComponent* G = Char ? Char->Gather.Get() : nullptr;
	UFont* Font = Fonts.IsValidIndex(1) ? Fonts[1].Get() : nullptr;
	APlayerController* PC = GetOwningPlayerController();
	FVector World;
	if (!G || !Font || !PC || !G->GetFocusLocation(World))
	{
		return;
	}
	if (!G->IsGathering() && Char->Interactor && Char->Interactor->Focus.IsValid())
	{
		return;   // the interactable's card (DrawInteract) takes over; the key also goes to it
	}
	FVector2D C;
	World += FVector(0.f, 0.f, 90.f);
	if (!PC->ProjectWorldLocationToScreen(World, C, true))
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.f;
	auto Tex = [&](const TCHAR* Path, const FVector2D& Center, float Size, const FLinearColor& Color, bool bAdditive = false)
	{
		if (UTexture2D* T = FNWUILayout::GameTexture(Path))
		{
			FCanvasTileItem Item(Center - FVector2D(Size * 0.5f), T->GetResource(), FVector2D(Size), Color);
			Item.BlendMode = bAdditive ? SE_BLEND_Additive : SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
	};
	const FLinearColor Tan(FColor(0xEB, 0xE3, 0xD6));
	Tex(TEXT("lyshineui/images/markers/marker_ringbackground.dds"), C, 256.f * S * 0.6f, FLinearColor(1.f, 1.f, 1.f, 0.7f));
	Tex(TEXT("lyshineui/images/markers/marker_ringgather.dds"), C, 64.f * S, G->bFocusBlocked ? FLinearColor(1.f, 0.45f, 0.4f) : FLinearColor::White);
	if (G->IsGathering())
	{
		// Progress ring: fan of triangles with the ring texture, from the top clockwise.
		if (UTexture2D* Ring = FNWUILayout::GameTexture(TEXT("lyshineui/images/markers/marker_ringgatherprogress.dds")))
		{
			const float R = 40.f * S, P = G->Progress();
			const int32 Segs = FMath::Max(1, FMath::CeilToInt(48 * P));
			TArray<FCanvasUVTri> Tris;
			for (int32 i = 0; i < Segs; ++i)
			{
				const float A0 = -HALF_PI + TWO_PI * P * i / Segs, A1 = -HALF_PI + TWO_PI * P * (i + 1) / Segs;
				FCanvasUVTri T;
				T.V0_Pos = C; T.V0_UV = FVector2D(0.5f, 0.5f);
				T.V1_Pos = C + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R; T.V1_UV = FVector2D(0.5f + 0.5f * FMath::Cos(A0), 0.5f + 0.5f * FMath::Sin(A0));
				T.V2_Pos = C + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R; T.V2_UV = FVector2D(0.5f + 0.5f * FMath::Cos(A1), 0.5f + 0.5f * FMath::Sin(A1));
				T.V0_Color = T.V1_Color = T.V2_Color = FLinearColor::White;
				Tris.Add(T);
			}
			FCanvasTriangleItem Item(Tris, Ring->GetResource());
			Item.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
		const FString Time = FString::Printf(TEXT("%.1fs"), G->Remaining());
		const float Scale = 15.f * S / FMath::Max(1, Font->LegacyFontSize);
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Font, Time, W, H, Scale, Scale);
		DrawText(Time, Tan, C.X - W * 0.5f, C.Y - H * 0.5f, Font, Scale);
		return;
	}
	if (G->FocusText.IsEmpty())
	{
		return;
	}
	// Action line: [E] Mine, or the warning in red (Requires Pickaxe).
	const FString Name = G->FocusName;
	const FString Action = G->FocusText.Contains(TEXT("·")) ? G->FocusText.Mid(G->FocusText.Find(TEXT("·")) + 1).TrimStartAndEnd()
		: G->FocusText.Left(G->FocusText.Len() - Name.Len()).TrimStartAndEnd();
	DrawCardText(C, Name, Action, G->bFocusBlocked);
}

void ANWHUD::DrawCardText(const FVector2D& C, const FString& Name, const FString& Action, bool bBlocked)
{
	UFont* Font = Fonts.IsValidIndex(1) ? Fonts[1].Get() : nullptr;
	if (!Font)
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.f;
	const FLinearColor Tan(FColor(0xEB, 0xE3, 0xD6));
	// Name ribbon to the right of the ring (TitleFrame: +185 from the card anchor; here attached to the ring) and the key line.
	const float NameScale = 19.f * S / FMath::Max(1, Font->LegacyFontSize);
	float NW = 0.f, NH = 0.f;
	Canvas->TextSize(Font, Name, NW, NH, NameScale, NameScale);
	const FVector2D TitlePos(C.X + 44.f * S, C.Y - 40.f * S);
	if (UTexture2D* Bg = FNWUILayout::GameTexture(TEXT("lyshineui/images/hud/unifiedinteractcard/unifiedinteractcardtextbg.dds")))
	{
		FCanvasTileItem Item(TitlePos, Bg->GetResource(), FVector2D(FMath::Max(250.f * S, NW + 40.f * S), 38.f * S), FLinearColor::White);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}
	DrawText(Name, Tan, TitlePos.X + 15.f * S, TitlePos.Y + (38.f * S - NH) * 0.5f, Font, NameScale);
	const float RowY = TitlePos.Y + 46.f * S;
	float X = TitlePos.X + 4.f * S;
	const float ActScale = 17.f * S / FMath::Max(1, Font->LegacyFontSize);
	if (!bBlocked)
	{
		const float Key = 30.f * S;
		if (UTexture2D* Frame = FNWUILayout::GameTexture(TEXT("lyshineui/images/slices/hint/hintframefilled.dds")))
		{
			FCanvasTileItem Item(FVector2D(X, RowY), Frame->GetResource(), FVector2D(Key), FLinearColor::White);
			Item.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
		float KW = 0.f, KH = 0.f;
		Canvas->TextSize(Font, TEXT("E"), KW, KH, ActScale, ActScale);
		DrawText(TEXT("E"), FLinearColor(0.05f, 0.05f, 0.05f), X + (Key - KW) * 0.5f, RowY + (Key - KH) * 0.5f, Font, ActScale);
		X += Key + 10.f * S;
	}
	float AW = 0.f, AH = 0.f;
	Canvas->TextSize(Font, Action, AW, AH, ActScale, ActScale);
	DrawText(Action, bBlocked ? FLinearColor(FColor(0xFF, 0x71, 0x71)) : Tan, X, RowY + (30.f * S - AH) * 0.5f, Font, ActScale);
}

void ANWHUD::DrawInteract(const APawn* Me)
{
	// Same card as gathering (ring + name ribbon + [E] action) at the interactable's focus point.
	const ANWCharacter* Char = Cast<ANWCharacter>(Me);
	const UNWInteractableComponent* I = Char && Char->Interactor ? Char->Interactor->Focus.Get() : nullptr;
	APlayerController* PC = GetOwningPlayerController();
	FVector2D C;
	if (!I || !PC || !PC->ProjectWorldLocationToScreen(I->FocusLocation() + FVector(0.f, 0.f, 90.f), C, true))
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.f;
	for (const TCHAR* Path : {TEXT("lyshineui/images/markers/marker_ringbackground.dds"), TEXT("lyshineui/images/markers/marker_ringgather.dds")})
	{
		if (UTexture2D* T = FNWUILayout::GameTexture(Path))
		{
			const float Size = FCString::Strstr(Path, TEXT("background")) ? 256.f * S * 0.6f : 64.f * S;
			FCanvasTileItem Item(C - FVector2D(Size * 0.5f), T->GetResource(), FVector2D(Size),
				FCString::Strstr(Path, TEXT("background")) ? FLinearColor(1.f, 1.f, 1.f, 0.7f) : FLinearColor::White);
			Item.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
	}
	// The week's drawn NPC with something to claim: "Claim" (ui_claim) instead of "Talk".
	FString Action = NWText::Get(I->ActionKey);
	if (const ANWNpc* Npc = Cast<ANWNpc>(I->GetOwner());
		Npc && FNWNpcRewards::Get().StateFor(Npc, Char->Interactor->RewardClaims) == FNWNpcRewards::EState::Available)
	{
		Action = NWText::Get(TEXT("ui_claim"));
	}
	DrawCardText(C, I->DisplayName.ToString(), Action, false);
}

void ANWHUD::DrawNpcNames(const APawn* Me)
{
	// The game's marker over the NPC (markerdata.luac, Conversation type; staticworldmarker.luac): no name, only the
	// state icon. In the port only the week's drawn NPC has a state (gifts, NWNpcRewards.h): "!" with something to claim,
	// clock afterwards. Height 1.9 m, scale from 1 to 0.35 up to 40 m, fades from 125 to 250 m, shows through walls.
	APlayerController* PC = GetOwningPlayerController();
	const ANWCharacter* Char = Cast<ANWCharacter>(Me);
	if (!PC || !Char || !Char->Interactor)
	{
		return;
	}
	const float UIScale = Canvas->ClipY / 1080.f;
	for (TActorIterator<ANWNpc> It(GetWorld()); It; ++It)
	{
		const ANWNpc* Npc = *It;
		const FNWNpcRewards::EState State = FNWNpcRewards::Get().StateFor(Npc, Char->Interactor->RewardClaims);
		// Merchant (tools/shops.json): eConversationState_HasShop, icon_npcshop (staticworldmarker.lua).
		const bool bShop = FNWShops::Get().ForNpc(Npc->Variant) != nullptr;
		if (State == FNWNpcRewards::EState::None && !bShop)
		{
			continue;
		}
		const FVector Foot = Npc->GetActorLocation() - FVector(0.f, 0.f, 86.f);
		const float Dist = FVector::Dist(Me->GetActorLocation(), Foot) / 100.f;
		FVector2D Screen;
		if (Dist > 250.f || !PC->ProjectWorldLocationToScreen(Foot + FVector(0.f, 0.f, 190.f), Screen, true))
		{
			continue;
		}
		UTexture2D* T = bShop ? FNWUILayout::GameTexture(TEXT("lyshineui/images/icons/objectives/icon_npcshop.dds"))
			: FNWUILayout::GameTexture(State == FNWNpcRewards::EState::Available
			? TEXT("lyshineui/images/icons/objectives/icon_questavailable_npc.dds")
			: TEXT("lyshineui/images/icons/objectives/icon_questcooldown_npc.dds"));
		if (!T)
		{
			continue;
		}
		const float Scale = FMath::GetMappedRangeValueClamped(FVector2D(0.f, 40.f), FVector2D(1.f, 0.35f), Dist);
		const float Alpha = FMath::GetMappedRangeValueClamped(FVector2D(125.f, 250.f), FVector2D(1.f, 0.f), Dist);
		const float Size = 160.f * 0.5f * UIScale * Scale;   // 160 px on the game's canvas (staticworldmarker), half scale
		FCanvasTileItem Item(Screen - FVector2D(Size * 0.5f, Size), T->GetResource(), FVector2D(Size), FLinearColor(1.f, 1.f, 1.f, Alpha));
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}
}

void ANWHUD::DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X - 2.f, Y - 2.f, W + 4.f, H + 4.f);
	DrawRect(FLinearColor(0.15f, 0.15f, 0.15f, 0.8f), X, Y, W, H);
	DrawRect(Fill, X, Y, W * FMath::Clamp(Fraction, 0.f, 1.f), H);
}

void ANWHUD::DrawTarget(const APawn* Me)
{
	const UNWCombatComponent* T = FindTarget(Me);
	if (!T)
	{
		return;
	}
	const float TW = 340.f * Canvas->ClipY / 1080.f;
	const float TX = (Canvas->ClipX - TW) * 0.5f;
	const FString Name = DisplayName(T->GetOwner());
	UFont* Font = Fonts.IsValidIndex(1) ? Fonts[1].Get() : nullptr;
	const float S = Font ? 18.f * Canvas->ClipY / 1080.f / FMath::Max(1, Font->LegacyFontSize) : 1.f;
	float W = 0.f, H = 0.f;
	if (Font)
	{
		Canvas->TextSize(Font, Name, W, H, S, S);
	}
	// Below the compass (its line and the town name end at ~80 px of the 1080p screen).
	const float U = Canvas->ClipY / 1080.f;
	DrawText(Name, FLinearColor(FColor(0xEB, 0xE3, 0xD6)), (Canvas->ClipX - W) * 0.5f, 84.f * U, Font, S);
	DrawBar(TX, 88.f * U + H, TW, 8.f * U, T->Health / T->MaxHealth, FLinearColor(FColor(180, 47, 14)));
}

void ANWHUD::SetDeathScreen(bool bOn)
{
	bDeathScreen = bOn;
	if (bOn)
	{
		if (const APawn* Me = GetOwningPawn())
		{
			WorldMap.EnsureLoaded();
			WorldMap.LastDeath = WorldMap.ToWorld(Me->GetActorLocation());
		}
	}
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}
	if (bOn)
	{
		Mastery.Close();
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
		PC->SetIgnoreLookInput(true);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
		PC->ResetIgnoreLookInput();
	}
}

FString ANWHUD::DisplayName(const AActor* Actor)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	if (const APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr)
	{
		return PS->GetPlayerName();
	}
	if (Actor && Actor->IsA<ANWLost>())
	{
		return TEXT("Lost");
	}
	if (const ANWCreature* Creature = Cast<ANWCreature>(Actor); Creature && Creature->Def())
	{
		return NWText::Get(Creature->Def()->NameKey);   // the game's name ("Javali", "Lobo Grisalho")
	}
	if (Actor && Actor->IsA<ANWCharacter>() && !Actor->IsA<ANWLost>())
	{
		return NWText::Get(TEXT("nw.dummy"));
	}
#if WITH_EDITOR
	return Actor ? Actor->GetActorLabel(false) : FString();
#else
	return Actor ? Actor->GetName() : FString();
#endif
}

// Name and health over the other characters up to 40 m, smaller and fainter with distance.
void ANWHUD::DrawNameplates(const APawn* Me)
{
	UFont* Font = Fonts.IsValidIndex(1) ? Fonts[1].Get() : nullptr;
	APlayerController* PC = GetOwningPlayerController();
	if (!Font || !PC || !Me)
	{
		return;
	}
	const float UIScale = Canvas->ClipY / 1080.f;
	const FVector Eye = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : Me->GetActorLocation();
	for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
	{
		const ANWCharacter* Other = *It;
		if (Other == Me || Other->Combat->State == ENWCombatState::Dead)
		{
			continue;
		}
		const float Dist = FVector::Dist(Eye, Other->GetActorLocation());
		if (Dist > 4000.f)
		{
			continue;
		}
		const FVector Head = Other->GetActorLocation() + FVector(0.f, 0.f, Other->GetSimpleCollisionHalfHeight() + 10.f);
		FVector2D Screen;
		if (!PC->ProjectWorldLocationToScreen(Head, Screen, true))
		{
			continue;
		}
		const float Near = FMath::GetMappedRangeValueClamped(FVector2D(800.f, 4000.f), FVector2D(1.f, 0.55f), Dist);
		const float Alpha = FMath::GetMappedRangeValueClamped(FVector2D(3000.f, 4000.f), FVector2D(1.f, 0.f), Dist);
		const bool bPlayer = Other->GetPlayerState() != nullptr;
		const ANWPlayerState* MyPS = Me->GetPlayerState<ANWPlayerState>();
		const bool bAlly = MyPS && MyPS->IsGroupedWith(Other->GetPlayerState<ANWPlayerState>());
		const FString Name = DisplayName(Other);
		const float S = 17.f * UIScale * Near / FMath::Max(1, Font->LegacyFontSize);
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Font, Name, W, H, S, S);
		const FLinearColor NameColor = bAlly ? FLinearColor(0.55f, 0.75f, 1.f, Alpha)
			: bPlayer ? FLinearColor(0.93f, 0.88f, 0.78f, Alpha) : FLinearColor(0.95f, 0.45f, 0.38f, Alpha);
		const float BarW = 90.f * UIScale * Near, BarH = 5.f * UIScale * Near;
		DrawText(Name, NameColor, Screen.X - W * 0.5f, Screen.Y - H - BarH - 3.f, Font, S);
		DrawBar(Screen.X - BarW * 0.5f, Screen.Y - BarH, BarW, BarH, Other->Combat->Health / FMath::Max(1.f, Other->Combat->MaxHealth),
			FLinearColor(0.70f, 0.13f, 0.10f, Alpha));
	}
}

void ANWHUD::DrawGroup(const APawn* Me)
{
	const ANWPlayerState* MyPS = Me ? Me->GetPlayerState<ANWPlayerState>() : nullptr;
	const AGameStateBase* GS = GetWorld()->GetGameState();
	FNWUIElement* Root = GroupLayout.FindOne(TEXT("member"), TEXT("GroupHealthBarSlice1"));
	if (!MyPS || MyPS->GroupId == 0 || !GS || !Root)
	{
		return;
	}
	TArray<const ANWPlayerState*> Members;
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const ANWPlayerState* N = Cast<ANWPlayerState>(PS);
		if (N && N != MyPS && N->GroupId == MyPS->GroupId)
		{
			Members.Add(N);
		}
	}
	Members.Sort([](const ANWPlayerState& A, const ANWPlayerState& B) { return A.bGroupLeader > B.bGroupLeader; });
	FNWUIElement* Name = GroupLayout.FindOne(TEXT("member"), TEXT("Name Container/Name"));
	FNWUIElement* Fill = GroupLayout.FindOne(TEXT("member"), TEXT("Health/Fill"));
	FNWUIElement* Dead = GroupLayout.FindOne(TEXT("member"), TEXT("SmallDeathIcon"));
	static const FLinearColor LeaderGold(0.93f, 0.78f, 0.25f);
	for (int32 i = 0; i < Members.Num(); ++i)
	{
		const ANWPlayerState* M = Members[i];
		const ANWCharacter* Pawn = Cast<ANWCharacter>(M->GetPawn());
		const bool bDead = Pawn && Pawn->Combat->State == ENWCombatState::Dead;
		Root->Translate = FVector2f(16.f, 230.f + i * 58.f);
		if (Name)
		{
			Name->TextOverride = M->GetPlayerName();
			Name->Tint = M->bGroupLeader ? TOptional<FLinearColor>(LeaderGold) : TOptional<FLinearColor>();
		}
		if (Fill)
		{
			Fill->Fill = Pawn ? Pawn->Combat->Health / FMath::Max(1.f, Pawn->Combat->MaxHealth) : 1.f;
		}
		if (Dead)
		{
			Dead->Visible = bDead;
		}
		GroupLayout.DrawOne(Canvas, TEXT("member"));
	}
}

void ANWHUD::UpdateConsumables(const ANWCharacter* Me)
{
	// Quick slots 3-6: the consumables equipped in the inventory (NWEquip::Quick1..4); the count is the whole inventory's,
	// as the slot refills from the bag.
	const UNWInventoryComponent* Inv = Me->Inventory;
	static const TCHAR* Roots[NWEquip::QuickCount] = {TEXT("ConsumableQuickslot/"), TEXT("ConsumableQuickslot2/"),
		TEXT("ConsumableQuickslot3/"), TEXT("ConsumableQuickslot4/")};
	for (int32 i = 0; i < NWEquip::QuickCount; ++i)
	{
		const FString Root = Roots[i];
		const FNWItem* Item = Inv ? Inv->At(ENWItemContainer::Equip, NWEquip::Quick1 + i) : nullptr;
		const FNWItemDef* Def = Item ? FNWItemCatalog::Get().Find(Item->ItemId) : nullptr;
		if (FNWUIElement* L = Layout.FindOne(ConsumableCanvas, Root + TEXT("ItemLayoutConsumable"))) L->Visible = Def != nullptr;
		const float Cd = Def && Def->Heal > 0.f ? Inv->GetPotionCooldown() : 0.f;
		if (FNWUIElement* T = Layout.FindOne(ConsumableCanvas, Root + TEXT("CooldownTimer"))) T->Visible = Cd > 0.f;
		if (!Def)
		{
			continue;
		}
		if (FNWUIElement* I = Layout.FindOne(ConsumableCanvas, Root + TEXT("ItemLayoutConsumable/ItemIcon"))) I->TextureOverride = Def->Icon;
		if (FNWUIElement* R = Layout.FindOne(ConsumableCanvas, Root + TEXT("ItemLayoutConsumable/ItemRarityBg"))) R->Visible = false;
		if (FNWUIElement* T = Layout.FindOne(ConsumableCanvas, Root + TEXT("ItemLayoutConsumable/ItemTier"))) T->TextOverride = FString::FromInt(Def->Tier);
		if (FNWUIElement* Q = Layout.FindOne(ConsumableCanvas, Root + TEXT("ItemLayoutConsumable/ItemQuantity"))) Q->TextOverride = FString::FromInt(Inv->CountOf(Item->ItemId));
		if (FNWUIElement* A = Layout.FindOne(ConsumableCanvas, Root + TEXT("ItemLayoutConsumable/ItemAnimatedIndicator"))) A->Visible = false;
		if (FNWUIElement* D = Layout.FindOne(ConsumableCanvas, Root + TEXT("ItemLayoutConsumable/ItemDimmer"))) D->Visible = false;
		const float Total = FMath::Max(1.f, Def->Cooldown);
		if (FNWUIElement* B = Layout.FindOne(ConsumableCanvas, Root + TEXT("CooldownTimer/BarFill")))
		{
			B->Fill = Cd / Total;
			B->Tint = FLinearColor(FColor(255, 153, 0)).CopyWithNewOpacity(0.8f);
		}
		if (FNWUIElement* X = Layout.FindOne(ConsumableCanvas, Root + TEXT("CooldownTimer/TimerText"))) X->TextOverride = FString::FromInt(FMath::CeilToInt(Cd));
	}
}

void ANWHUD::ShowLoot(const TArray<FNWItem>& Gained, int64 Coins)
{
	const double Now = FPlatformTime::Seconds();
	if (Coins > 0)
	{
		LootLines.Add({NAME_None, 0, 0, Coins, Now});
	}
	for (const FNWItem& I : Gained)
	{
		if (FNWItemCatalog::Get().Find(I.ItemId))
		{
			LootLines.Add({I.ItemId, I.Quantity, I.Rarity, 0, Now});
		}
	}
	// At most 5 lines, like the game (the rest becomes "more items"; here the oldest leave).
	while (LootLines.Num() > 5)
	{
		LootLines.RemoveAt(0);
	}
}

void ANWHUD::ShowTradeXp(uint8 Skill, int32 Amount)
{
	FLootLine L;
	L.Quantity = Amount;
	L.Trade = Skill;
	L.Time = FPlatformTime::Seconds();
	LootLines.Add(L);
	while (LootLines.Num() > 5)
	{
		LootLines.RemoveAt(0);
	}
}

void ANWHUD::DrawLootTicker()
{
	const double Now = FPlatformTime::Seconds();
	LootLines.RemoveAll([Now](const FLootLine& L) { return Now - L.Time > 6.0; });
	UFont* Font = Fonts.IsValidIndex(1) ? Fonts[1].Get() : nullptr;
	if (!Font || LootLines.Num() == 0 || !Canvas)
	{
		return;
	}
	// LootTicker2024: bottom-right corner (x from -802, y from -490 of the 1080p screen), 58 px lines, newest on top.
	const float S = Canvas->ClipY / 1080.f;
	const FVector2D O(Canvas->ClipX - 802.f * S, Canvas->ClipY - 490.f * S);
	auto Tex = [&](const FString& Path, FVector2D Pos, FVector2D Size, float Alpha, FLinearColor Tint = FLinearColor::White)
	{
		if (UTexture2D* T = FNWUILayout::GameTexture(Path))
		{
			FCanvasTileItem Item(Pos, T->GetResource(), Size, Tint.CopyWithNewOpacity(Tint.A * Alpha));
			Item.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
	};
	// Draws and returns the right edge of the text (the next text starts after it).
	auto Text = [&](const FString& Str, float X, float Y, float Size, FLinearColor Color, float Alpha)
	{
		const float Scale = Size * S / FMath::Max(1, Font->LegacyFontSize);
		DrawText(Str, Color.CopyWithNewOpacity(Alpha), X, Y, Font, Scale);
		float TW = 0.f, TH = 0.f;
		Canvas->TextSize(Font, Str, TW, TH, Scale, Scale);
		return X + TW;
	};
	float MaxAlpha = 0.f;
	for (const FLootLine& L : LootLines)
	{
		MaxAlpha = FMath::Max(MaxAlpha, FMath::Clamp(float(6.0 - (Now - L.Time)) / 1.5f, 0.f, 1.f));
	}
	// Background only behind the lines there are (a fixed 310 px block looked like an empty panel).
	const float BgH = 24.f + FMath::Min(LootLines.Num(), 5) * 58.f;
	Tex(TEXT("lyshineui/images/hud2024/lootticker/loottickerbg.dds"), O + FVector2D(28.f, -6.f) * S, FVector2D(420.f, BgH) * S, 0.6f * MaxAlpha);
	const FLinearColor Tan(FColor(0xEB, 0xE3, 0xD6));
	for (int32 i = 0; i < LootLines.Num(); ++i)
	{
		const FLootLine& L = LootLines[LootLines.Num() - 1 - i];
		const float Alpha = FMath::Clamp(float(6.0 - (Now - L.Time)) / 1.5f, 0.f, 1.f);
		const FVector2D Row = O + FVector2D(0.f, 12.f + i * 58.f) * S;
		Tex(TEXT("lyshineui/images/lootticker/progressiontickeritembg.dds"), Row + FVector2D(2.f, -12.f) * S, FVector2D(385.f, 68.f) * S, Alpha);
		if (L.Trade >= 0)
		{
			// Trade skill progress (the game's progressiontickeritembg): icon, "+N", name and the level with the bar.
			const ANWCharacter* Me = Cast<ANWCharacter>(GetOwningPawn());
			const UNWProgressionComponent* P = Me ? Me->Progression.Get() : nullptr;
			Tex(NWTradeskill::Icon(L.Trade), Row + FVector2D(52.f, 8.f) * S, FVector2D(42.f) * S, Alpha);
			Text(FString::Printf(TEXT("+%d"), L.Quantity), Row.X + 107.f * S, Row.Y + 8.f * S, 19.f, FLinearColor(FColor(0xED, 0xC6, 0x3E)), Alpha);
			Text(NWText::Get(NWTradeskill::TextKey(L.Trade)), Row.X + 175.f * S, Row.Y + 8.f * S, 18.f, Tan, Alpha);
			if (P)
			{
				Text(FString::FromInt(P->TradeLevel(L.Trade)), Row.X + 107.f * S, Row.Y + 30.f * S, 15.f, Tan, Alpha);
				const FVector2D Bar = Row + FVector2D(175.f, 38.f) * S;
				FCanvasTileItem Back(Bar, FVector2D(190.f, 4.f) * S, FLinearColor(1.f, 1.f, 1.f, 0.15f * Alpha));
				Back.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Back);
				FCanvasTileItem Fill(Bar, FVector2D(190.f * P->TradeProgress(L.Trade), 4.f) * S, FLinearColor(0.93f, 0.78f, 0.24f, Alpha));
				Fill.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Fill);
			}
			continue;
		}
		if (L.Coins > 0)
		{
			Tex(TEXT("lyshineui/images/icons/objectives/reward_coin_cover.dds"), Row + FVector2D(52.f, 8.f) * S, FVector2D(42.f) * S, Alpha);
			const float End = Text(TEXT("+") + UNWInventoryComponent::FormatCoins(L.Coins), Row.X + 107.f * S, Row.Y + 14.f * S, 19.f, FLinearColor(FColor(0xED, 0xC6, 0x3E)), Alpha);
			Text(NWText::Get(TEXT("nw.inv.coins")), FMath::Max(Row.X + 175.f * S, End + 8.f * S), Row.Y + 14.f * S, 18.f, Tan, Alpha);
			continue;
		}
		const FNWItemDef* D = FNWItemCatalog::Get().Find(L.ItemId);
		if (!D)
		{
			continue;
		}
		// Item piece (ItemLayoutSimple) reduced to 92 x 46: background, rarity ribbon, icon and tier.
		const int32 Rarity = D->Type == TEXT("weapon") || D->Type == TEXT("armor") ? FMath::Min<int32>(L.Rarity, 4) : FMath::Clamp(D->Tier - 1, 0, 4);
		const FVector2D Tile = Row + FVector2D(10.f, 6.f) * S;
		const FVector2D TileSize = FVector2D(92.f, 46.f) * S;
		Tex(TEXT("lyshineui/images/lootticker/lootticker_itembg_square.dds"), Tile - FVector2D(3.f) * S, TileSize + FVector2D(6.f) * S, Alpha);
		Tex(FString::Printf(TEXT("lyshineui/images/slices/itemlayout/itemraritybglarge%d.dds"), Rarity), Tile, TileSize, Alpha);
		Tex(D->Icon, Tile + FVector2D((TileSize.X - TileSize.Y) * 0.5f, 0.f), FVector2D(TileSize.Y), Alpha);
		static const TCHAR* Roman[] = {TEXT(""), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V")};
		Text(Roman[FMath::Clamp(D->Tier, 0, 5)], Tile.X + 4.f * S, Tile.Y, 13.f, Tan, Alpha);
		// "+N" and the name on the right (ItemQuantity / ItemName), name in the weapons' rarity color.
		const float End = Text(FString::Printf(TEXT("+%d"), L.Quantity), Row.X + 112.f * S, Row.Y + 16.f * S, 18.f, FLinearColor(FColor(0xED, 0xC6, 0x3E)), Alpha);
		Text(D->Name, FMath::Max(Row.X + 160.f * S, End + 8.f * S), Row.Y + 16.f * S, 18.f, D->Type == TEXT("weapon") || D->Type == TEXT("armor") ? NWRarityColor(L.Rarity) : Tan, Alpha);
	}
}

void ANWHUD::DrawDiagnostics(const APawn* Me)
{
	const ANWCharacter* Char = Cast<ANWCharacter>(Me);
	const UNWAnimInstance* Anim = Char ? Cast<UNWAnimInstance>(Char->GetMesh()->GetAnimInstance()) : nullptr;
	if (!Anim)
	{
		return;
	}
	const UEnum* StateEnum = StaticEnum<ENWCombatState>();
	const float CamYaw = GetOwningPlayerController() ? GetOwningPlayerController()->GetControlRotation().Yaw : 0.f;
	DrawText(FString::Printf(TEXT("vel %.0f  dir %.0f  giro %.1f  sprint %d  corpo %.0f  camera %.0f  estado %s  montagem %s"),
		Anim->Speed, Anim->Direction, Anim->TurnRate, Anim->bSprinting ? 1 : 0,
		Me->GetActorRotation().Yaw, FRotator::NormalizeAxis(CamYaw),
		*StateEnum->GetNameStringByValue(int64(Char->Combat->State)),
		Anim->GetCurrentActiveMontage() ? *Anim->GetCurrentActiveMontage()->GetName() : TEXT("-")),
		FLinearColor(0.7f, 0.9f, 1.f), 20.f, 20.f, nullptr, 0.9f);
}

const UNWCombatComponent* ANWHUD::FindTarget(const APawn* Self) const
{
	const UNWCombatComponent* Best = nullptr;
	float BestDot = 0.85f;
	const FVector Eye = Self->GetActorLocation();
	const FVector Fwd = GetOwningPlayerController()->GetControlRotation().Vector();
	for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == Self || It->Combat->State == ENWCombatState::Dead)
		{
			continue;
		}
		const FVector To = It->GetActorLocation() - Eye;
		if (To.Size() > TargetRange)
		{
			continue;
		}
		const float Dot = FVector::DotProduct(Fwd.GetSafeNormal2D(), To.GetSafeNormal2D());
		if (Dot > BestDot)
		{
			BestDot = Dot;
			Best = It->Combat;
		}
	}
	return Best;
}

void ANWHUD::DrawCompass()
{
	APlayerController* PC = GetOwningPlayerController();
	UTexture2D* Strip = FNWUILayout::GameTexture(TEXT("lyshineui/images/compass/compass2048.dds"));
	const APawn* ViewPawn = ShotPawn ? ShotPawn : PC ? PC->GetPawn() : nullptr;
	if (!ViewPawn || !Strip || !Canvas)
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.f;
	const float CX = Canvas->ClipX * 0.5f;
	// Camera direction in compass degrees (0 = north, 90 = east), -180..180.
	const float Heading = FRotator::NormalizeAxis(ViewYaw() - CompassNorthYaw);
	// On the strip: N at 1021.6 px, 4.592 px per degree; covers from -180° (S on the left) to +222°. 1:1 scale on the 1080p screen.
	constexpr float PxPerDeg = 4.592f, NorthPx = 1021.6f, TexW = 2048.f, Half = 386.f / PxPerDeg;
	const float Y0 = 3.f * S, H = 64.f * S;
	auto Piece = [&](float From, float To, float ScreenFrom)
	{
		// Slices with alpha that fades at the ends (the game's compassmask).
		constexpr int32 Slices = 24;
		const float Step = (To - From) / Slices;
		for (int32 i = 0; i < Slices; ++i)
		{
			const float A = From + i * Step;
			const float SX = CX + (ScreenFrom + (A - From)) * PxPerDeg * S;
			const float Edge = FMath::Abs((SX - CX) / (386.f * S));
			const float Alpha = FMath::Clamp((1.f - Edge) / 0.25f, 0.f, 1.f);
			FCanvasTileItem T(FVector2D(SX, Y0), Strip->GetResource(), FVector2D(Step * PxPerDeg * S, H),
				FVector2D((NorthPx + A * PxPerDeg) / TexW, 0.f), FVector2D((NorthPx + (A + Step) * PxPerDeg) / TexW, 1.f),
				FLinearColor(1.f, 1.f, 1.f, Alpha));
			T.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(T);
		}
	};
	const float Left = Heading - Half;
	if (Left >= -180.f && Heading + Half <= 222.f)
	{
		Piece(Left, Heading + Half, -Half);
	}
	else if (Left < -180.f)
	{
		// Went past south on the left: the piece before -180 comes from the end of the turn (+180).
		Piece(Left + 360.f, 180.f, -Half);
		Piece(-180.f, Heading + Half, -180.f - Heading);
	}
	else
	{
		Piece(Left, 180.f, -Half);
		Piece(-180.f, Heading + Half - 360.f, 180.f - Heading);
	}
	// POIs up to 400 m on the strip, at their bearing (the game shows nearby points on the compass).
	if (const APawn* Me = ViewPawn)
	{
		WorldMap.EnsureLoaded();
		const float IconSz = 28.f * S;
		for (const FNWWorldMap::FPoi& P : WorldMap.GetPois())
		{
			const FVector2D U = WorldMap.ToUnreal(P.Pos);
			const FVector2D D = U - FVector2D(Me->GetActorLocation());
			if (D.SizeSquared() > FMath::Square(40000.0))
			{
				continue;
			}
			const float Rel = FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)) - ViewYaw());
			if (FMath::Abs(Rel) > Half)
			{
				continue;
			}
			const float SX = CX + Rel * PxPerDeg * S;
			const float Alpha = FMath::Clamp((1.f - FMath::Abs(Rel) / Half) / 0.25f, 0.f, 1.f);
			if (UTexture2D* Icon = FNWUILayout::GameTexture(P.Icon))
			{
				FCanvasTileItem T(FVector2D(SX - IconSz * 0.5f, 14.f * S), Icon->GetResource(), FVector2D(IconSz), FLinearColor(1.f, 1.f, 1.f, Alpha));
				T.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(T);
			}
		}
	}
	// Map marker on the strip (waypoint_compass) with the distance below; pinned to the edge when it is behind.
	if (const APawn* Me = ViewPawn; Me && WorldMap.Waypoint)
	{
		const FVector2D D = WorldMap.ToUnreal(*WorldMap.Waypoint) - FVector2D(Me->GetActorLocation());
		const float Rel = FMath::Clamp(FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)) - ViewYaw()), -Half, Half);
		const float SX = CX + Rel * PxPerDeg * S;
		const float IconSz = 30.f * S;
		if (UTexture2D* Icon = FNWUILayout::GameTexture(TEXT("lyshineui/images/map/icon/waypoint_compass.dds")))
		{
			FCanvasTileItem T(FVector2D(SX - IconSz * 0.5f, 10.f * S), Icon->GetResource(), FVector2D(IconSz), FLinearColor::White);
			T.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(T);
		}
		if (UFont* Font = Fonts.IsValidIndex(1) ? Fonts[1].Get() : nullptr)
		{
			const FString Dist = FString::Printf(TEXT("%.0f m"), D.Size() / 100.f);
			const float Scale = 14.f * S / FMath::Max(1, Font->LegacyFontSize);
			float TW = 0.f, TH = 0.f;
			Canvas->TextSize(Font, Dist, TW, TH, Scale, Scale);
			DrawText(Dist, FLinearColor(FColor(0xEB, 0xE3, 0xD6)), SX - TW * 0.5f, 52.f * S, Font, Scale);
		}
	}
	// Base line and needle (CompassLines / Needle).
	if (UTexture2D* Line = FNWUILayout::GameTexture(TEXT("lyshineui/images/compass/compassbg.dds")))
	{
		FCanvasTileItem T(FVector2D(CX - 386.f * S, 45.f * S), Line->GetResource(), FVector2D(772.f * S, 8.f * S), FLinearColor(0.8f, 0.74f, 0.635f, 1.f));
		T.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(T);
	}
	if (UTexture2D* Needle = FNWUILayout::GameTexture(TEXT("lyshineui/images/compass/compassneedle.dds")))
	{
		FCanvasTileItem T(FVector2D(CX - 6.f * S, 41.f * S), Needle->GetResource(), FVector2D(12.f * S, 6.f * S), FLinearColor::White);
		T.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(T);
	}
	// Settlement name under the line while inside the town (the game's "CORINTH"): both city maps use the town's frame,
	// with the capital at the origin; 250 m around it.
	const FString MapName = ShotPawn ? ShotMap : UWorld::RemovePIEPrefix(GetWorld() ? GetWorld()->GetMapName() : FString());
	const TCHAR* Town = MapName.Contains(TEXT("SouthWindsward")) ? TEXT("windsward") : MapName.Contains(TEXT("QuedaEterna")) ? TEXT("everfall") : nullptr;
	if (UFont* Pica = FNWUILayout::GameFont(3); Town && Pica && FVector2D(ViewPawn->GetActorLocation()).Size() < 25000.0)
	{
		const FString Spaced = NWText::Get(Town).ToUpper();
		const float Scale = 15.f * S / FMath::Max(1, Pica->LegacyFontSize);
		float TW = 0.f, TH = 0.f;
		Canvas->TextSize(Pica, Spaced, TW, TH, Scale, Scale);
		FFontRenderInfo Shadow;
		Shadow.bEnableShadow = true;
		Canvas->SetDrawColor(FColor(0xEB, 0xE3, 0xD6));
		Canvas->DrawText(Pica, Spaced, CX - TW * 0.5f, 54.f * S, Scale, Scale, Shadow);
	}
}

void ANWHUD::AddDamageNumber(const FVector& World, const FString& Text, const FLinearColor& Color, bool bBig)
{
	// Small random lateral offset: consecutive hits do not land on top of each other.
	DamageNumbers.Add({World, Text, Color, bBig, FPlatformTime::Seconds(), FMath::FRandRange(-30.f, 30.f)});
	if (DamageNumbers.Num() > 40)
	{
		DamageNumbers.RemoveAt(0);
	}
}

void ANWHUD::DrawDamageNumbers()
{
	const double Now = FPlatformTime::Seconds();
	DamageNumbers.RemoveAll([Now](const FDamageNumber& N) { return Now - N.Time > 1.2; });
	UFont* Font = Fonts.IsValidIndex(2) ? Fonts[2].Get() : Fonts.IsValidIndex(1) ? Fonts[1].Get() : nullptr;
	APlayerController* PC = GetOwningPlayerController();
	if (!Font || !PC || !Canvas)
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.f;
	// Stacked (game option): fixed list to the right of the reticle, newest at the bottom; floating: over the target.
	const bool bStacked = FNWOptions::Get().DamageNumbers == 1;
	int32 Row = DamageNumbers.Num();
	for (const FDamageNumber& N : DamageNumbers)
	{
		--Row;
		FVector2D Screen;
		if (bStacked)
		{
			Screen = FVector2D(Canvas->ClipX * 0.5f + 110.f * S, Canvas->ClipY * 0.5f - Row * 30.f * S);
		}
		else if (!PC->ProjectWorldLocationToScreen(N.World, Screen, true))
		{
			continue;
		}
		const float T = float(Now - N.Time) / 1.2f;
		// Enters large and shrinks (0.15 s), rises 70 px, fades in the last third.
		const float Pop = 1.f + 0.35f * FMath::Clamp(1.f - T / 0.125f, 0.f, 1.f);
		const float Size = (N.bBig ? 30.f : 24.f) * Pop * S;
		const float Scale = Size / FMath::Max(1, Font->LegacyFontSize);
		const float Alpha = FMath::Clamp((1.f - T) / 0.33f, 0.f, 1.f);
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Font, N.Text, W, H, Scale, Scale);
		const FVector2D At = bStacked ? FVector2D(Screen.X, Screen.Y - H * 0.5f)
			: FVector2D(Screen.X + N.Drift * S - W * 0.5f, Screen.Y - 70.f * S * T - H * 0.5f);
		FCanvasTextItem Item(At, FText::FromString(N.Text), Font, N.Color.CopyWithNewOpacity(Alpha));
		Item.Scale = FVector2D(Scale, Scale);
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.9f * Alpha));
		Canvas->DrawItem(Item);
	}
}

void ANWHUD::DrawEncumbrance(const APawn* Me)
{
	const ANWCharacter* Char = Cast<ANWCharacter>(Me);
	const UNWInventoryComponent* Inv = Char ? Char->Inventory.Get() : nullptr;
	UFont* Font = Fonts.IsValidIndex(1) ? Fonts[1].Get() : nullptr;
	if (!Inv || !Font || !Canvas)
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.f;
	const float Ratio = Inv->TotalWeight() / Inv->MaxWeight;
	const bool bOver = Ratio > 1.f;
	// EncumbranceLayoutElement: 60 x 60 in the bottom-right corner (offsets -96..-36, -113..-53 of the 1080p screen).
	const FVector2D Slot(Canvas->ClipX - 96.f * S, Canvas->ClipY - 113.f * S);
	const float Size = 60.f * S;
	auto Tex = [&](const TCHAR* Path, FVector2D Pos, FVector2D Dim, FLinearColor Color)
	{
		if (UTexture2D* T = FNWUILayout::GameTexture(Path))
		{
			FCanvasTileItem Item(Pos, T->GetResource(), Dim, Color);
			Item.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
	};
	Tex(TEXT("lyshineui/images/hud2024/actionbar/actionbaritembg.dds"), Slot, FVector2D(Size), FLinearColor::White);
	// Bag icon (100 x 72) centered in the space.
	const FVector2D Icon(46.f * S, 33.f * S);
	Tex(TEXT("lyshineui/images/inventory/icon_inventory.dds"), Slot + (FVector2D(Size) - Icon) * 0.5, Icon,
		bOver ? FLinearColor(1.f, 0.45f, 0.4f) : FLinearColor(1.f, 1.f, 1.f, 0.9f));
	// Weight percentage in the circle (PercentageBg), from 80% on as a warning.
	if (Ratio >= 0.8f)
	{
		const FVector2D Circle(34.f * S);
		const FVector2D CPos = Slot + FVector2D(Size - 22.f * S, -12.f * S);
		Tex(TEXT("lyshineui/images/common/circle_64x64.dds"), CPos, Circle, FLinearColor(0.f, 0.f, 0.f, 0.85f));
		const FString Pct = FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Ratio * 100.f));
		const float Scale = 13.f * S / FMath::Max(1, Font->LegacyFontSize);
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Font, Pct, W, H, Scale, Scale);
		DrawText(Pct, bOver ? FLinearColor(FColor(0xFF, 0x71, 0x71)) : FLinearColor(FColor(0xEB, 0xE3, 0xD6)),
			CPos.X + (Circle.X - W) * 0.5f, CPos.Y + (Circle.Y - H) * 0.5f, Font, Scale);
	}
	if (bOver)
	{
		// Message ribbon on the left (Message: encumbrance_messagebgside) with the game's warning.
		const FString Msg = NWText::Get(TEXT("inv_equiploadencumberednotification"));
		const float Scale = 16.f * S / FMath::Max(1, Font->LegacyFontSize);
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Font, Msg, W, H, Scale, Scale);
		const FVector2D BgPos(Slot.X - W - 50.f * S, Slot.Y + 6.f * S);
		Tex(TEXT("lyshineui/images/encumbrance/encumbrance_messagebgside.dds"), BgPos, FVector2D(W + 46.f * S, 48.f * S), FLinearColor::White);
		DrawText(Msg, FLinearColor(FColor(0xFF, 0x9A, 0x8A)), BgPos.X + 18.f * S, BgPos.Y + (48.f * S - H) * 0.5f, Font, Scale);
	}
}

void ANWHUD::ShowXp(int64 Amount)
{
	const double Now = FPlatformTime::Seconds();
	// Consecutive gains add up in the same notice.
	XpShown = Now - XpShownAt < 2.5 ? XpShown + Amount : Amount;
	XpShownAt = Now;
}

void ANWHUD::UpdateXp(const ANWCharacter* Me)
{
	const UNWProgressionComponent* P = Me->Progression;
	if (!P)
	{
		return;
	}
	if (LastLevel > 0 && P->Level > LastLevel)
	{
		LevelUpValue = P->Level;
		LevelUpAt = FPlatformTime::Seconds();
	}
	LastLevel = P->Level;
	const float Fill = P->LevelProgress();
	if (FNWUIElement* E = Layout.FindOne(XpCanvas, TEXT("LevelText"))) E->TextOverride = FString::FromInt(P->Level);
	if (FNWUIElement* E = Layout.FindOne(XpCanvas, TEXT("MeterFill"))) E->Fill = Fill;
	// The tip follows the fill (in the game the Lua moves it; bar width = 322).
	if (FNWUIElement* E = Layout.FindOne(XpCanvas, TEXT("MeterFillEdge")))
	{
		E->Translate = FVector2f(322.f * Fill, 0.f);
		E->Visible = Fill > 0.f && Fill < 1.f;
	}
	const double Age = FPlatformTime::Seconds() - XpShownAt;
	const bool bDelta = Age < 2.5;
	if (FNWUIElement* E = Layout.FindOne(XpCanvas, TEXT("DeltaXpBg")))
	{
		E->Visible = bDelta;
		E->AlphaMul = FMath::Clamp(float(2.5 - Age) / 0.5f, 0.f, 1.f);
	}
	if (FNWUIElement* E = Layout.FindOne(XpCanvas, TEXT("DeltaXpText")))
	{
		E->TextOverride = FString::Printf(TEXT("+%lld %s"), XpShown, *NWText::Get(TEXT("ui_xp")));
	}
}

void ANWHUD::DrawLevelUp()
{
	const double Age = FPlatformTime::Seconds() - LevelUpAt;
	UFont* Font = Fonts.IsValidIndex(1) ? Fonts[1].Get() : nullptr;
	if (Age > 4.5 || !Canvas || !Font)
	{
		return;
	}
	// Enters in 0.3 s, stays, leaves in the last 0.8 s. bannerlevelup positions (1080p screen, center x = 960).
	const float Alpha = FMath::Clamp(float(FMath::Min(Age / 0.3, (4.5 - Age) / 0.8)), 0.f, 1.f);
	const float S = Canvas->ClipY / 1080.f;
	const float CX = Canvas->ClipX * 0.5f;
	auto Tex = [&](const TCHAR* Path, float L, float T, float R, float B, FLinearColor Color, float Rotation = 0.f)
	{
		if (UTexture2D* Tx = FNWUILayout::GameTexture(Path))
		{
			FCanvasTileItem Item(FVector2D(CX + (L - 960.f) * S, T * S), Tx->GetResource(), FVector2D((R - L) * S, (B - T) * S), Color.CopyWithNewOpacity(Color.A * Alpha));
			Item.BlendMode = SE_BLEND_Translucent;
			if (Rotation != 0.f)
			{
				Item.Rotation = FRotator(0.f, Rotation, 0.f);
				Item.PivotPoint = FVector2D(0.5, 0.5);
			}
			Canvas->DrawItem(Item);
		}
	};
	Tex(TEXT("lyshineui/images/banner/banner_fog.dds"), 760.f, 60.f, 1160.f, 360.f, FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Tex(TEXT("lyshineui/images/banner/bannerleveluprune_xp.dds"), 848.f, 96.f, 1072.f, 321.f, FLinearColor::White);
	Tex(TEXT("lyshineui/images/banner/bannerleveluprune_x.dds"), 840.f, 98.f, 1080.f, 338.f, FLinearColor::White, float(Age) * 12.f);
	Tex(TEXT("lyshineui/images/banner/banner_light_blue2.dds"), 800.f, 156.f, 1120.f, 266.f, FLinearColor(0.f, 0.f, 0.f, 0.85f));
	Tex(TEXT("lyshineui/images/notifications/notification_showline.dds"), 735.f, 170.f, 1185.f, 180.f, FLinearColor::White);
	auto Centered = [&](const FString& Text, float Y, float Size)
	{
		const float Scale = Size * S / FMath::Max(1, Font->LegacyFontSize);
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Font, Text, W, H, Scale, Scale);
		DrawText(Text, FLinearColor(1.f, 1.f, 1.f, Alpha), CX - W * 0.5f, Y * S, Font, Scale);
	};
	Centered(NWText::Get(TEXT("ui_level_up")), 186.f, 24.f);
	Centered(FString::FromInt(LevelUpValue), 208.f, 54.f);
}

void ANWHUD::AddHitIndicator(const FVector& From, float Fraction)
{
	// Up to 5 at the same time, like the game's HitIndicatorManager (EdgeIndicator1–5 / ArrowIndicator1–5).
	HitIndicators.Add({From, Fraction, FPlatformTime::Seconds()});
	if (HitIndicators.Num() > 5)
	{
		HitIndicators.RemoveAt(0);
	}
}

void ANWHUD::DrawHitIndicators()
{
	const double Now = FPlatformTime::Seconds();
	HitIndicators.RemoveAll([Now](const FHitIndicator& H) { return Now - H.Time > 1.0; });
	const APlayerController* PC = GetOwningPlayerController();
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me || !Canvas || HitIndicators.Num() == 0)
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.f;
	const FVector2D C(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	auto Tex = [&](const TCHAR* Path, FVector2D Center, FVector2D Size, float Angle, float Alpha)
	{
		if (UTexture2D* T = FNWUILayout::GameTexture(Path))
		{
			FCanvasTileItem Item(Center - Size * 0.5, T->GetResource(), Size, FLinearColor(1.f, 1.f, 1.f, Alpha));
			Item.BlendMode = SE_BLEND_Translucent;
			Item.Rotation = FRotator(0.f, Angle, 0.f);
			Item.PivotPoint = FVector2D(0.5, 0.5);
			Canvas->DrawItem(Item);
		}
	};
	for (const FHitIndicator& H : HitIndicators)
	{
		const float Alpha = FMath::Clamp(float(1.0 - (Now - H.Time)) / 0.6f, 0.f, 1.f);
		// Attacker angle relative to the camera: 0 = ahead (top), positive = to the right.
		const FVector To = H.From - Me->GetActorLocation();
		const float Angle = FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X)) - PC->GetControlRotation().Yaw);
		const float Rad = FMath::DegreesToRadians(Angle);
		// Arrow (Arrow, 172 x 46 at 204 px from the center on the 1080p screen).
		Tex(TEXT("lyshineui/images/hitindicator/hitdirection.dds"), C + FVector2D(FMath::Sin(Rad), -FMath::Cos(Rad)) * 204.f * S,
			FVector2D(172.f, 46.f) * S, Angle, Alpha);
		// Glow on the edge closest to the direction (EdgeImage): light up to 15% of health, medium above.
		const float Side = FMath::RoundToFloat(Angle / 90.f) * 90.f;
		const bool bVertical = FMath::Abs(FMath::Fmod(FMath::Abs(Side), 180.f)) > 1.f;
		const float Len = (bVertical ? Canvas->ClipY : Canvas->ClipX) * 0.7f;
		const float Thick = (bVertical ? Canvas->ClipX : Canvas->ClipY) * 0.3f;
		const float SideRad = FMath::DegreesToRadians(Side);
		const FVector2D Dir(FMath::Sin(SideRad), -FMath::Cos(SideRad));
		const FVector2D Half(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
		const FVector2D EdgeCenter = C + FVector2D(Dir.X * (Half.X - Thick * 0.5f), Dir.Y * (Half.Y - Thick * 0.5f));
		Tex(H.Fraction > 0.15f ? TEXT("lyshineui/images/hitindicator/hiteffect_medium_b.dds") : TEXT("lyshineui/images/hitindicator/hiteffect_light_b.dds"),
			EdgeCenter, FVector2D(Len, Thick), Side, Alpha * 0.8f);
	}
}

void ANWHUD::DrawWarning()
{
	const double Age = FPlatformTime::Seconds() - WarningAt;
	UFont* Font = Fonts.IsValidIndex(0) ? Fonts[0].Get() : nullptr;
	if (Age > 3.0 || WarningText.IsEmpty() || !Font || !Canvas)
	{
		return;
	}
	// WarningBar: background_warningbar background (51 px) and yellow text 23; fades in the last half second.
	const float Alpha = FMath::Clamp(float(3.0 - Age) / 0.5f, 0.f, 1.f);
	const float S = Canvas->ClipY / 1080.f;
	const float Scale = 23.f * S / FMath::Max(1, Font->LegacyFontSize);
	float W = 0.f, H = 0.f;
	Canvas->TextSize(Font, WarningText, W, H, Scale, Scale);
	const float BW = W + 160.f * S, BH = 41.f * S;
	const FVector2D Pos(Canvas->ClipX * 0.5f - BW * 0.5f, Canvas->ClipY * 0.3f);
	if (UTexture2D* Bg = FNWUILayout::GameTexture(TEXT("lyshineui/images/warningbar/background_warningbar.dds")))
	{
		FCanvasTileItem Item(Pos, Bg->GetResource(), FVector2D(BW, BH), FLinearColor(1.f, 1.f, 1.f, Alpha));
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}
	DrawText(WarningText, FLinearColor(1.f, 0.87f, 0.33f, Alpha), Pos.X + (BW - W) * 0.5f, Pos.Y + (BH - H) * 0.5f, Font, Scale);
}

void ANWHUD::DrawNavbar(int32 Active, const FVector2D& Mouse)
{
	UFont* Font = Fonts.IsValidIndex(1) ? Fonts[1].Get() : nullptr;
	if (!Canvas || !Font)
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.f;
	auto Tex = [&](const TCHAR* Path, FVector2D Pos, FVector2D Size, FLinearColor Tint = FLinearColor::White)
	{
		if (UTexture2D* T = FNWUILayout::GameTexture(Path))
		{
			FCanvasTileItem Item(Pos, T->GetResource(), Size, Tint);
			Item.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
	};
	// navbar_bg_2023 background (1920 x 104 on the reference screen) stretched across the width.
	Tex(TEXT("lyshineui/images/navbar/navbar_bg_2023.dds"), FVector2D::ZeroVector, FVector2D(Canvas->ClipX, 70.f * S));
	static const TCHAR* Icons[3] = {TEXT("lyshineui/images/inventory/icon_inventory.dds"), TEXT("lyshineui/images/navbar/navbar_icon_character.dds"),
		TEXT("lyshineui/images/navbar/navbar_icon_map.dds")};
	static const TCHAR* Labels[3] = {TEXT("ui_inventory"), TEXT("ui_skills"), TEXT("ui_map")};
	const float Scale = 20.f * S / FMath::Max(1, Font->LegacyFontSize);
	float X = 30.f * S;
	const ANWCharacter* Me = Cast<ANWCharacter>(GetOwningPawn());
	for (int32 i = 0; i < 3; ++i)
	{
		const FString Text = NWText::Get(Labels[i]);
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Font, Text, W, H, Scale, Scale);
		const float TabW = W + 70.f * S;
		NavTabs[i] = FBox2f(FVector2f(X, 8.f * S), FVector2f(X + TabW, 60.f * S));
		const bool bHover = NavTabs[i].IsInside(FVector2f(Mouse));
		if (i == Active || bHover)
		{
			// The game's highlight (392 x 88) behind the active/hovered tab.
			Tex(TEXT("lyshineui/images/navbar/highlight.dds"), FVector2D(NavTabs[i].Min), FVector2D(NavTabs[i].GetSize()),
				FLinearColor(1.f, 1.f, 1.f, i == Active ? 1.f : 0.5f));
		}
		const float IconSz = 24.f * S;
		Tex(Icons[i], FVector2D(X + 14.f * S, 22.f * S), FVector2D(IconSz), i == Active ? FLinearColor(FColor(0xED, 0xC6, 0x3E)) : FLinearColor::White);
		DrawText(Text, i == Active ? FLinearColor(FColor(0xED, 0xC6, 0x3E)) : FLinearColor(FColor(0xEB, 0xE3, 0xD6)),
			X + 46.f * S, 34.f * S - H * 0.5f, Font, Scale);
		// Free attribute points: the game's unspentpoints badge on the Character tab.
		if (i == 1 && Me && Me->Progression && Me->Progression->UnspentPoints() > 0)
		{
			Tex(TEXT("lyshineui/images/navbar/unspentpoints.dds"), FVector2D(NavTabs[i].Max.X - 22.f * S, 6.f * S), FVector2D(26.f * S));
		}
		X += TabW + 10.f * S;
	}
}

int32 ANWHUD::NavTabAt(const FVector2D& Mouse) const
{
	for (int32 i = 0; i < 3; ++i)
	{
		if (NavTabs[i].bIsValid && NavTabs[i].IsInside(FVector2f(Mouse)))
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void ANWHUD::OpenRefining(ANWCharacter* Who, ANWStation* Station)
{
	ANWPlayerController* PC = Cast<ANWPlayerController>(GetOwningPlayerController());
	if (!PC || !Who || !Station)
	{
		return;
	}
	PC->CloseMenus();
	Refining.Open(Who, Station);
	PC->EnterCanvasMenu();
}

void ANWHUD::OpenShop(ANWCharacter* Who, const FString& ShopId, const FString& Title)
{
	ANWPlayerController* PC = Cast<ANWPlayerController>(GetOwningPlayerController());
	if (!PC || !Who)
	{
		return;
	}
	PC->CloseMenus();
	Shop.Open(Who, ShopId, Title);
	if (Shop.IsOpen())
	{
		PC->EnterCanvasMenu();
	}
}

void ANWHUD::OpenStorage(ANWCharacter* Who, ANWStation* Station)
{
	ANWPlayerController* PC = Cast<ANWPlayerController>(GetOwningPlayerController());
	if (!PC || !Who || !Station || !Who->Interactor)
	{
		return;
	}
	PC->CloseMenus();
	// Shared Storage: the same in any storehouse (nw-core: shared); the storehouse's city becomes known.
	Who->Interactor->ServerStorageOpen(Station->City);
	// Inventory with Storage on the left (catcontainer.lua): drag between the two.
	Inventory.OpenStorage(Who, Station->Interact->DisplayName.ToString());
	PC->EnterCanvasMenu();
}

void ANWHUD::OpenMarket(ANWCharacter* Who, ANWStation* Station)
{
	ANWPlayerController* PC = Cast<ANWPlayerController>(GetOwningPlayerController());
	if (!PC || !Who || !Station || !Who->Interactor)
	{
		return;
	}
	// Trading Post: its own Slate screen (SNWMarket); Esc and CloseMenus close it like the F10 menu.
	TSharedRef<SNWMarket> Menu = SNew(SNWMarket).Character(Who).Title(Station->Interact->DisplayName.ToString());
	PC->ShowMenu(Menu, Menu);
}

void ANWHUD::OpenCity(ANWCharacter* Who, ANWStation* Station, bool bGovernance)
{
	ANWPlayerController* PC = Cast<ANWPlayerController>(GetOwningPlayerController());
	if (!PC || !Who || !Station || !Who->Interactor)
	{
		return;
	}
	TSharedRef<SNWCity> Menu = SNew(SNWCity).Character(Who).Title(Station->Interact->DisplayName.ToString()).bGovernance(bGovernance);
	PC->ShowMenu(Menu, Menu);
}

void ANWHUD::OpenLootBag(ANWCharacter* Who, ANWLootBag* Bag)
{
	ANWPlayerController* PC = Cast<ANWPlayerController>(GetOwningPlayerController());
	if (!PC || !Who || !Bag)
	{
		return;
	}
	PC->CloseMenus();
	Inventory.OpenLootBag(Who, Bag);
	PC->EnterCanvasMenu();
}

bool ANWHUD::StationMenuWheel(float Delta)
{
	if (Refining.IsOpen())
	{
		Refining.Wheel(Delta);
		return true;
	}
	if (Inventory.IsOpen())   // bag alone (Tab) or with Storage
	{
		float MX = 0.f, MY = 0.f;
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->GetMousePosition(MX, MY);
		}
		Inventory.Wheel(Delta, FVector2D(MX, MY));
		return true;
	}
	return false;
}

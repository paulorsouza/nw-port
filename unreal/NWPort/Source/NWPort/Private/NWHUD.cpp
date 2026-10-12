#include "NWHUD.h"
#include "NWText.h"

#include "NWBackend.h"
#include "NWItems.h"
#include "NWGatherComponent.h"
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

static TAutoConsoleVariable<int32> CVarHudDebug(TEXT("nw.HudDebug"), 0, TEXT("Mostra a linha de diagnóstico de movimento/animação na HUD."));

static const TCHAR* VitalsCanvas = TEXT("vitals2024");
static const TCHAR* WeaponCanvas = TEXT("weaponquickslots");
static const TCHAR* ConsumableCanvas = TEXT("consumablequickslots");

static const TCHAR* SlotNames[3] = {TEXT("WeaponAbility"), TEXT("WeaponAbility2"), TEXT("WeaponAbility3")};
static const TCHAR* SetNames[2] = {TEXT("WeaponQuickslots/WeaponAbilitySet"), TEXT("WeaponQuickslots/WeaponAbilitySet2")};
static const TCHAR* SlotKeys[3] = {TEXT("Q"), TEXT("R"), TEXT("F")};

void ANWHUD::BeginPlay()
{
	Super::BeginPlay();
	// Fora de Content: o editor vigia Content e tentaria importar o .json e os .otf como assets.
	Layout.Load(NWText::DataPath(TEXT("Data/UI/hud_layout.json")));
	Layout.ResolveTexture = [this](const FString& P) { return ResolveTexture(P); };
	// Ícone e fundo de cada habilidade (tools/make_abilities.py), para as duas armas.
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
		// A barra do jogo enche por recorte; aqui, preenchimento linear da esquerda.
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
	bStaticDone = false;
	Mastery.Close();
	Mastery = FNWMasteryMenu();
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
	for (FNWUIElement* E : Layout.Find(WeaponCanvas, TEXT("WeaponQuickslots/SwapHint"))) E->Visible = false;
	// Sem consumíveis por enquanto: molduras vazias.
	for (FNWUIElement* E : Layout.Find(ConsumableCanvas, TEXT("ItemLayoutConsumable"))) E->Visible = false;
	for (FNWUIElement* E : Layout.Find(ConsumableCanvas, TEXT("CooldownTimer"))) E->Visible = false;
	// Teclas do jogo (libs/config/defaultprofile.xml: quickslot-consumable-1..4 = 3..6); sem ícone de controle.
	const TCHAR* ConsumableSlots[4] = {TEXT("ConsumableQuickslot"), TEXT("ConsumableQuickslot2"), TEXT("ConsumableQuickslot3"), TEXT("ConsumableQuickslot4")};
	for (int32 i = 0; i < 4; ++i)
	{
		if (FNWUIElement* E = Layout.FindOne(ConsumableCanvas, FString(ConsumableSlots[i]) + TEXT("/Hint/HintText"))) E->TextOverride = FString::FromInt(3 + i);
	}
	for (FNWUIElement* E : Layout.Find(ConsumableCanvas, TEXT("ModifierDivider/HintModifier"))) E->Visible = false;
	// Cores que o Lua aplicava nas barras.
	auto Tint = [this](const TCHAR* Path, const FLinearColor& C) { for (FNWUIElement* E : Layout.Find(VitalsCanvas, Path)) E->Tint = C; };
	Tint(TEXT("HealthMeter/MultiFillMeter/MainFill"), FLinearColor(0.70f, 0.13f, 0.10f));
	Tint(TEXT("HealthMeter/MultiFillMeter/UnderFill"), FLinearColor(0.95f, 0.78f, 0.62f, 0.85f));
	Tint(TEXT("ManaMeter/MultiFillMeter/MainFill"), FLinearColor(0.22f, 0.48f, 0.88f));
	Tint(TEXT("ManaMeter/MultiFillMeter/UnderFill"), FLinearColor(0.6f, 0.75f, 1.f, 0.6f));
	Tint(TEXT("StaminaMeter/MultiFillMeter/UnderFill"), FLinearColor(1.f, 0.95f, 0.75f, 0.7f));
	for (const TCHAR* Cover : {TEXT("WarningCover"), TEXT("RefillCover")})
	{
		for (FNWUIElement* E : Layout.Find(VitalsCanvas, Cover)) E->Visible = false;
	}
}

void ANWHUD::UpdateVitals(const UNWCombatComponent* C, float DeltaSeconds)
{
	const float H = C->MaxHealth > 0.f ? C->Health / C->MaxHealth : 0.f;
	const float S = C->MaxStamina > 0.f ? C->Stamina / C->MaxStamina : 0.f;
	// A barra de trás acompanha a da frente com atraso quando cai; quando sobe, junto.
	HealthLag = H >= HealthLag ? H : FMath::FInterpConstantTo(HealthLag, H, DeltaSeconds, 0.35f);
	StaminaLag = S >= StaminaLag ? S : FMath::FInterpConstantTo(StaminaLag, S, DeltaSeconds, 0.6f);
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("HealthMeter/MultiFillMeter/MainFill"))) E->Fill = H;
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("HealthMeter/MultiFillMeter/UnderFill"))) E->Fill = HealthLag;
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("HealthMeter/ValueText"))) E->TextOverride = FString::FromInt(FMath::CeilToInt(C->Health));
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("StaminaMeter/MultiFillMeter/MainFill")))
	{
		E->Fill = S;
		E->Tint = C->bExhausted ? FLinearColor(0.55f, 0.45f, 0.22f) : FLinearColor(0.88f, 0.70f, 0.22f);
	}
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("StaminaMeter/MultiFillMeter/UnderFill"))) E->Fill = StaminaLag;
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("StaminaMeter/ValueText"))) E->Visible = false;
	for (FNWUIElement* E : Layout.Find(VitalsCanvas, TEXT("ManaMeter/ValueText"))) E->Visible = false;
}

void ANWHUD::UpdateAbilities(const ANWCharacter* Me, float DeltaSeconds)
{
	const UNWCombatComponent* C = Me->Combat;
	ReadyFlash.SetNumZeroed(6);
	WasCooling.SetNumZeroed(6);
	const int32 NumSets = FMath::Max(1, Me->WeaponSets.Num());
	for (int32 Row = 0; Row < 2; ++Row)
	{
		// Linha 0 (embaixo, maior) = arma ativa; linha 1 = a outra, com o fundo "inativo" do jogo.
		const FString Set = SetNames[Row];
		const int32 SetIndex = Me->WeaponSets.Num() > 0 ? (Me->ActiveWeapon + Row) % NumSets : INDEX_NONE;
		const bool bShown = Row == 0 || Me->WeaponSets.Num() > 1;
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
			Icon->TextureOverride = WS ? WS->HudIcon : FString(TEXT("lyshineui/images/icons/items/drawing/1hsword.dds"));
		}
		if (FNWUIElement* ShieldIcon = Layout.FindOne(WeaponCanvas, Set + TEXT("/ShieldIcon")))
		{
			ShieldIcon->Visible = !WS || WS->bUsesShield;
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
			if (FNWUIElement* Bar = Layout.FindOne(WeaponCanvas, Slot + TEXT("/CooldownTimer/BarFill"))) Bar->Fill = Duration > 0.f ? Remaining / Duration : 0.f;
			if (FNWUIElement* Text = Layout.FindOne(WeaponCanvas, Slot + TEXT("/CooldownTimer/TimerText"))) Text->TextOverride = FString::FromInt(FMath::CeilToInt(Remaining));
			if (FNWUIElement* Dim = Layout.FindOne(WeaponCanvas, Slot + TEXT("/Dimmer")))
			{
				Dim->Visible = bCooling || Row == 1;
				Dim->Fill = 1.f;
			}
			// Brilho quando a recarga termina.
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
	Super::EndPlay(Reason);
}

void ANWHUD::DrawHUD()
{
	DrawGameHUD();
	if (Canvas && FNWOptions::Get().bShowFps)
	{
		// FPS no canto superior direito (opção @ui_show_fps).
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

	// -nwshot[=arquivo]: depois de alguns segundos tira um print da tela e fecha (conferência sem pessoa).
	static double FirstDraw = Now;
	static int32 ShotState = 0;
	FString ShotFile;
	if (FParse::Value(FCommandLine::Get(), TEXT("-nwshot="), ShotFile) || FParse::Param(FCommandLine::Get(), TEXT("nwshot")))
	{
		if (ShotFile.IsEmpty())
		{
			ShotFile = FPaths::ProjectSavedDir() / TEXT("nwshot.png");
		}
		// -nwmenu=skill|dev: abre o menu antes do print.
		// Por HUD (reabre depois de entrar no servidor); 1,5 s depois de esta HUD começar a desenhar.
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
		// -nwweapon=2: troca para a segunda arma antes.
		// Repete até a troca valer (logo depois de nascer o personagem ainda está caindo).
		int32 WeaponArg = 0;
		if (Now - FirstDraw > 1.0 && FParse::Value(FCommandLine::Get(), TEXT("-nwweapon="), WeaponArg) && Me->ActiveWeapon != WeaponArg - 1)
		{
			const_cast<ANWCharacter*>(Me)->SwitchWeapon(WeaponArg - 1);
		}
		// Usa Q e R aos 3 s para o print mostrar a recarga.
		static bool bUsedAbilities = false;
		if (!bUsedAbilities && Now - FirstDraw > 3.0 && Me->Abilities.Num() >= 2)
		{
			bUsedAbilities = true;
			Me->Combat->RequestAction(Me->Abilities[0]);
			FTimerHandle H;
			// Ponteiro fraco: o mapa pode trocar (entrada no servidor) antes do timer.
			GetWorldTimerManager().SetTimer(H, [Weak = TWeakObjectPtr<const ANWCharacter>(Me)]()
			{
				if (const ANWCharacter* C = Weak.Get(); C && C->Abilities.IsValidIndex(1))
				{
					C->Combat->RequestAction(C->Abilities[1]);
				}
			}, 1.6f, false);
		}
		// -nwshotdelay=N: espera mais (cliente que conecta num servidor antes do print).
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
	// Morto: só a tela de morte, com o mouse livre para o Renascer.
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
	if (Inventory.IsOpen())
	{
		float MX = 0.f, MY = 0.f;
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->GetMousePosition(MX, MY);
		}
		Inventory.Draw(Canvas, FVector2D(MX, MY));
		DrawLootTicker();
		return;
	}
	if (Mastery.IsOpen())
	{
		// Com a tela de maestria aberta, a HUD de combate sai (como no jogo).
		float MX = 0.f, MY = 0.f;
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->GetMousePosition(MX, MY);
		}
		Mastery.Draw(Canvas, FVector2D(MX, MY));
		return;
	}
	if (Layout.IsLoaded())
	{
		if (!bStaticDone)
		{
			SetupStatic();
		}
		UpdateVitals(C, Dt);
		UpdateAbilities(Me, Dt);
		UpdateConsumables(Me);
		Layout.Draw(Canvas);
	}

	// Carga do pesado, perto da mira: marca do pesado normal; cheia = carregado.
	if (C->IsCharging())
	{
		const float CW = 120.f;
		const float CX = (Canvas->ClipX - CW) * 0.5f;
		const float CY = Canvas->ClipY * 0.5f + 60.f;
		const float Ratio = C->GetChargeRatio();
		DrawBar(CX, CY, CW, 5.f, Ratio, Ratio >= 1.f ? FLinearColor(1.f, 0.85f, 0.3f) : FLinearColor(0.85f, 0.85f, 0.85f));
		DrawRect(FLinearColor::Black, CX + CW * (C->HeavyMinHold / C->HeavyFullCharge) - 1.f, CY - 2.f, 2.f, 9.f);
	}
	DrawCompass();
	DrawDamageNumbers();
	DrawNameplates(Me);
	DrawGroup(Me);
	DrawLootTicker();
	DrawGather(Me);
	DrawTarget(Me);
	Chat.Draw(Canvas);
	if (bShowDiagnostics || CVarHudDebug.GetValueOnGameThread())
	{
		DrawDiagnostics(Me);
	}
}

void ANWHUD::DrawGather(const APawn* Me)
{
	const ANWCharacter* Char = Cast<ANWCharacter>(Me);
	const UNWGatherComponent* G = Char ? Char->Gather.Get() : nullptr;
	UFont* Font = Fonts.IsValidIndex(1) ? Fonts[1].Get() : nullptr;
	if (!G || !Font)
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.f;
	const float Scale = 20.f * S / FMath::Max(1, Font->LegacyFontSize);
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.5f + 120.f * S;
	if (G->IsGathering())
	{
		// Barra de coleta abaixo da mira.
		const float W = 260.f * S;
		DrawBar(CX - W * 0.5f, Y, W, 8.f * S, G->Progress(), FLinearColor(FColor(0xED, 0xC6, 0x3E)));
		return;
	}
	if (G->FocusText.IsEmpty())
	{
		return;
	}
	// [E] Ação Nome  (vermelho apagado quando falta a ferramenta).
	float W = 0.f, H = 0.f;
	Canvas->TextSize(Font, G->FocusText, W, H, Scale, Scale);
	const float Key = 30.f * S, Pad = 10.f * S;
	const float Total = Key + Pad + W;
	const float X = CX - Total * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X - Pad, Y - Pad * 0.5f, Total + Pad * 2.f, Key + Pad);
	if (!G->bFocusBlocked)
	{
		DrawRect(FLinearColor(0.92f, 0.89f, 0.84f, 0.9f), X, Y, Key, Key);
		float KW = 0.f, KH = 0.f;
		Canvas->TextSize(Font, TEXT("E"), KW, KH, Scale, Scale);
		DrawText(TEXT("E"), FLinearColor(0.05f, 0.05f, 0.05f), X + (Key - KW) * 0.5f, Y + (Key - KH) * 0.5f, Font, Scale);
	}
	const FLinearColor Color = G->bFocusBlocked ? FLinearColor(FColor(0xFF, 0x71, 0x71)) : FLinearColor(FColor(0xEB, 0xE3, 0xD6));
	DrawText(G->FocusText, Color, X + Key + Pad, Y + (Key - H) * 0.5f, Font, Scale);
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
	DrawText(Name, FLinearColor(0.93f, 0.88f, 0.78f), (Canvas->ClipX - W) * 0.5f, 30.f, Font, S);
	DrawBar(TX, 34.f + H, TW, 8.f, T->Health / T->MaxHealth, FLinearColor(0.70f, 0.13f, 0.10f));
}

void ANWHUD::SetDeathScreen(bool bOn)
{
	bDeathScreen = bOn;
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

// Nome e vida sobre os outros personagens até 40 m, menores e mais apagados com a distância.
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
	// Atalho 3 = melhor poção de vida da bolsa (no jogo o jogador escolhe; aqui é automático por enquanto).
	const UNWInventoryComponent* Inv = Me->Inventory;
	const int32 Slot = Inv ? Inv->BestPotionSlot() : INDEX_NONE;
	const FNWItem* Item = Slot != INDEX_NONE ? Inv->At(ENWItemContainer::Bag, Slot) : nullptr;
	const FNWItemDef* Def = Item ? FNWItemCatalog::Get().Find(Item->ItemId) : nullptr;
	const FString Root = TEXT("ConsumableQuickslot/");
	if (FNWUIElement* L = Layout.FindOne(ConsumableCanvas, Root + TEXT("ItemLayoutConsumable"))) L->Visible = Def != nullptr;
	if (!Def)
	{
		if (FNWUIElement* T = Layout.FindOne(ConsumableCanvas, Root + TEXT("CooldownTimer"))) T->Visible = false;
		return;
	}
	if (FNWUIElement* I = Layout.FindOne(ConsumableCanvas, Root + TEXT("ItemLayoutConsumable/ItemIcon"))) I->TextureOverride = Def->Icon;
	if (FNWUIElement* R = Layout.FindOne(ConsumableCanvas, Root + TEXT("ItemLayoutConsumable/ItemRarityBg"))) R->Visible = false;
	if (FNWUIElement* T = Layout.FindOne(ConsumableCanvas, Root + TEXT("ItemLayoutConsumable/ItemTier"))) T->TextOverride = FString::FromInt(Def->Tier);
	if (FNWUIElement* Q = Layout.FindOne(ConsumableCanvas, Root + TEXT("ItemLayoutConsumable/ItemQuantity"))) Q->TextOverride = FString::FromInt(Inv->CountOf(Item->ItemId));
	if (FNWUIElement* A = Layout.FindOne(ConsumableCanvas, Root + TEXT("ItemLayoutConsumable/ItemAnimatedIndicator"))) A->Visible = false;
	if (FNWUIElement* D = Layout.FindOne(ConsumableCanvas, Root + TEXT("ItemLayoutConsumable/ItemDimmer"))) D->Visible = false;
	const float Cd = Inv->GetPotionCooldown();
	const float Total = FMath::Max(1.f, Def->Cooldown);
	if (FNWUIElement* T = Layout.FindOne(ConsumableCanvas, Root + TEXT("CooldownTimer"))) T->Visible = Cd > 0.f;
	if (FNWUIElement* B = Layout.FindOne(ConsumableCanvas, Root + TEXT("CooldownTimer/BarFill"))) B->Fill = Cd / Total;
	if (FNWUIElement* X = Layout.FindOne(ConsumableCanvas, Root + TEXT("CooldownTimer/TimerText"))) X->TextOverride = FString::FromInt(FMath::CeilToInt(Cd));
}

void ANWHUD::ShowLoot(const TArray<FNWItem>& Gained, int64 Coins)
{
	const double Now = FPlatformTime::Seconds();
	if (Coins > 0)
	{
		LootLines.Add({FString(), FString::Printf(TEXT("+%lld  %s"), Coins, *NWText::Get(TEXT("nw.inv.coins"))), FLinearColor(0.93f, 0.78f, 0.25f), Now});
	}
	for (const FNWItem& I : Gained)
	{
		const FNWItemDef* D = FNWItemCatalog::Get().Find(I.ItemId);
		if (D)
		{
			LootLines.Add({D->Icon, I.Quantity > 1 ? FString::Printf(TEXT("%s  x%d"), *D->Name, I.Quantity) : D->Name,
				D->Type == TEXT("weapon") ? NWRarityColor(I.Rarity) : FLinearColor(0.93f, 0.88f, 0.78f), Now});
		}
	}
}

void ANWHUD::DrawLootTicker()
{
	const double Now = FPlatformTime::Seconds();
	LootLines.RemoveAll([Now](const FLootLine& L) { return Now - L.Time > 6.0; });
	UFont* Font = Fonts.IsValidIndex(1) ? Fonts[1].Get() : nullptr;
	if (!Font || LootLines.Num() == 0)
	{
		return;
	}
	// À direita, no meio da tela, como o aviso de saque do jogo: ícone + nome; some nos últimos 1,5 s.
	const float S = Canvas->ClipY / 1080.f;
	const float Scale = 20.f * S / FMath::Max(1, Font->LegacyFontSize);
	const float IconSize = 40.f * S;
	float Y = Canvas->ClipY * 0.42f;
	for (const FLootLine& L : LootLines)
	{
		const float Alpha = FMath::Clamp(float(6.0 - (Now - L.Time)) / 1.5f, 0.f, 1.f);
		const float X = Canvas->ClipX - 380.f * S;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f * Alpha), X - 8.f * S, Y - 2.f * S, 360.f * S, IconSize + 4.f * S);
		if (!L.Icon.IsEmpty())
		{
			if (UTexture2D* Tex = ResolveTexture(L.Icon))
			{
				DrawTexture(Tex, X, Y, IconSize, IconSize, 0.f, 0.f, 1.f, 1.f, FLinearColor(1.f, 1.f, 1.f, Alpha));
			}
		}
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Font, L.Text, W, H, Scale, Scale);
		DrawText(L.Text, L.Color.CopyWithNewOpacity(Alpha), X + IconSize + 10.f * S, Y + (IconSize - H) * 0.5f, Font, Scale);
		Y += IconSize + 8.f * S;
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
	if (!PC || !Strip || !Canvas)
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.f;
	const float CX = Canvas->ClipX * 0.5f;
	// Direção da câmera em graus de bússola (0 = norte, 90 = leste), -180..180.
	const float Heading = FRotator::NormalizeAxis(PC->GetControlRotation().Yaw - CompassNorthYaw);
	// Na tira: N em 1021,6 px, 4,592 px por grau; cobre de -180° (S à esquerda) até +222°. Escala 1:1 na tela 1080p.
	constexpr float PxPerDeg = 4.592f, NorthPx = 1021.6f, TexW = 2048.f, Half = 386.f / PxPerDeg;
	const float Y0 = 3.f * S, H = 64.f * S;
	auto Piece = [&](float From, float To, float ScreenFrom)
	{
		// Fatias com alfa que some nas pontas (a máscara compassmask do jogo).
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
		// Passou do sul pela esquerda: o pedaço de antes de -180 vem do fim da volta (+180).
		Piece(Left + 360.f, 180.f, -Half);
		Piece(-180.f, Heading + Half, -180.f - Heading);
	}
	else
	{
		Piece(Left, 180.f, -Half);
		Piece(-180.f, Heading + Half - 360.f, 180.f - Heading);
	}
	// Linha de base e agulha (CompassLines / Needle).
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
}

void ANWHUD::AddDamageNumber(const FVector& World, const FString& Text, const FLinearColor& Color, bool bBig)
{
	// Deslocamento lateral pequeno e aleatório: golpes seguidos não ficam um em cima do outro.
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
	for (const FDamageNumber& N : DamageNumbers)
	{
		FVector2D Screen;
		if (!PC->ProjectWorldLocationToScreen(N.World, Screen, true))
		{
			continue;
		}
		const float T = float(Now - N.Time) / 1.2f;
		// Entra grande e encolhe (0,15 s), sobe 70 px, some no último terço.
		const float Pop = 1.f + 0.35f * FMath::Clamp(1.f - T / 0.125f, 0.f, 1.f);
		const float Size = (N.bBig ? 30.f : 24.f) * Pop * S;
		const float Scale = Size / FMath::Max(1, Font->LegacyFontSize);
		const float Alpha = FMath::Clamp((1.f - T) / 0.33f, 0.f, 1.f);
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Font, N.Text, W, H, Scale, Scale);
		const FVector2D At(Screen.X + N.Drift * S - W * 0.5f, Screen.Y - 70.f * S * T - H * 0.5f);
		FCanvasTextItem Item(At, FText::FromString(N.Text), Font, N.Color.CopyWithNewOpacity(Alpha));
		Item.Scale = FVector2D(Scale, Scale);
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.9f * Alpha));
		Canvas->DrawItem(Item);
	}
}

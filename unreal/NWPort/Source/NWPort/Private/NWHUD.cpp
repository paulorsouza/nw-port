#include "NWHUD.h"
#include "NWProgressionComponent.h"
#include "NWText.h"

#include "NWBackend.h"
#include "NWItems.h"
#include "NWGatherComponent.h"
#include "NWGatherField.h"
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
static const TCHAR* XpCanvas = TEXT("xp");

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
	Tradeskills.Close();
	Tradeskills = FNWTradeskillMenu();
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
		FNWTradeskillMenu::DrawTabs(Canvas, FVector2D(MX, MY), 0);
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
		UpdateXp(Me);
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
	DrawLevelUp();
	DrawHitIndicators();
	DrawWarning();
	DrawDamageNumbers();
	DrawEncumbrance(Me);
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
	// Cartão de coleta do jogo (markers/unifiedinteract, Gatherable_UnifiedInteractCard): anel no objeto, nome numa
	// faixa à direita e a linha com a tecla e a ação; durante a coleta o anel de progresso enche com o tempo no meio.
	const ANWCharacter* Char = Cast<ANWCharacter>(Me);
	const UNWGatherComponent* G = Char ? Char->Gather.Get() : nullptr;
	const ANWGatherField* Field = ANWGatherField::Get(GetWorld());
	UFont* Font = Fonts.IsValidIndex(1) ? Fonts[1].Get() : nullptr;
	APlayerController* PC = GetOwningPlayerController();
	if (!G || !Field || !Font || !PC)
	{
		return;
	}
	const int32 Index = G->IsGathering() ? G->GetGatherIndex() : G->FocusIndex;
	if (Index == INDEX_NONE || Index >= Field->Num())
	{
		return;
	}
	FVector2D C;
	const FVector World = Field->GetActorTransform().TransformPosition(Field->Node(Index).Transform.GetLocation()) + FVector(0.f, 0.f, 90.f);
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
		// Anel de progresso: leque de triângulos com a textura do anel, do topo no sentido horário.
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
	// Faixa do nome à direita do anel (TitleFrame: +185 da âncora do cartão; aqui colada ao anel) e a linha da tecla.
	const ANWGatherField::FVariant& V = Field->VariantOf(Index);
	const FString Name = NWText::Get(V.NameKey);
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
	// Linha de ação: [E] Minerar, ou o aviso em vermelho (Requer Picareta).
	const FString Action = G->FocusText.Contains(TEXT("·")) ? G->FocusText.Mid(G->FocusText.Find(TEXT("·")) + 1).TrimStartAndEnd()
		: G->FocusText.Left(G->FocusText.Len() - Name.Len()).TrimStartAndEnd();
	const float RowY = TitlePos.Y + 46.f * S;
	float X = TitlePos.X + 4.f * S;
	const float ActScale = 17.f * S / FMath::Max(1, Font->LegacyFontSize);
	if (!G->bFocusBlocked)
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
	DrawText(Action, G->bFocusBlocked ? FLinearColor(FColor(0xFF, 0x71, 0x71)) : Tan, X, RowY + (30.f * S - AH) * 0.5f, Font, ActScale);
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
		LootLines.Add({NAME_None, 0, 0, Coins, Now});
	}
	for (const FNWItem& I : Gained)
	{
		if (FNWItemCatalog::Get().Find(I.ItemId))
		{
			LootLines.Add({I.ItemId, I.Quantity, I.Rarity, 0, Now});
		}
	}
	// No máximo 5 linhas, como o jogo (o resto vira "mais itens"; aqui as mais antigas saem).
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
	// LootTicker2024: canto inferior direito (x de -802, y de -490 da tela 1080p), linhas de 58 px, a mais nova em cima.
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
	auto Text = [&](const FString& Str, float X, float Y, float Size, FLinearColor Color, float Alpha)
	{
		const float Scale = Size * S / FMath::Max(1, Font->LegacyFontSize);
		DrawText(Str, Color.CopyWithNewOpacity(Alpha), X, Y, Font, Scale);
	};
	float MaxAlpha = 0.f;
	for (const FLootLine& L : LootLines)
	{
		MaxAlpha = FMath::Max(MaxAlpha, FMath::Clamp(float(6.0 - (Now - L.Time)) / 1.5f, 0.f, 1.f));
	}
	Tex(TEXT("lyshineui/images/hud2024/lootticker/loottickerbg.dds"), O + FVector2D(28.f, 0.f) * S, FVector2D(420.f, 310.f) * S, 0.8f * MaxAlpha);
	const FLinearColor Tan(FColor(0xEB, 0xE3, 0xD6));
	for (int32 i = 0; i < LootLines.Num(); ++i)
	{
		const FLootLine& L = LootLines[LootLines.Num() - 1 - i];
		const float Alpha = FMath::Clamp(float(6.0 - (Now - L.Time)) / 1.5f, 0.f, 1.f);
		const FVector2D Row = O + FVector2D(0.f, 12.f + i * 58.f) * S;
		Tex(TEXT("lyshineui/images/lootticker/progressiontickeritembg.dds"), Row + FVector2D(2.f, -12.f) * S, FVector2D(385.f, 68.f) * S, Alpha);
		if (L.Trade >= 0)
		{
			// Progresso de ofício (progressiontickeritembg do jogo): ícone, "+N", nome e o nível com a barra.
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
			Text(FString::Printf(TEXT("+%lld"), L.Coins), Row.X + 107.f * S, Row.Y + 14.f * S, 19.f, FLinearColor(FColor(0xED, 0xC6, 0x3E)), Alpha);
			Text(NWText::Get(TEXT("nw.inv.coins")), Row.X + 175.f * S, Row.Y + 14.f * S, 18.f, Tan, Alpha);
			continue;
		}
		const FNWItemDef* D = FNWItemCatalog::Get().Find(L.ItemId);
		if (!D)
		{
			continue;
		}
		// Peça do item (ItemLayoutSimple) reduzida a 92 x 46: fundo, faixa de raridade, ícone e tier.
		const int32 Rarity = D->Type == TEXT("weapon") ? FMath::Min<int32>(L.Rarity, 4) : FMath::Clamp(D->Tier - 1, 0, 4);
		const FVector2D Tile = Row + FVector2D(10.f, 6.f) * S;
		const FVector2D TileSize = FVector2D(92.f, 46.f) * S;
		Tex(TEXT("lyshineui/images/lootticker/lootticker_itembg_square.dds"), Tile - FVector2D(3.f) * S, TileSize + FVector2D(6.f) * S, Alpha);
		Tex(FString::Printf(TEXT("lyshineui/images/slices/itemlayout/itemraritybglarge%d.dds"), Rarity), Tile, TileSize, Alpha);
		Tex(D->Icon, Tile + FVector2D((TileSize.X - TileSize.Y) * 0.5f, 0.f), FVector2D(TileSize.Y), Alpha);
		static const TCHAR* Roman[] = {TEXT(""), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V")};
		Text(Roman[FMath::Clamp(D->Tier, 0, 5)], Tile.X + 4.f * S, Tile.Y, 13.f, Tan, Alpha);
		// "+N" e o nome à direita (ItemQuantity / ItemName), nome na cor da raridade das armas.
		Text(FString::Printf(TEXT("+%d"), L.Quantity), Row.X + 112.f * S, Row.Y + 16.f * S, 18.f, FLinearColor(FColor(0xED, 0xC6, 0x3E)), Alpha);
		Text(D->Name, Row.X + 160.f * S, Row.Y + 16.f * S, 18.f, D->Type == TEXT("weapon") ? NWRarityColor(L.Rarity) : Tan, Alpha);
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
	// POIs a até 400 m na tira, no rumo em que estão (o jogo mostra os pontos próximos na bússola).
	if (const APawn* Me = PC->GetPawn())
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
			const float Rel = FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)) - PC->GetControlRotation().Yaw);
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
	// Empilhado (opção do jogo): lista fixa à direita da mira, o mais novo embaixo; flutuando: sobre o alvo.
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
		// Entra grande e encolhe (0,15 s), sobe 70 px, some no último terço.
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
	const float Ratio = Inv->TotalWeight() / UNWInventoryComponent::MaxWeight;
	const bool bOver = Ratio > 1.f;
	// EncumbranceLayoutElement: 60 x 60 no canto inferior direito (offsets -96..-36, -113..-53 da tela 1080p).
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
	// Ícone da bolsa (100 x 72) centrado no espaço.
	const FVector2D Icon(46.f * S, 33.f * S);
	Tex(TEXT("lyshineui/images/inventory/icon_inventory.dds"), Slot + (FVector2D(Size) - Icon) * 0.5, Icon,
		bOver ? FLinearColor(1.f, 0.45f, 0.4f) : FLinearColor(1.f, 1.f, 1.f, 0.9f));
	// Porcentagem do peso no círculo (PercentageBg), a partir de 80% como aviso.
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
		// Faixa de mensagem à esquerda (Message: encumbrance_messagebgside) com o aviso do jogo.
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
	// Ganhos seguidos somam no mesmo aviso.
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
	// A ponta acompanha o preenchimento (no jogo o Lua move; largura da barra = 322).
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
	// Entra em 0,3 s, fica, sai no último 0,8 s. Posições do bannerlevelup (tela 1080p, centro x = 960).
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
	// Até 5 ao mesmo tempo, como o HitIndicatorManager do jogo (EdgeIndicator1–5 / ArrowIndicator1–5).
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
		// Ângulo do atacante em relação à câmera: 0 = à frente (em cima), positivo = à direita.
		const FVector To = H.From - Me->GetActorLocation();
		const float Angle = FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X)) - PC->GetControlRotation().Yaw);
		const float Rad = FMath::DegreesToRadians(Angle);
		// Seta (Arrow, 172 x 46 a 204 px do centro na tela 1080p).
		Tex(TEXT("lyshineui/images/hitindicator/hitdirection.dds"), C + FVector2D(FMath::Sin(Rad), -FMath::Cos(Rad)) * 204.f * S,
			FVector2D(172.f, 46.f) * S, Angle, Alpha);
		// Brilho na borda mais perto da direção (EdgeImage): leve até 15% da vida, médio acima.
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
	// WarningBar: fundo background_warningbar (51 px) e texto amarelo 23; some no último meio segundo.
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

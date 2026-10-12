#include "NWGameMenu.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "NWOptions.h"
#include "NWText.h"
#include "NWUILayout.h"

static const FLinearColor MenuTan(FColor(0xEB, 0xE3, 0xD6));
static const FLinearColor MenuDim(FColor(0x9A, 0x96, 0x8E));
static const FLinearColor MenuGold(FColor(0xED, 0xC6, 0x3E));

namespace
{
	// Ids dos botões: ações, abas e linhas de opção (linha * 10 + parte: 0 seta esquerda, 1 seta direita, 2 barra).
	enum : int32 { IdResume = 1, IdSettings, IdExitMenu, IdExitDesktop, IdBack, IdResetKeys, IdUnstuck, IdTab = 10, IdKey = 500, IdRow = 1000 };
	const TArray<int32> FpsSteps = {30, 60, 120, 144, 240, 0};

	FString OnOff(bool b) { return NWText::Get(b ? TEXT("ui_on") : TEXT("ui_off")); }
	FString Percent(float V) { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V * 100.f)); }
}

float FNWGameMenu::TextWidth(const FString& Text, float Size, int32 Style) const
{
	UFont* Font = FNWUILayout::GameFont(Style);
	return Font ? Font->GetStringSize(*Text) * Size * UIScale / Font->LegacyFontSize : 0.f;
}

void FNWGameMenu::DrawText(UCanvas* Canvas, const FString& Text, float X, float Y, float Size, const FLinearColor& Color, int32 Style) const
{
	UFont* Font = FNWUILayout::GameFont(Style);
	if (!Font || Text.IsEmpty())
	{
		return;
	}
	const float Scale = Size * UIScale / Font->LegacyFontSize;
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Color);
	Item.Scale = FVector2D(Scale, Scale);
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	Canvas->DrawItem(Item);
}

void FNWGameMenu::DrawPanel(UCanvas* Canvas, const FBox2f& R) const
{
	// Fundo das janelas do jogo (inventory/contentbg) e a faixa de cabeçalho.
	if (UTexture2D* Tex = FNWUILayout::GameTexture(TEXT("lyshineui/images/inventory/contentbg.dds")))
	{
		FCanvasTileItem Tile(FVector2D(R.Min), Tex->GetResource(), FVector2D(R.GetSize()), FLinearColor(1.f, 1.f, 1.f, 0.97f));
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}
	else
	{
		FCanvasTileItem Tile(FVector2D(R.Min), FVector2D(R.GetSize()), FLinearColor(0.05f, 0.05f, 0.05f, 0.95f));
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}
}

bool FNWGameMenu::Button(UCanvas* Canvas, const FBox2f& R, const FString& Label, int32 Id, const FVector2D& Mouse, bool bSelected)
{
	const bool bHover = R.IsInside(FVector2f(Mouse));
	FCanvasTileItem Bg(FVector2D(R.Min), FVector2D(R.GetSize()),
		bSelected ? FLinearColor(0.93f, 0.78f, 0.24f, 0.18f) : FLinearColor(1.f, 1.f, 1.f, bHover ? 0.10f : 0.04f));
	Bg.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Bg);
	FCanvasBoxItem Frame(FVector2D(R.Min), FVector2D(R.GetSize()));
	Frame.SetColor(bHover || bSelected ? MenuGold.CopyWithNewOpacity(0.8f) : FLinearColor(0.4f, 0.4f, 0.4f, 0.6f));
	Canvas->DrawItem(Frame);
	const float Size = 19.f;
	DrawText(Canvas, Label, R.Min.X + (R.GetSize().X - TextWidth(Label, Size, 1)) * 0.5f, R.Min.Y + (R.GetSize().Y - Size * 1.25f * UIScale) * 0.5f,
		Size, bHover || bSelected ? MenuGold : MenuTan, 1);
	Buttons.Add({R, Id});
	return bHover;
}

void FNWGameMenu::Draw(UCanvas* Canvas, const FVector2D& Mouse)
{
	if (!bOpen)
	{
		return;
	}
	UIScale = Canvas->ClipY / 1080.f;
	Buttons.Reset();
	// Escurece a tela inteira (ScreenScrim do jogo).
	FCanvasTileItem Scrim(FVector2D::ZeroVector, FVector2D(Canvas->ClipX, Canvas->ClipY), FLinearColor(0.f, 0.f, 0.f, 0.55f));
	Scrim.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Scrim);
	if (Page == 0)
	{
		DrawMain(Canvas, Mouse);
	}
	else
	{
		DrawOptions(Canvas, Mouse);
	}
}

void FNWGameMenu::DrawMain(UCanvas* Canvas, const FVector2D& Mouse)
{
	const float S = UIScale;
	// Janela do Esc do jogo: 840 x 800 no centro; aqui só a coluna "Jogo" / "Sair do Jogo" (sem loja nem suporte).
	const FVector2f C(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	Panel = FBox2f(C - FVector2f(260.f, 335.f) * S, C + FVector2f(260.f, 335.f) * S);
	DrawPanel(Canvas, Panel);
	const float X = Panel.Min.X + 50.f * S, W = Panel.GetSize().X - 100.f * S, H = 56.f * S;
	float Y = Panel.Min.Y + 50.f * S;
	auto Header = [&](const TCHAR* Key)
	{
		DrawText(Canvas, NWText::Get(Key), X, Y, 22.f, MenuDim, 1);
		FCanvasTileItem Line(FVector2D(X, Y + 32.f * S), FVector2D(W, 1.5f * S), MenuDim.CopyWithNewOpacity(0.5f));
		Line.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Line);
		Y += 48.f * S;
	};
	Header(TEXT("ui_escapemenu_game"));
	Button(Canvas, FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + H)), NWText::Get(TEXT("ui_resume")), IdResume, Mouse);
	Y += H + 12.f * S;
	Button(Canvas, FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + H)), NWText::Get(TEXT("ui_navmenu_settings")), IdSettings, Mouse);
	Y += H + 12.f * S;
	Button(Canvas, FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + H)), NWText::Get(TEXT("ui_escapemenu_unstuck")), IdUnstuck, Mouse);
	Y += H + 50.f * S;
	Header(TEXT("ui_escapemenu_leave_game"));
	Button(Canvas, FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + H)), NWText::Get(TEXT("ui_escapemenu_exit_to_main_menu")), IdExitMenu, Mouse);
	Y += H + 12.f * S;
	Button(Canvas, FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + H)), NWText::Get(TEXT("ui_escapemenu_exit_to_desktop")), IdExitDesktop, Mouse);
}

void FNWGameMenu::OptionRow(UCanvas* Canvas, float Y, const FString& Label, const FString& Value, float Bar, int32 Id, const FVector2D& Mouse)
{
	const float S = UIScale;
	const float X = Panel.Min.X + 60.f * S, Right = Panel.Max.X - 60.f * S, H = 46.f * S;
	const FBox2f Row(FVector2f(X, Y), FVector2f(Right, Y + H));
	if (Row.IsInside(FVector2f(Mouse)))
	{
		FCanvasTileItem Hl(FVector2D(Row.Min), FVector2D(Row.GetSize()), FLinearColor(1.f, 1.f, 1.f, 0.05f));
		Hl.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Hl);
	}
	DrawText(Canvas, Label, X + 10.f * S, Y + 11.f * S, 18.f, MenuTan);
	// Controle à direita: [<] valor [>] ou barra 0..1 com o valor ao lado.
	const float CW = 300.f * S, CX = Right - CW - 10.f * S;
	if (Bar >= 0.f)
	{
		const FBox2f Track(FVector2f(CX, Y + H * 0.5f - 3.f * S), FVector2f(CX + CW - 70.f * S, Y + H * 0.5f + 3.f * S));
		FCanvasTileItem T(FVector2D(Track.Min), FVector2D(Track.GetSize()), FLinearColor(1.f, 1.f, 1.f, 0.15f));
		T.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(T);
		FCanvasTileItem F(FVector2D(Track.Min), FVector2D(Track.GetSize().X * FMath::Clamp(Bar, 0.f, 1.f), Track.GetSize().Y), MenuGold.CopyWithNewOpacity(0.85f));
		F.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(F);
		const FBox2f Hit(FVector2f(Track.Min.X, Y), FVector2f(Track.Max.X, Y + H));
		Buttons.Add({Hit, IdRow + Id * 10 + 2});
		DrawText(Canvas, Value, Track.Max.X + 16.f * S, Y + 11.f * S, 18.f, MenuGold);
		return;
	}
	const FBox2f L(FVector2f(CX, Y + 6.f * S), FVector2f(CX + 34.f * S, Y + H - 6.f * S));
	const FBox2f R(FVector2f(CX + CW - 34.f * S, Y + 6.f * S), FVector2f(CX + CW, Y + H - 6.f * S));
	for (const auto& [Box, Arrow, Part] : {TTuple<FBox2f, const TCHAR*, int32>(L, TEXT("<"), 0), TTuple<FBox2f, const TCHAR*, int32>(R, TEXT(">"), 1)})
	{
		const bool bHover = Box.IsInside(FVector2f(Mouse));
		DrawText(Canvas, Arrow, Box.Min.X + 10.f * S, Box.Min.Y + 5.f * S, 20.f, bHover ? MenuGold : MenuDim, 1);
		Buttons.Add({Box, IdRow + Id * 10 + Part});
	}
	DrawText(Canvas, Value, CX + (CW - TextWidth(Value, 18.f)) * 0.5f, Y + 11.f * S, 18.f, MenuGold);
}

void FNWGameMenu::DrawOptions(UCanvas* Canvas, const FVector2D& Mouse)
{
	const float S = UIScale;
	const FNWOptions& O = FNWOptions::Get();
	const FVector2f C(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	Panel = FBox2f(C - FVector2f(520.f, 400.f) * S, C + FVector2f(520.f, 400.f) * S);
	DrawPanel(Canvas, Panel);
	DrawText(Canvas, NWText::Get(TEXT("ui_navmenu_settings")), Panel.Min.X + 60.f * S, Panel.Min.Y + 34.f * S, 32.f, MenuTan, 1);
	// Abas do jogo: Vídeo, Áudio, Jogo, Atalhos de Teclado e Mouse.
	static const TCHAR* Tabs[] = {TEXT("ui_options_visuals"), TEXT("ui_options_audio"), TEXT("ui_options_gameplay"), TEXT("ui_options_kbm_controls")};
	float TX = Panel.Min.X + 60.f * S;
	for (int32 t = 0; t < 4; ++t)
	{
		const FString L = NWText::Get(Tabs[t]);
		const float W = FMath::Max(150.f * S, TextWidth(L, 19.f, 1) + 40.f * S);
		Button(Canvas, FBox2f(FVector2f(TX, Panel.Min.Y + 92.f * S), FVector2f(TX + W, Panel.Min.Y + 140.f * S)), L, IdTab + t, Mouse, Tab == t);
		TX += W + 10.f * S;
	}
	float Y = Panel.Min.Y + 170.f * S;
	const float Step = 54.f * S;
	static const TCHAR* Quality[] = {TEXT("ui_options_graphics_low"), TEXT("ui_options_graphics_medium"), TEXT("ui_options_graphics_high"), TEXT("ui_options_graphics_vhigh")};
	static const TCHAR* Windows[] = {TEXT("ui_fullscreen"), TEXT("ui_windowed"), TEXT("ui_windowed_fullscreen")};
	if (Tab == 0)
	{
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_graphics_settings")), NWText::Get(Quality[O.Quality]), -1.f, 0, Mouse); Y += Step;
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_window_mode")), NWText::Get(Windows[FMath::Clamp(O.WindowMode, 0, 2)]), -1.f, 1, Mouse); Y += Step;
		const FString Fps = O.MaxFps > 0 ? NWText::Get(*FString::Printf(TEXT("ui_options_graphics_%dfps"), O.MaxFps)) : NWText::Get(TEXT("ui_options_graphics_uncappedfps"));
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_max_fps")), Fps, -1.f, 2, Mouse); Y += Step;
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_enable_vsync")), OnOff(O.bVSync), -1.f, 3, Mouse); Y += Step;
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_options_fov")), FString::Printf(TEXT("%.0f"), O.FovVertical), (O.FovVertical - 40.f) / 40.f, 4, Mouse); Y += Step;
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_enable_motionblur")), OnOff(O.bMotionBlur), -1.f, 5, Mouse); Y += Step;
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_show_fps")), OnOff(O.bShowFps), -1.f, 6, Mouse);
	}
	else if (Tab == 1)
	{
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_mastervolume")), Percent(O.MasterVolume), O.MasterVolume, 10, Mouse); Y += Step;
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_musicvolume")), Percent(O.MusicVolume), O.MusicVolume, 11, Mouse); Y += Step;
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_sfxvolume")), Percent(O.SfxVolume), O.SfxVolume, 12, Mouse); Y += Step;
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_ambvolume")), Percent(O.AmbienceVolume), O.AmbienceVolume, 13, Mouse);
	}
	else if (Tab == 3)
	{
		DrawKeys(Canvas, Mouse);
	}
	else
	{
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_language")), NWText::Lang() == TEXT("pt-br") ? TEXT("Português") : TEXT("English"), -1.f, 20, Mouse); Y += Step;
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_looksensitivityhorizontal")), FString::Printf(TEXT("%.1f"), O.SensitivityX), (O.SensitivityX - 0.2f) / 2.8f, 21, Mouse); Y += Step;
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_looksensitivityvertical")), FString::Printf(TEXT("%.1f"), O.SensitivityY), (O.SensitivityY - 0.2f) / 2.8f, 22, Mouse); Y += Step;
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_invertlook")), OnOff(O.bInvertY), -1.f, 23, Mouse); Y += Step;
		OptionRow(Canvas, Y, NWText::Get(TEXT("ui_options_damage_numbers_display_mode")),
			NWText::Get(O.DamageNumbers == 1 ? TEXT("ui_options_damage_numbers_display_mode_stacked") : TEXT("ui_options_damage_numbers_display_mode_floating")), -1.f, 24, Mouse);
	}
	const float BW = 220.f * S, BH = 52.f * S;
	Button(Canvas, FBox2f(FVector2f(Panel.Max.X - 60.f * S - BW, Panel.Max.Y - 50.f * S - BH), FVector2f(Panel.Max.X - 60.f * S, Panel.Max.Y - 50.f * S)),
		NWText::Get(TEXT("ui_back")), IdBack, Mouse);
}

void FNWGameMenu::ChangeOption(int32 Id, int32 Dir, float BarValue)
{
	FNWOptions& O = FNWOptions::Get();
	auto Cycle = [Dir](int32 V, int32 N) { return (V + Dir + N) % N; };
	auto Bar = [&](float& V, float Min, float Max, float StepSize)
	{
		V = BarValue >= 0.f ? Min + BarValue * (Max - Min) : V + Dir * StepSize;
		V = FMath::Clamp(V, Min, Max);
	};
	switch (Id)
	{
	case 0: O.Quality = Cycle(O.Quality, 4); break;
	case 1: O.WindowMode = Cycle(O.WindowMode, 3); break;
	case 2: O.MaxFps = FpsSteps[Cycle(FMath::Max(0, FpsSteps.IndexOfByKey(O.MaxFps)), FpsSteps.Num())]; break;
	case 3: O.bVSync = !O.bVSync; break;
	case 4: Bar(O.FovVertical, 40.f, 80.f, 1.f); O.FovVertical = FMath::RoundToFloat(O.FovVertical); break;
	case 5: O.bMotionBlur = !O.bMotionBlur; break;
	case 6: O.bShowFps = !O.bShowFps; break;
	case 10: Bar(O.MasterVolume, 0.f, 1.f, 0.05f); break;
	case 11: Bar(O.MusicVolume, 0.f, 1.f, 0.05f); break;
	case 12: Bar(O.SfxVolume, 0.f, 1.f, 0.05f); break;
	case 13: Bar(O.AmbienceVolume, 0.f, 1.f, 0.05f); break;
	case 20: NWText::SetLang(NWText::Lang() == TEXT("pt-br") ? TEXT("en") : TEXT("pt")); break;
	case 21: Bar(O.SensitivityX, 0.2f, 3.f, 0.1f); break;
	case 22: Bar(O.SensitivityY, 0.2f, 3.f, 0.1f); break;
	case 23: O.bInvertY = !O.bInvertY; break;
	case 24: O.DamageNumbers = O.DamageNumbers == 1 ? 0 : 1; break;
	default: return;
	}
	O.Save();
}

FNWGameMenu::EAction FNWGameMenu::Click(const FVector2D& Mouse)
{
	const FButton* B = Buttons.FindByPredicate([&](const FButton& X) { return X.Rect.IsInside(FVector2f(Mouse)); });
	if (!B)
	{
		return EAction::None;
	}
	switch (B->Id)
	{
	case IdResume: return EAction::Resume;
	case IdSettings: Page = 1; return EAction::None;
	case IdUnstuck: return EAction::Unstuck;
	case IdExitMenu: return EAction::ExitToMenu;
	case IdExitDesktop: return EAction::ExitToDesktop;
	case IdBack: Page = 0; WaitingKey = NAME_None; return EAction::None;
	default: break;
	}
	if (B->Id >= IdTab && B->Id < IdTab + 4)
	{
		Tab = B->Id - IdTab;
		WaitingKey = NAME_None;
		return EAction::None;
	}
	if (B->Id == IdResetKeys)
	{
		FNWOptions::Get().ResetKeys();
		FNWOptions::Get().Save();
		WaitingKey = NAME_None;
		bKeysChanged = true;
		return EAction::None;
	}
	if (B->Id >= IdKey && B->Id < IdRow)
	{
		const TArray<FNWKeyAction>& Actions = FNWOptions::KeyActions();
		const int32 i = B->Id - IdKey;
		WaitingKey = Actions.IsValidIndex(i) ? Actions[i].Id : NAME_None;
		return EAction::None;
	}
	if (B->Id >= IdRow)
	{
		const int32 Row = (B->Id - IdRow) / 10, Part = (B->Id - IdRow) % 10;
		const float BarValue = Part == 2 ? FMath::Clamp((float(Mouse.X) - B->Rect.Min.X) / B->Rect.GetSize().X, 0.f, 1.f) : -1.f;
		ChangeOption(Row, Part == 0 ? -1 : 1, BarValue);
		return EAction::None;
	}
	return EAction::None;
}

void FNWGameMenu::DrawKeys(UCanvas* Canvas, const FVector2D& Mouse)
{
	// Duas colunas: ação (texto do jogo) e a tecla; clicar espera a próxima tecla (Esc cancela).
	const float S = UIScale;
	const TArray<FNWKeyAction>& Actions = FNWOptions::KeyActions();
	const FNWOptions& O = FNWOptions::Get();
	const int32 PerColumn = (Actions.Num() + 1) / 2;
	const float ColW = (Panel.GetSize().X - 140.f * S) * 0.5f, H = 46.f * S, Top = Panel.Min.Y + 166.f * S;
	for (int32 i = 0; i < Actions.Num(); ++i)
	{
		const float X = Panel.Min.X + 60.f * S + (i / PerColumn) * (ColW + 20.f * S);
		const float Y = Top + (i % PerColumn) * (H + 4.f * S);
		const FBox2f Row(FVector2f(X, Y), FVector2f(X + ColW, Y + H));
		const bool bWaiting = WaitingKey == Actions[i].Id;
		if (bWaiting || Row.IsInside(FVector2f(Mouse)))
		{
			FCanvasTileItem Hl(FVector2D(Row.Min), FVector2D(Row.GetSize()), FLinearColor(1.f, 1.f, 1.f, bWaiting ? 0.1f : 0.05f));
			Hl.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Hl);
		}
		DrawText(Canvas, NWText::Get(Actions[i].LabelKey), X + 10.f * S, Y + 12.f * S, 17.f, MenuTan);
		const FString Key = bWaiting ? FString(TEXT("...")) : O.KeyFor(Actions[i].Id).GetDisplayName().ToString();
		DrawText(Canvas, Key, Row.Max.X - 12.f * S - TextWidth(Key, 17.f, 1), Y + 12.f * S, 17.f, bWaiting ? MenuTan : MenuGold, 1);
		Buttons.Add({Row, IdKey + i});
	}
	const float BW = 260.f * S, BH = 52.f * S;
	Button(Canvas, FBox2f(FVector2f(Panel.Min.X + 60.f * S, Panel.Max.Y - 50.f * S - BH), FVector2f(Panel.Min.X + 60.f * S + BW, Panel.Max.Y - 50.f * S)),
		NWText::Get(TEXT("ui_restore_defaults")), IdResetKeys, Mouse);
}

void FNWGameMenu::AssignKey(const FKey& Key)
{
	if (WaitingKey.IsNone())
	{
		return;
	}
	FNWOptions& O = FNWOptions::Get();
	O.SetKey(WaitingKey, Key);
	O.Save();
	WaitingKey = NAME_None;
	bKeysChanged = true;
}

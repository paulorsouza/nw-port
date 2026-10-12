#include "NWTradeskillMenu.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "NWCharacter.h"
#include "NWProgressionComponent.h"
#include "NWText.h"

FBox2f FNWTradeskillMenu::TabRects[2] = {FBox2f(ForceInit), FBox2f(ForceInit)};

bool FNWTradeskillMenu::Load()
{
	bLoaded = true;
	if (!Layout.Load(NWText::DataPath(TEXT("Data/UI/tradeskills_layout.json"))))
	{
		return false;
	}
	Layout.ResolveTexture = [](const FString& P) { return FNWUILayout::GameTexture(P); };
	for (int32 i = 0; i < 3; ++i)
	{
		Layout.Fonts[i] = FNWUILayout::GameFont(i);
	}
	return true;
}

void FNWTradeskillMenu::Open(ANWCharacter* InCharacter)
{
	if (!bLoaded)
	{
		Load();
	}
	if (Layout.IsLoaded())
	{
		Character = InCharacter;
	}
}

void FNWTradeskillMenu::Draw(UCanvas* Canvas, const FVector2D& Mouse)
{
	const ANWCharacter* Me = Character.Get();
	if (!Me || !Canvas)
	{
		return;
	}
	const UNWProgressionComponent* P = Me->Progression;
	// Escurece o fundo como as outras janelas cheias do jogo.
	FCanvasTileItem Dim(FVector2D::ZeroVector, FVector2D(Canvas->ClipX, Canvas->ClipY), FLinearColor(0.f, 0.f, 0.f, 0.5f));
	Dim.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Dim);
	static const TCHAR* Cells[NWTradeskill::Num] = {TEXT("MiningCell"), TEXT("LoggingCell"), TEXT("HarvestingCell"), TEXT("SkinningCell")};
	for (int32 t = 0; t < NWTradeskill::Num; ++t)
	{
		const FString Cell = FString(TEXT("GatheringSection/")) + Cells[t];
		if (FNWUIElement* E = Layout.FindOne(TEXT("tradeskills"), Cell + TEXT("/SkillLevelCircleProgress/TextContainer/LevelText")))
		{
			E->TextOverride = FString::FromInt(P ? P->TradeLevel(t) : 0);
		}
		for (const TCHAR* Ring : {TEXT("/CircleBg/CircleProgress"), TEXT("/CircleBg/InnerCircleProgress")})
		{
			if (FNWUIElement* E = Layout.FindOne(TEXT("tradeskills"), Cell + TEXT("/SkillLevelCircleProgress/ScaleContainer") + Ring))
			{
				E->Fill = P ? P->TradeProgress(t) : 0.f;
			}
		}
		// Destaque do jogo (skillcellhover) com o mouse em cima.
		FNWUIElement* Root = Layout.FindOne(TEXT("tradeskills"), Cell);
		if (FNWUIElement* Hl = Layout.FindOne(TEXT("tradeskills"), Cell + TEXT("/Highlight")))
		{
			const FBox2f R = Root ? Layout.RectOf(Root) : FBox2f(ForceInit);
			Hl->bEnabled = true;
			Hl->Visible = R.bIsValid && R.IsInside(FVector2f(Mouse));
		}
	}
	Layout.Draw(Canvas);
	DrawTabs(Canvas, Mouse, 1);
}

void FNWTradeskillMenu::DrawTabs(UCanvas* Canvas, const FVector2D& Mouse, int32 Active)
{
	// TabbedList do jogo (no topo da janela): botões escuros com moldura; a aba atual fica em dourado.
	const float S = Canvas->ClipY / 1080.f;
	const float W = 300.f * S, H = 44.f * S, Y = 100.f * S;
	const float X0 = Canvas->ClipX * 0.5f - W - 6.f * S;
	static const TCHAR* Labels[2] = {TEXT("ui_weapon_mastery"), TEXT("ui_trade_skills")};
	UFont* Font = FNWUILayout::GameFont(1);
	for (int32 i = 0; i < 2; ++i)
	{
		TabRects[i] = FBox2f(FVector2f(X0 + i * (W + 12.f * S), Y), FVector2f(X0 + i * (W + 12.f * S) + W, Y + H));
		const bool bHover = TabRects[i].IsInside(FVector2f(Mouse));
		auto Tex = [&](const TCHAR* Path, const FLinearColor& Tint)
		{
			if (UTexture2D* T = FNWUILayout::GameTexture(Path))
			{
				FCanvasTileItem Item(FVector2D(TabRects[i].Min), T->GetResource(), FVector2D(TabRects[i].GetSize()), Tint);
				Item.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Item);
			}
		};
		Tex(TEXT("lyshineui/images/slices/button/buttonbg.dds"), FLinearColor(0.17f, 0.15f, 0.11f));
		if (bHover || i == Active)
		{
			Tex(TEXT("lyshineui/images/slices/button/focusnew.dds"), FLinearColor(1.f, 1.f, 1.f, i == Active ? 1.f : 0.6f));
		}
		Tex(TEXT("lyshineui/images/slices/button/heroframedisabled2023.dds"), FLinearColor::White);
		if (Font)
		{
			const FString Text = NWText::Get(Labels[i]);
			const float Scale = 20.f * S / FMath::Max(1, Font->LegacyFontSize);
			const float TW = Font->GetStringSize(*Text) * Scale;
			FCanvasTextItem Item(FVector2D(TabRects[i].GetCenter().X - TW * 0.5f, Y + 11.f * S), FText::FromString(Text), Font,
				i == Active ? FLinearColor(FColor(0xED, 0xC6, 0x3E)) : FLinearColor(FColor(0xEB, 0xE3, 0xD6)));
			Item.Scale = FVector2D(Scale, Scale);
			Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f));
			Canvas->DrawItem(Item);
		}
	}
}

int32 FNWTradeskillMenu::TabAt(const FVector2D& Mouse)
{
	for (int32 i = 0; i < 2; ++i)
	{
		if (TabRects[i].bIsValid && TabRects[i].IsInside(FVector2f(Mouse)))
		{
			return i;
		}
	}
	return INDEX_NONE;
}

#include "NWAttributesMenu.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "NWCharacter.h"
#include "NWProgressionComponent.h"
#include "NWText.h"

static const TCHAR* AttrCanvas = TEXT("attributes");

bool FNWAttributesMenu::Load()
{
	bLoaded = true;
	Pending.Init(0, 5);
	if (!Layout.Load(NWText::DataPath(TEXT("Data/UI/attributes_layout.json"))))
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

void FNWAttributesMenu::Open(ANWCharacter* InCharacter)
{
	if (!bLoaded)
	{
		Load();
	}
	if (Layout.IsLoaded())
	{
		Character = InCharacter;
		Pending.Init(0, 5);
	}
}

FString FNWAttributesMenu::Row(int32 i) const
{
	return i == 0 ? FString(TEXT("AttributeModifiers/Attribute")) : FString::Printf(TEXT("AttributeModifiers/Attribute%d"), i + 1);
}

int32 FNWAttributesMenu::PendingTotal() const
{
	int32 Sum = 0;
	for (int32 P : Pending)
	{
		Sum += P;
	}
	return Sum;
}

void FNWAttributesMenu::Draw(UCanvas* Canvas, const FVector2D& Mouse)
{
	const ANWCharacter* Me = Character.Get();
	const UNWProgressionComponent* P = Me ? Me->Progression.Get() : nullptr;
	if (!P || !Canvas)
	{
		return;
	}
	FCanvasTileItem Dim(FVector2D::ZeroVector, FVector2D(Canvas->ClipX, Canvas->ClipY), FLinearColor(0.f, 0.f, 0.f, 0.5f));
	Dim.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Dim);
	const int32 Free = P->UnspentPoints() - PendingTotal();
	if (FNWUIElement* E = Layout.FindOne(AttrCanvas, TEXT("PointsAvailableContainer/PointsAvailableNumber"))) E->TextOverride = FString::FromInt(Free);
	auto Hover = [&](const FString& Button)
	{
		FNWUIElement* B = Layout.FindOne(AttrCanvas, Button);
		const FBox2f R = B ? Layout.RectOf(B) : FBox2f(ForceInit);
		const bool bOn = R.bIsValid && R.IsInside(FVector2f(Mouse));
		if (FNWUIElement* F = Layout.FindOne(AttrCanvas, Button + TEXT("/buttonFocus")))
		{
			F->bEnabled = bOn;
		}
	};
	for (int32 i = 0; i < 5; ++i)
	{
		const FString R = Row(i);
		if (FNWUIElement* E = Layout.FindOne(AttrCanvas, R + TEXT("/Value"))) E->TextOverride = FString::FromInt(P->Attribute(ENWAttribute(i)));
		if (FNWUIElement* E = Layout.FindOne(AttrCanvas, R + TEXT("/PendingValue")))
		{
			E->bEnabled = Pending[i] > 0;
			E->TextOverride = FString::Printf(TEXT("+%d"), Pending[i]);
		}
		Hover(R + TEXT("/SubtractButton"));
		Hover(R + TEXT("/AddButton"));
	}
	Hover(TEXT("Attributes/Confirm"));
	// Level XP bar (XpDetails/XpMeter, same as the HUD's) and "N EXP to Level X".
	const float Fill = P->LevelProgress();
	if (FNWUIElement* E = Layout.FindOne(AttrCanvas, TEXT("XpMeter/LevelPositioner/LevelText"))) E->TextOverride = FString::FromInt(P->Level);
	if (FNWUIElement* E = Layout.FindOne(AttrCanvas, TEXT("XpMeter/MeterFillPositioner/MeterFillMask/MeterFill"))) E->Fill = Fill;
	if (FNWUIElement* E = Layout.FindOne(AttrCanvas, TEXT("MeterFillMask/MeterFillEdge")))
	{
		E->Translate = FVector2f(1588.f * Fill, 0.f);
		E->Visible = Fill > 0.f && Fill < 1.f;
	}
	if (FNWUIElement* E = Layout.FindOne(AttrCanvas, TEXT("XpDetails/XpToNextLevel")))
	{
		E->TextOverride = P->Level >= P->MaxLevel() ? FString()
			: NWText::Get(TEXT("ui_glory_to_level")).Replace(TEXT("{xp}"), *FString::Printf(TEXT("%lld"), P->XpForLevel(P->Level + 1) - P->Xp))
				.Replace(TEXT("{level}"), *FString::FromInt(P->Level + 1));
	}
	Layout.Draw(Canvas);

	// Bar for each attribute (the game's attributebarbg): up to 300, with marks at each 50 threshold and the pending in green.
	const float S = Canvas->ClipY / 1080.f;
	UTexture2D* BarBg = FNWUILayout::GameTexture(TEXT("lyshineui/images/attributes/attributebarbg.dds"));
	for (int32 i = 0; i < 5; ++i)
	{
		const FNWUIElement* RowEl = Layout.FindOne(AttrCanvas, Row(i));
		const FBox2f RR = RowEl ? Layout.RectOf(RowEl) : FBox2f(ForceInit);
		if (!RR.bIsValid)
		{
			continue;
		}
		const FVector2D Pos(RR.Min.X + 2.f * S, RR.Min.Y + 68.f * S);
		const FVector2D Size(882.f * S, 11.f * S);
		if (BarBg)
		{
			FCanvasTileItem Bg(Pos, BarBg->GetResource(), Size, FLinearColor::White);
			Bg.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Bg);
		}
		const int32 Value = P->Attribute(ENWAttribute(i));
		const float Base = FMath::Clamp(Value / 300.f, 0.f, 1.f), WithPending = FMath::Clamp((Value + Pending[i]) / 300.f, 0.f, 1.f);
		const FVector2D Inner = Pos + FVector2D(0.f, 4.f * S);
		const float H = 4.f * S;
		FCanvasTileItem Pend(Inner, FVector2D(Size.X * WithPending, H), FLinearColor(0.f, 0.67f, 0.f, 1.f));
		Canvas->DrawItem(Pend);
		FCanvasTileItem Fill2(Inner, FVector2D(Size.X * Base, H), FLinearColor(0.67f, 0.67f, 0.f, 1.f));
		Canvas->DrawItem(Fill2);
		for (int32 T = 50; T < 300; T += 50)
		{
			FCanvasTileItem Tick(Inner + FVector2D(Size.X * T / 300.f - 1.f * S, -3.f * S), FVector2D(2.f * S, H + 6.f * S), FLinearColor(0.f, 0.f, 0.f, 1.f));
			Canvas->DrawItem(Tick);
		}
	}
}

void FNWAttributesMenu::Click(const FVector2D& Mouse)
{
	ANWCharacter* Me = Character.Get();
	UNWProgressionComponent* P = Me ? Me->Progression.Get() : nullptr;
	if (!P)
	{
		return;
	}
	auto Hit = [&](const FString& Path)
	{
		const FNWUIElement* E = Layout.FindOne(AttrCanvas, Path);
		const FBox2f R = E ? Layout.RectOf(E) : FBox2f(ForceInit);
		return R.bIsValid && R.IsInside(FVector2f(Mouse));
	};
	for (int32 i = 0; i < 5; ++i)
	{
		if (Hit(Row(i) + TEXT("/AddButton")) && P->UnspentPoints() - PendingTotal() > 0)
		{
			++Pending[i];
			return;
		}
		if (Hit(Row(i) + TEXT("/SubtractButton")) && Pending[i] > 0)
		{
			--Pending[i];
			return;
		}
	}
	if (Hit(TEXT("Attributes/Confirm")) && PendingTotal() > 0)
	{
		P->ServerSpendPoints(Pending);
		Pending.Init(0, 5);
	}
}

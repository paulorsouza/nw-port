#include "NWShopMenu.h"

#include "Engine/Canvas.h"
#include "NWCharacter.h"
#include "NWInteractable.h"
#include "NWInventoryComponent.h"
#include "NWItems.h"
#include "NWNpcRewards.h"
#include "NWPlayerController.h"
#include "NWProgressionComponent.h"
#include "NWShop.h"
#include "NWText.h"

namespace
{
	// Game colors (_common/uistyle.lua).
	const FLinearColor ShopTanWarm(FColor(0xEB, 0xE3, 0xD6));
	const FLinearColor ShopGray50(FColor(0x80, 0x80, 0x80));
	const FLinearColor ShopRedNew(FColor(0xFF, 0x4E, 0x4E));
	const TCHAR* ShopImg = TEXT("lyshineui/images/");
	// shopgriditem.lua: HORIZONTAL_SPACING 11, VERTICAL_SPACING 12; 290×326 cell (ShopGridItem).
	constexpr float CellGapX = 11.f;
	constexpr float CellGapY = 12.f;
	// Hero button loop: 30 frames at 30 fps (as in refining).
	constexpr int32 ShopLoopFrames = 30;

	FString ShopTex(const TCHAR* Rel) { return FString(ShopImg) + Rel; }
	void ShopSetText(FNWUIElement* El, const FString& T, TOptional<FLinearColor> Color = {})
	{
		if (El)
		{
			El->TextOverride = T;
			if (Color.IsSet())
			{
				El->Tint = Color.GetValue();
			}
		}
	}
	void ShopShow(FNWUIElement* El, bool bOn) { if (El) { El->Visible = bOn; } }
}

bool FNWShopMenu::Load()
{
	auto Init = [](FNWUILayout& L, const TCHAR* File)
	{
		if (!L.Load(NWText::DataPath(File)))
		{
			return false;
		}
		L.ResolveTexture = [](const FString& P) { return FNWUILayout::GameTexture(P); };
		L.bExactText = true;
		for (int32 i = 0; i < 3; ++i)
		{
			L.Fonts[i] = FNWUILayout::GameFont(i);
		}
		return true;
	};
	if (!Init(Ui, TEXT("Data/UI/shop_layout.json")) || !Init(Head, TEXT("Data/UI/screen_header_layout.json"))
		|| !Init(Help, TEXT("Data/UI/helpbar_layout.json")))
	{
		return false;
	}
	for (int32 i = 0; i < ShopLoopFrames; ++i)
	{
		FNWUILayout::GameTexture(ShopTex(*FString::Printf(TEXT("slices/button/blue_loop/blue_%02d.dds"), i)));
		FNWUILayout::GameTexture(ShopTex(*FString::Printf(TEXT("slices/button/gray_loop/gray_%02d.dds"), i)));
	}
	return true;
}

void FNWShopMenu::Open(ANWCharacter* InCharacter, const FString& InShopId, const FString& InTitle)
{
	if (!bLoaded)
	{
		bLoaded = Load();
	}
	if (!bLoaded || !FNWShops::Get().Find(InShopId))
	{
		return;
	}
	Character = InCharacter;
	ShopId = InShopId;
	Title = InTitle;
	Detail = INDEX_NONE;
	Count = 1;
	bDragging = false;
}

const FNWShop* FNWShopMenu::Shop() const
{
	return FNWShops::Get().Find(ShopId);
}

void FNWShopMenu::Select(FName Item, int32 InCount)
{
	const FNWShop* S = Shop();
	Detail = S ? S->Items.IndexOfByPredicate([Item](const FNWShopItem& I) { return I.Id == Item; }) : INDEX_NONE;
	Count = FMath::Max(1, InCount);
}

void FNWShopMenu::Dump(const FString& Filter) const
{
	Ui.Dump(Filter);
	Head.Dump(Filter);
}

FString FNWShopMenu::TimeToReset()
{
	// Same week as the gifts (FNWNpcRewards::Period): Tuesday 08:00 UTC.
	const FDateTime NowUtc = FDateTime::UtcNow();
	const FDateTime Shifted = NowUtc - FTimespan::FromHours(8.0);
	const int32 Back = (int32(Shifted.GetDayOfWeek()) - int32(EDayOfWeek::Tuesday) + 7) % 7;
	const FDateTime Next = Shifted.GetDate() - FTimespan::FromDays(Back) + FTimespan::FromDays(7) + FTimespan::FromHours(8.0);
	const FTimespan Left = Next - NowUtc;
	return Left.GetDays() > 0 ? FString::Printf(TEXT("%dd %dh"), Left.GetDays(), Left.GetHours())
		: FString::Printf(TEXT("%dh %dm"), Left.GetHours(), Left.GetMinutes());
}

int32 FNWShopMenu::MaxCount(int32 Index, int32* OutLimitLeft) const
{
	const ANWCharacter* Me = Character.Get();
	const FNWShop* S = Shop();
	if (OutLimitLeft)
	{
		*OutLimitLeft = -1;
	}
	if (!Me || !Me->Inventory || !S || !S->Items.IsValidIndex(Index))
	{
		return 0;
	}
	const FNWShopItem& I = S->Items[Index];
	int32 Max = I.Price > 0 ? int32(FMath::Min<int64>(Me->Inventory->Coins / I.Price, 9999)) : 9999;
	if (I.WeeklyLimit > 0)
	{
		const int32 Bought = Me->Progression ? Me->Progression->ShopBought(I.Id, FNWNpcRewards::Get().Period()) : 0;
		const int32 Left = FMath::Max(0, I.WeeklyLimit - Bought);
		if (OutLimitLeft)
		{
			*OutLimitLeft = Left;
		}
		Max = FMath::Min(Max, Left);
	}
	if (const FNWItemDef* D = FNWItemCatalog::Get().Find(I.Id))
	{
		if (D->Weight > 0.f)
		{
			Max = FMath::Min(Max, FMath::FloorToInt((Me->Inventory->MaxWeight - Me->Inventory->TotalWeight()) / D->Weight));
		}
	}
	return FMath::Max(0, Max);
}

void FNWShopMenu::DrawGrid(UCanvas* Canvas, const FVector2D& Mouse)
{
	CellRects.Reset();
	FNWUIElement* Proto = E(TEXT("ShopItemListContainer/ShopGridItem"));
	FNWUIElement* Grid = E(TEXT("ShopItemListContainer/SimpleDynamicGrid"));
	const FNWShop* S = Shop();
	if (!Proto || !Grid || !S)
	{
		return;
	}
	const float UIScale = Canvas->ClipY / 1080.f;
	const FBox2f ProtoRect = Ui.RectOf(Proto), GridRect = Ui.RectOf(Grid);
	const FVector2f Base((GridRect.Min - ProtoRect.Min) / UIScale);
	const FVector2f Cell(ProtoRect.GetSize() / UIScale);
	const int32 Cols = FMath::Max(1, int32((GridRect.GetSize().X / UIScale + CellGapX) / (Cell.X + CellGapX)));
	auto At = [this](const TCHAR* Off) { return Ui.FindOne(TEXT("shop"), FString(TEXT("ShopItemListContainer/ShopGridItem/GridItemContainer")) + Off); };
	for (int32 i = 0; i < S->Items.Num(); ++i)
	{
		const FNWShopItem& I = S->Items[i];
		const FNWItemDef* D = FNWItemCatalog::Get().Find(I.Id);
		const FVector2f Offset = Base + FVector2f((i % Cols) * (Cell.X + CellGapX), (i / Cols) * (Cell.Y + CellGapY));
		const FBox2f Guess(ProtoRect.Min + Offset * UIScale, ProtoRect.Max + Offset * UIScale);
		if (FNWUIElement* Icon = At(TEXT("/Mask/ItemIcon")))
		{
			Icon->ImageType = 4;   // StretchedToFit: square icon in the card strip
			Icon->TextureOverride = D ? D->Icon : FString();
		}
		ShopSetText(At(TEXT("/ItemName")), D ? D->Name : I.Id.ToString());
		ShopSetText(At(TEXT("/CurrencyContainer/Currency1/Amount")), UNWInventoryComponent::FormatCoins(I.Price));
		// UpdateCooldown: red clock (COLOR_RED_NEW) when the week's limit is used up.
		int32 Left = -1;
		MaxCount(i, &Left);
		const bool bLimit = Left == 0;
		ShopShow(At(TEXT("/CooldownText")), bLimit);
		ShopSetText(At(TEXT("/CooldownText")), TimeToReset(), ShopRedNew);
		if (FNWUIElement* Ic = At(TEXT("/CooldownText/CooldownIcon")))
		{
			Ic->Tint = ShopRedNew;
		}
		ShopShow(At(TEXT("/HoverFrame")), Guess.IsInside(FVector2f(Mouse)));
		const FBox2f Drawn = Ui.DrawSubtree(Canvas, TEXT("shop"), Proto, Offset);
		CellRects.Add({Drawn, i});
	}
}

void FNWShopMenu::FillDetail()
{
	const ANWCharacter* Me = Character.Get();
	const FNWShop* S = Shop();
	if (!Me || !S || !S->Items.IsValidIndex(Detail))
	{
		return;
	}
	const FNWShopItem& I = S->Items[Detail];
	const FNWItemDef* D = FNWItemCatalog::Get().Find(I.Id);
	ShopSetText(E(TEXT("InfoPanel/Title")), D ? D->Name : I.Id.ToString());
	if (FNWUIElement* T = E(TEXT("InfoPanel/Title")))
	{
		// Category is a child of Title and the port does not stretch the title with the text: one line, shrinking to fit.
		T->bWrap = false;
		T->ShrinkToFit = 1;
		T->MinShrink = 0.6f;
	}
	ShopSetText(E(TEXT("Title/Category")), D ? D->TypeName : FString());
	ShopSetText(E(TEXT("Category/Description")), D ? D->Description : FString());
	if (FNWUIElement* Hi = E(TEXT("HiResPanel/HiRes2dItemImage")))
	{
		Hi->ImageType = 4;
		Hi->TextureOverride = D ? D->Icon : FString();
	}
	int32 LimitLeft = -1;
	const int32 Max = MaxCount(Detail, &LimitLeft);
	Count = FMath::Clamp(Count, 1, FMath::Max(1, Max));
	const bool bCan = Max > 0;

	// Quantity (SliderWithTextInput): current value in the box on the left, maximum on the right.
	ShopShow(E(TEXT("Slider/SliderText")), false);
	ShopSetText(E(TEXT("CurrentValueTextInput/CurrentValueText")), FString::FromInt(bCan ? Count : 0));
	ShopSetText(E(TEXT("SliderWithTextInput/MaxValueText")), FString::FromInt(Max));
	if (FNWUIElement* H = E(TEXT("Slider/SliderHandleHolder")))
	{
		const FNWUIElement* Track = E(TEXT("Slider/SliderTrack"));
		const float W = Track ? Track->Offsets.Z - Track->Offsets.X : 300.f;
		H->Translate.X = Max > 1 ? W * float(Count - 1) / float(Max - 1) : 0.f;
	}

	// No purchase: the reason in ButtonDisabledText (UpdateConfirmButton) and, at the limit, the time until renewal.
	FString Why;
	if (LimitLeft == 0)
	{
		Why = NWText::Format(TEXT("nw.shop.weekly_limit"), {FString::FromInt(I.WeeklyLimit), FString::FromInt(I.WeeklyLimit)});
	}
	else if (!bCan && Me->Inventory && Me->Inventory->Coins < I.Price)
	{
		Why = NWText::Get(TEXT("ui_insufficient_funds"));
	}
	else if (!bCan)
	{
		Why = NWText::Get(TEXT("ui_inventoryfull"));
	}
	else if (LimitLeft > 0)
	{
		Why = NWText::Format(TEXT("nw.shop.weekly_limit"), {FString::FromInt(I.WeeklyLimit - LimitLeft), FString::FromInt(I.WeeklyLimit)});
	}
	ShopShow(E(TEXT("InfoPanel/ButtonDisabledText")), !Why.IsEmpty() && LimitLeft != 0);
	ShopSetText(E(TEXT("InfoPanel/ButtonDisabledText")), Why, bCan ? ShopTanWarm : ShopRedNew);
	ShopShow(E(TEXT("InfoPanel/CooldownContainer")), LimitLeft == 0);
	for (const TCHAR* Off : {TEXT("CooldownContainer/ExclamationIconContainer"), TEXT("ExclamationIconContainer/ExclmationIcon"),
		TEXT("CooldownContainer/CooldownLabel"), TEXT("CooldownContainer/HourGlassIconContainer"), TEXT("HourGlassIconContainer/HourGlassIcon"),
		TEXT("CooldownContainer/CooldownTextUpdater")})
	{
		if (FNWUIElement* El = E(Off))
		{
			El->bEnabled = true;
			El->Visible = true;
		}
	}
	ShopSetText(E(TEXT("CooldownContainer/CooldownTextUpdater")), TimeToReset());

	// Hero button with the cost (GetCostStrings: coin icon + value, Pica 54 tan).
	const int64 Cost = I.Price * (bCan ? Count : 1);
	ShopSetText(E(TEXT("ConfirmButton/buttonText")), UNWInventoryComponent::FormatCoins(Cost), bCan ? ShopTanWarm : ShopGray50);
	if (FNWUIElement* T = E(TEXT("ConfirmButton/buttonText")))
	{
		T->FontSize = 54.f;
	}
	if (FNWUIElement* Ic = E(TEXT("ConfirmButton/buttonText/buttonIconHolder")))
	{
		Ic->Visible = true;
		Ic->Fade = 1.f;
		Ic->TextureOverride = ShopTex(TEXT("icon_crowns.dds"));
	}
	ShopShow(E(TEXT("buttonIconHolder/buttonIconValue")), false);
	ShopShow(E(TEXT("ConfirmButton/buttonText/buttonTextPrefix")), false);
	if (FNWUIElement* F = E(TEXT("ConfirmButton/frame")))
	{
		F->TextureOverride = ShopTex(bCan ? TEXT("slices/button/heroframe2023.dds") : TEXT("slices/button/heroframedisabled2023.dds"));
	}
	ShopShow(E(TEXT("ConfirmButton/heroImageSequence")), bCan);
	ShopShow(E(TEXT("ConfirmButton/heroImageSequenceDisabled")), !bCan);
	ShopShow(E(TEXT("ConfirmButton/buttonFocus")), false);
	ShopShow(E(TEXT("ConfirmButton/frameFocus")), false);
	const int32 Frame = int32(Now() * 30.0) % ShopLoopFrames;
	if (FNWUIElement* Seq = E(TEXT("ConfirmButton/heroImageSequence")))
	{
		Seq->Fade = 1.f;
		Seq->TextureOverride = ShopTex(*FString::Printf(TEXT("slices/button/blue_loop/blue_%02d.dds"), Frame));
	}
	if (FNWUIElement* Seq = E(TEXT("ConfirmButton/heroImageSequenceDisabled")))
	{
		Seq->Fade = 1.f;
		Seq->TextureOverride = ShopTex(*FString::Printf(TEXT("slices/button/gray_loop/gray_%02d.dds"), Frame));
	}
}

void FNWShopMenu::DrawHelp(UCanvas* Canvas)
{
	// Footer (helpbar): ENTER Chat on the left, ESC Back on the right (same drawing as refining).
	FNWUIElement* Proto = Help.FindOne(TEXT("helpbar"), TEXT("ActionsDisplays/ActionDisplay"));
	FNWUIElement* Hint = Help.FindOne(TEXT("helpbar"), TEXT("ActionsDisplays/ActionDisplay/Hint"));
	FNWUIElement* Key = Help.FindOne(TEXT("helpbar"), TEXT("ActionDisplay/Hint/HintText"));
	FNWUIElement* Label = Help.FindOne(TEXT("helpbar"), TEXT("ActionsDisplays/ActionDisplay/Text"));
	if (!Proto || !Hint || !Key || !Label)
	{
		return;
	}
	for (int32 n = 2; n <= 12; ++n)
	{
		ShopShow(Help.FindOne(TEXT("helpbar"), *FString::Printf(TEXT("ActionsDisplays/ActionDisplay%d"), n)), false);
	}
	for (FNWUIElement* El : Help.Find(TEXT("helpbar"), TEXT("GlobalActionDisplays")))
	{
		El->Visible = false;
	}
	Proto->Visible = false;
	Help.Draw(Canvas);
	const float UIScale = Canvas->ClipY / 1080.f;
	const float ProtoRight = Help.RectOf(Proto).Max.X / UIScale;
	const float ProtoLeft = ProtoRight - 200.f;
	auto Measure = [&](const FString& K, const FString& Text, float& HintW)
	{
		ShopSetText(Key, K);
		ShopSetText(Label, Text);
		HintW = FMath::Max(32.f, Help.TextWidth(Canvas, *Key, UIScale) + 16.f);
		return HintW + 12.f + Help.TextWidth(Canvas, *Label, UIScale);
	};
	auto DrawAction = [&](const FString& K, const FString& Text, float Left)
	{
		float HintW = 32.f;
		Measure(K, Text, HintW);
		Hint->Offsets.Z = HintW;
		Label->Offsets.X = HintW + 12.f;
		Label->Offsets.Z = HintW + 12.f + 300.f;
		Help.DrawSubtree(Canvas, TEXT("helpbar"), Proto, FVector2f(Left - ProtoLeft, 0.f));
	};
	DrawAction(TEXT("ENTER"), NWText::Get(TEXT("ui_chat")), 40.f);
	float HintW = 32.f;
	const float W = Measure(TEXT("ESC"), NWText::Get(TEXT("ui_back")), HintW);
	DrawAction(TEXT("ESC"), NWText::Get(TEXT("ui_back")), ProtoRight - 40.f - W);
}

void FNWShopMenu::Draw(UCanvas* Canvas, const FVector2D& Mouse)
{
	ANWCharacter* Me = Character.Get();
	if (!Me || !Canvas || !Shop())
	{
		Close();
		return;
	}
	const bool bDetail = Shop()->Items.IsValidIndex(Detail);
	ShopShow(E(TEXT("MasterContainer/ShopListingsPanel")), !bDetail);
	ShopShow(E(TEXT("MasterContainer/ShopItemDetailPanel")), bDetail);
	if (FNWUIElement* Proto = E(TEXT("ShopItemListContainer/ShopGridItem")))
	{
		Proto->Visible = false;   // template item: drawn per cell in DrawGrid
	}
	// No shop rank in the port: the rank panel disappears and the grid takes the whole window.
	ShopShow(E(TEXT("ShopListingsPanelContent/RankPanelContainer")), false);
	if (FNWUIElement* List = E(TEXT("ShopListingsPanelContent/ShopItemListContainer")))
	{
		List->Offsets.X = 0.f;
	}
	if (bDetail)
	{
		FillDetail();
	}
	Ui.Draw(Canvas);
	if (!bDetail)
	{
		DrawGrid(Canvas, Mouse);
	}
	ShopSetText(Head.FindOne(TEXT("screen_header"), TEXT("HeaderHolder/HeaderText")), Title);
	ShopSetText(Head.FindOne(TEXT("screen_header"), TEXT("CurrencyDisplay/CurrencyAmount")),
		UNWInventoryComponent::FormatCoins(Me->Inventory ? Me->Inventory->Coins : 0));
	Head.Draw(Canvas);
	DrawHelp(Canvas);
}

void FNWShopMenu::SliderTo(float MouseX)
{
	const int32 Max = MaxCount(Detail);
	if (Max <= 1)
	{
		return;
	}
	const FBox2f Track = Ui.RectOf(E(TEXT("Slider/SliderTrack")));
	const float K = FMath::Clamp((MouseX - Track.Min.X) / FMath::Max(1.f, Track.GetSize().X), 0.f, 1.f);
	Count = 1 + FMath::RoundToInt(K * (Max - 1));
}

void FNWShopMenu::Hold(const FVector2D& Mouse, bool bDown)
{
	if (!bDown)
	{
		bDragging = false;
		return;
	}
	if (bDragging)
	{
		SliderTo(float(Mouse.X));
	}
}

bool FNWShopMenu::Click(const FVector2D& Mouse)
{
	if (!IsOpen())
	{
		return false;
	}
	ANWCharacter* Me = Character.Get();
	const FVector2f M(Mouse);
	const FNWShop* S = Shop();
	if (Head.RectOf(Head.FindOne(TEXT("screen_header"), TEXT("HeaderHolder/BackButton"))).ExpandBy(10.f).IsInside(M))
	{
		if (ANWPlayerController* PC = Me ? Cast<ANWPlayerController>(Me->GetController()) : nullptr)
		{
			PC->CloseMenus();
		}
		return true;
	}
	if (!S->Items.IsValidIndex(Detail))
	{
		for (const TPair<FBox2f, int32>& C : CellRects)
		{
			if (C.Key.IsInside(M))
			{
				Detail = C.Value;
				Count = 1;
				return true;
			}
		}
		return true;
	}
	if (Ui.RectOf(E(TEXT("InfoPanel/ButtonClose"))).IsInside(M))
	{
		Detail = INDEX_NONE;   // back to the grid
		return true;
	}
	if (Ui.RectOf(E(TEXT("SliderWithTextInput/Slider"))).ExpandBy(6.f).IsInside(M) && MaxCount(Detail) > 1)
	{
		bDragging = true;
		SliderTo(M.X);
		return true;
	}
	if (Ui.RectOf(E(TEXT("InfoPanel/ConfirmButton"))).IsInside(M) && MaxCount(Detail) > 0 && Me && Me->Interactor)
	{
		Me->Interactor->ServerShopBuy(ShopId, S->Items[Detail].Id, Count);
		Count = 1;
	}
	return true;
}

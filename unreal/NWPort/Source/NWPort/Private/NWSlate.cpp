#include "NWSlate.h"

#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "NWInventoryComponent.h"
#include "NWUILayout.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

namespace NWSlate
{
	const FLinearColor Tan(FColor(0xEB, 0xE3, 0xD6)), Gray(FColor(0xB2, 0xAE, 0xA1)), Dim(FColor(0x7A, 0x75, 0x6B)),
		Gold(FColor(0xED, 0xC6, 0x3E)), Line(FColor(0xFF, 0xF1, 0xCE)), Red(FColor(0xF2, 0x5D, 0x5D)),
		Wood(FColor(0x2C, 0x25, 0x1B)), Bronze(FColor(0x6B, 0x4E, 0x1E));

	FLinearColor WithA(FLinearColor C, float A)
	{
		C.A *= A;
		return C;
	}

	FSlateFontInfo Font(int32 Style, int32 Size)
	{
		return FSlateFontInfo(FNWUILayout::GameFont(Style), Size);
	}

	FSlateRoundedBoxBrush Round(const FLinearColor& Fill, const FLinearColor& Outline, float W, float R)
	{
		return FSlateRoundedBoxBrush(Fill, R, Outline, W);
	}

	FStyle::FStyle()
	{
		auto Btn = [](FButtonStyle& B, const FLinearColor& Base, float Outline)
		{
			B = FButtonStyle()
				.SetNormal(Round(Base, WithA(Gold, Outline), 1.f))
				.SetHovered(Round(Base * 1.35f, WithA(Gold, FMath::Min(1.f, Outline + 0.4f)), 1.f))
				.SetPressed(Round(Base * 0.8f, Gold, 1.f))
				.SetDisabled(Round(WithA(Base, 0.4f), WithA(Gold, 0.12f), 1.f))
				.SetNormalPadding(FMargin(14.f, 7.f))
				.SetPressedPadding(FMargin(14.f, 8.f, 14.f, 6.f));
		};
		Btn(Button, Wood, 0.35f);
		Btn(Primary, Bronze, 0.6f);
		Btn(Chip, FLinearColor(1.f, 1.f, 1.f, 0.04f), 0.f);
		Chip.SetNormalPadding(FMargin(10.f, 5.f)).SetPressedPadding(FMargin(10.f, 6.f, 10.f, 4.f));
		Flat = FButtonStyle().SetNormal(None).SetHovered(Round(FLinearColor(1.f, 1.f, 1.f, 0.05f))).SetPressed(Round(FLinearColor(1.f, 1.f, 1.f, 0.08f)))
			.SetDisabled(None).SetNormalPadding(FMargin(10.f, 6.f)).SetPressedPadding(FMargin(10.f, 7.f, 10.f, 5.f));

		Row = FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
		const FSlateRoundedBoxBrush Sel = Round(WithA(Gold, 0.13f), WithA(Gold, 0.55f), 1.f, 3.f);
		Row.SetEvenRowBackgroundBrush(Round(FLinearColor(1.f, 1.f, 1.f, 0.03f), FLinearColor::Transparent, 0.f, 3.f))
			.SetOddRowBackgroundBrush(Round(FLinearColor(1.f, 1.f, 1.f, 0.015f), FLinearColor::Transparent, 0.f, 3.f))
			.SetEvenRowBackgroundHoveredBrush(Round(FLinearColor(1.f, 1.f, 1.f, 0.07f), FLinearColor::Transparent, 0.f, 3.f))
			.SetOddRowBackgroundHoveredBrush(Round(FLinearColor(1.f, 1.f, 1.f, 0.07f), FLinearColor::Transparent, 0.f, 3.f))
			.SetActiveBrush(Sel).SetActiveHoveredBrush(Sel).SetInactiveBrush(Sel).SetInactiveHoveredBrush(Sel)
			.SetActiveHighlightedBrush(Sel).SetInactiveHighlightedBrush(Sel)
			.SetSelectorFocusedBrush(None)
			.SetTextColor(Tan).SetSelectedTextColor(Tan);

		FTableColumnHeaderStyle Col = FCoreStyle::Get().GetWidgetStyle<FTableColumnHeaderStyle>("TableView.Header.Column");
		Col.SetNormalBrush(None).SetHoveredBrush(Round(FLinearColor(1.f, 1.f, 1.f, 0.04f)));
		Header = FCoreStyle::Get().GetWidgetStyle<FHeaderRowStyle>("TableView.Header");
		Header.SetBackgroundBrush(None).SetColumnStyle(Col).SetLastColumnStyle(Col).SetForegroundColor(Dim)
			.SetHorizontalSeparatorBrush(Divider).SetHorizontalSeparatorThickness(1.f);

		List = FTableViewStyle().SetBackgroundBrush(None);

		Scroll = FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar");
		Scroll.SetNormalThumbImage(Round(WithA(Gold, 0.30f), FLinearColor::Transparent, 0.f, 3.f))
			.SetHoveredThumbImage(Round(WithA(Gold, 0.55f), FLinearColor::Transparent, 0.f, 3.f))
			.SetDraggedThumbImage(Round(WithA(Gold, 0.75f), FLinearColor::Transparent, 0.f, 3.f))
			.SetVerticalBackgroundImage(Round(FLinearColor(1.f, 1.f, 1.f, 0.03f), FLinearColor::Transparent, 0.f, 3.f))
			.SetVerticalTopSlotImage(None).SetVerticalBottomSlotImage(None)
			.SetThickness(6.f);

		ScrollBox = FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>("ScrollBox");
		ScrollBox.SetTopShadowBrush(None).SetBottomShadowBrush(None).SetLeftShadowBrush(None).SetRightShadowBrush(None);

		Field = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
		Field.SetBackgroundImageNormal(Round(FLinearColor(1.f, 1.f, 1.f, 0.05f), WithA(Gold, 0.25f), 1.f))
			.SetBackgroundImageHovered(Round(FLinearColor(1.f, 1.f, 1.f, 0.07f), WithA(Gold, 0.5f), 1.f))
			.SetBackgroundImageFocused(Round(FLinearColor(1.f, 1.f, 1.f, 0.08f), WithA(Gold, 0.9f), 1.f))
			.SetBackgroundImageReadOnly(Round(FLinearColor(1.f, 1.f, 1.f, 0.03f)))
			.SetForegroundColor(Tan).SetFocusedForegroundColor(FLinearColor::White)
			.SetPadding(FMargin(10.f, 6.f));
	}

	const FStyle& St()
	{
		static FStyle S;
		return S;
	}

	const FSlateBrush* Tex(const FString& Path, float Size)
	{
		static TMap<FString, TSharedPtr<FSlateBrush>> Cache;
		const FString Key = FString::Printf(TEXT("%s@%.0f"), *Path, Size);
		if (const TSharedPtr<FSlateBrush>* Found = Cache.Find(Key))
		{
			return Found->Get();
		}
		UTexture2D* T = Path.IsEmpty() ? nullptr : FNWUILayout::GameTexture(Path);
		TSharedPtr<FSlateBrush> B;
		if (T)
		{
			B = MakeShared<FSlateBrush>();
			B->SetResourceObject(T);
			B->ImageSize = FVector2D(Size, Size);
			B->DrawAs = ESlateBrushDrawType::Image;
		}
		Cache.Add(Key, B);
		return B.Get();
	}

	TSharedRef<STextBlock> Txt(const FText& T, int32 Style, int32 Size, const FLinearColor& C)
	{
		return SNew(STextBlock).Text(T).Font(Font(Style, Size)).ColorAndOpacity(C);
	}

	TSharedRef<STextBlock> Txt(const FString& S, int32 Style, int32 Size, const FLinearColor& C)
	{
		return Txt(FText::FromString(S), Style, Size, C);
	}

	TSharedRef<SWidget> Coins(TAttribute<int64> Cents, int32 Style, int32 Size, TAttribute<FSlateColor> Color)
	{
		const float Icon = FMath::RoundToFloat(Size * 1.5f);
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 5.f, 0.f)
			[
				SNew(SBox).WidthOverride(Icon).HeightOverride(Icon)
				[
					SNew(SImage).Image(Tex(TEXT("lyshineui/images/icons/objectives/reward_coin.dds"), Icon))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Font(Style, Size)).ColorAndOpacity(Color)
				.Text_Lambda([Cents]() { return FText::FromString(UNWInventoryComponent::FormatCoins(Cents.Get())); })
			];
	}

	TSharedRef<SWidget> CloseButton(TFunction<void()> OnClose)
	{
		return SNew(SBox).WidthOverride(40.f).HeightOverride(40.f)
			[
				SNew(SButton)
				.ButtonStyle(&St().Flat)
				.ContentPadding(FMargin(0.f))
				.HAlign(HAlign_Center).VAlign(VAlign_Center)
				.OnClicked_Lambda([OnClose]() { OnClose(); return FReply::Handled(); })
				[
					SNew(SBox).WidthOverride(22.f).HeightOverride(22.f)
					[
						SNew(SImage).Image(Tex(TEXT("lyshineui/images/icons/misc/icon_close.dds"), 22.f)).ColorAndOpacity(Tan)
					]
				]
			];
	}
}

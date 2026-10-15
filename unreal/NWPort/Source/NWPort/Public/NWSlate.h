#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"

/**
 * Style of the port's own screens (those that do not copy the game; maintainer 2026-10-14: new screens follow the
 * Trading Post style). The game's colors and fonts, buttons, table, scrolling and fields in Slate; coin icon and close button.
 */
namespace NWSlate
{
	NWPORT_API extern const FLinearColor Tan, Gray, Dim, Gold, Line, Red, Wood, Bronze;

	NWPORT_API FLinearColor WithA(FLinearColor C, float A);
	/** The game's font (FNWUILayout::GameFont: 0 regular, 1 semibold, 2 bold, 3 Pica, 6 italic). */
	NWPORT_API FSlateFontInfo Font(int32 Style, int32 Size);
	NWPORT_API FSlateRoundedBoxBrush Round(const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float W = 0.f, float R = 4.f);

	struct NWPORT_API FStyle
	{
		FSlateRoundedBoxBrush Panel = Round(FLinearColor(0.f, 0.f, 0.f, 0.42f), WithA(Line, 0.10f), 1.f, 6.f);
		FSlateRoundedBoxBrush Tip = Round(FLinearColor(0.035f, 0.028f, 0.02f, 0.97f), WithA(Gold, 0.35f), 1.f, 6.f);
		FSlateRoundedBoxBrush Divider = Round(WithA(Line, 0.18f), FLinearColor::Transparent, 0.f, 0.f);
		FSlateColorBrush White = FSlateColorBrush(FLinearColor::White);
		FSlateColorBrush Backdrop = FSlateColorBrush(FLinearColor(0.02f, 0.015f, 0.01f, 0.93f));
		FSlateNoResource None;
		FButtonStyle Button, Primary, Chip, Flat;
		FTableRowStyle Row;
		FHeaderRowStyle Header;
		FTableViewStyle List;
		FScrollBarStyle Scroll;
		FEditableTextBoxStyle Field;
		FScrollBoxStyle ScrollBox;

		FStyle();
	};
	NWPORT_API const FStyle& St();

	/** The game's texture brush (lyshineui/...dds); null if the texture was not imported. */
	NWPORT_API const FSlateBrush* Tex(const FString& Path, float Size);
	NWPORT_API TSharedRef<STextBlock> Txt(const FText& T, int32 Style, int32 Size, const FLinearColor& C);
	NWPORT_API TSharedRef<STextBlock> Txt(const FString& S, int32 Style, int32 Size, const FLinearColor& C);
	/** Coins with the game's icon on the left (cents; "12.345,00"). */
	NWPORT_API TSharedRef<SWidget> Coins(TAttribute<int64> Cents, int32 Style, int32 Size, TAttribute<FSlateColor> Color);
	/** City balance sheet: the game's large gold coin, amber color (different from the player's coins). */
	NWPORT_API extern const FLinearColor Amber;
	NWPORT_API TSharedRef<SWidget> Treasury(TAttribute<int64> Cents, int32 Style, int32 Size);
	/** Close button (X) in the screen corner. */
	NWPORT_API TSharedRef<SWidget> CloseButton(TFunction<void()> OnClose);
}

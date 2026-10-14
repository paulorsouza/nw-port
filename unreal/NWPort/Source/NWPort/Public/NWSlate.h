#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"

/**
 * Estilo das telas próprias do port (as que não copiam o jogo; dono 2026-10-14: telas novas seguem o estilo do
 * entreposto). Cores e fontes do jogo, botões, tabela, rolagem e campos em Slate; ícone de moeda e botão de sair.
 */
namespace NWSlate
{
	NWPORT_API extern const FLinearColor Tan, Gray, Dim, Gold, Line, Red, Wood, Bronze;

	NWPORT_API FLinearColor WithA(FLinearColor C, float A);
	/** Fonte do jogo (FNWUILayout::GameFont: 0 regular, 1 semibold, 2 bold, 3 Pica, 6 itálico). */
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

	/** Pincel de textura do jogo (lyshineui/...dds); nulo se a textura não foi importada. */
	NWPORT_API const FSlateBrush* Tex(const FString& Path, float Size);
	NWPORT_API TSharedRef<STextBlock> Txt(const FText& T, int32 Style, int32 Size, const FLinearColor& C);
	NWPORT_API TSharedRef<STextBlock> Txt(const FString& S, int32 Style, int32 Size, const FLinearColor& C);
	/** Moedas com o ícone do jogo à esquerda (centavos; "12.345,00"). */
	NWPORT_API TSharedRef<SWidget> Coins(TAttribute<int64> Cents, int32 Style, int32 Size, TAttribute<FSlateColor> Color);
	/** Botão de sair (X) do canto da tela. */
	NWPORT_API TSharedRef<SWidget> CloseButton(TFunction<void()> OnClose);
}

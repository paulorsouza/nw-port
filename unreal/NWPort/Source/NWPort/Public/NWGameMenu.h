#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class UCanvas;

/**
 * Menu do Esc (`lyshineui/escapemenu`) e configurações (`lyshineui/options`), com os textos do jogo
 * (docs/hud-original.md). Desenhado pela HUD; cliques pelo controlador. As opções ficam em FNWOptions.
 */
class NWPORT_API FNWGameMenu
{
public:
	enum class EAction : uint8 { None, Resume, ExitToMenu, ExitToDesktop };

	bool IsOpen() const { return bOpen; }
	void Open() { bOpen = true; Page = 0; }
	void Close() { bOpen = false; }
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	/** Clique: muda a opção ou devolve a ação pedida (o controlador executa). */
	EAction Click(const FVector2D& Mouse);
	/** Aba Atalhos: esperando a próxima tecla para a ação escolhida (o controlador entrega em AssignKey). */
	bool IsWaitingKey() const { return bOpen && !WaitingKey.IsNone(); }
	void AssignKey(const FKey& Key);
	void CancelKey() { WaitingKey = NAME_None; }
	/** Teclas mudaram desde a última consulta (o controlador refaz o mapeamento do personagem). */
	bool ConsumeKeysChanged() { const bool b = bKeysChanged; bKeysChanged = false; return b; }

private:
	struct FButton
	{
		FBox2f Rect = FBox2f(ForceInit);
		int32 Id = 0;
	};
	void DrawText(UCanvas* Canvas, const FString& Text, float X, float Y, float Size, const FLinearColor& Color, int32 Style = 0) const;
	float TextWidth(const FString& Text, float Size, int32 Style = 0) const;
	void DrawPanel(UCanvas* Canvas, const FBox2f& R) const;
	bool Button(UCanvas* Canvas, const FBox2f& R, const FString& Label, int32 Id, const FVector2D& Mouse, bool bSelected = false);
	void DrawMain(UCanvas* Canvas, const FVector2D& Mouse);
	void DrawOptions(UCanvas* Canvas, const FVector2D& Mouse);
	/** Uma linha de opção: rótulo à esquerda, valor com setas (escolha) ou barra (0..1). */
	void OptionRow(UCanvas* Canvas, float Y, const FString& Label, const FString& Value, float Bar, int32 Id, const FVector2D& Mouse);
	void ChangeOption(int32 Id, int32 Dir, float BarValue);
	void DrawKeys(UCanvas* Canvas, const FVector2D& Mouse);

	bool bOpen = false;
	int32 Page = 0;       // 0 menu, 1 configurações
	int32 Tab = 0;        // 0 vídeo, 1 áudio, 2 jogo, 3 atalhos
	FName WaitingKey;
	bool bKeysChanged = false;
	float UIScale = 1.f;
	TArray<FButton> Buttons;
	FBox2f Panel = FBox2f(ForceInit);
};

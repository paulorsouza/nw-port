#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class UCanvas;

/**
 * Esc menu (`lyshineui/escapemenu`) and settings (`lyshineui/options`), with the game texts
 * (docs/game/ui/original.md). Drawn by the HUD; clicks through the controller. The options live in FNWOptions.
 */
class NWPORT_API FNWGameMenu
{
public:
	enum class EAction : uint8 { None, Resume, Unstuck, ExitToMenu, ExitToDesktop };

	bool IsOpen() const { return bOpen; }
	void Open() { bOpen = true; Page = 0; }
	void Close() { bOpen = false; }
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	/** Click: changes the option or returns the requested action (the controller runs it). */
	EAction Click(const FVector2D& Mouse);
	/** Bindings tab: waiting for the next key for the chosen action (the controller delivers it in AssignKey). */
	bool IsWaitingKey() const { return bOpen && !WaitingKey.IsNone(); }
	void AssignKey(const FKey& Key);
	void CancelKey() { WaitingKey = NAME_None; }
	/** Keys changed since the last query (the controller redoes the character mapping). */
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
	/** One option row: label on the left, value with arrows (choice) or bar (0..1). */
	void OptionRow(UCanvas* Canvas, float Y, const FString& Label, const FString& Value, float Bar, int32 Id, const FVector2D& Mouse);
	void ChangeOption(int32 Id, int32 Dir, float BarValue);
	void DrawKeys(UCanvas* Canvas, const FVector2D& Mouse);

	bool bOpen = false;
	int32 Page = 0;       // 0 menu, 1 settings
	int32 Tab = 0;        // 0 video, 1 audio, 2 game, 3 bindings
	FName WaitingKey;
	bool bKeysChanged = false;
	float UIScale = 1.f;
	TArray<FButton> Buttons;
	FBox2f Panel = FBox2f(ForceInit);
};

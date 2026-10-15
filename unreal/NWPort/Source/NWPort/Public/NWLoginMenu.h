#pragma once

#include "CoreMinimal.h"
#include "NWUILayout.h"

class APlayerController;
class SNWLoginFields;
class SWidget;
class UCanvas;
class UNWBackend;

/**
 * The clan's login screen, built from game pieces (Data/UI/login_layout.json, see tools/ui_build_login.py):
 * the character panel from landingscreenv2 (3 slots and Play) and its popup as the account, character creation and
 * character deletion window. The text fields are the game's (NameInputField) with an invisible SEditableText on
 * top for typing.
 */
class NWPORT_API FNWLoginMenu
{
public:
	bool IsActive() const { return bActive; }
	void Activate(APlayerController* PC, UNWBackend* Backend);
	void Deactivate();
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	void Click(const FVector2D& Mouse);
	// Reloads the game pieces in the current language (NWText::Lang).
	void ReloadLanguage();

	// Fields of the open window (used by the text overlay).
	int32 NumFields() const { return Fields.Num(); }
	FBox2f FieldRect(int32 Index) const { return Fields.IsValidIndex(Index) ? Fields[Index].Rect : FBox2f(ForceInit); }
	bool IsPassword(int32 Index) const { return Fields.IsValidIndex(Index) && Fields[Index].bPassword; }
	const FString& FieldValue(int32 Index) const { return Fields[Index].Value; }
	void SetFieldValue(int32 Index, const FString& Value);
	float FontPixels() const { return 28.f * UIScale; }
	void Submit();

	/**
	 * Unattended test: `-nwuisteps="fill:0:teste1|fill:1:$NW_PASSWORD|click:accept|click:slot2"` runs one step
	 * every 1.5 s; the click goes through the same mouse Click, at the center of the element.
	 */
	void RunTestSteps();

private:
	enum class EScreen : uint8 { Login, Register, Characters, NewCharacter, ConfirmDelete };
	struct FField
	{
		FString Placeholder;
		FString Value;
		bool bPassword = false;
		FBox2f Rect = FBox2f(ForceInit);   // text pixels, in the last draw
	};

	bool Load();
	void Show(EScreen Screen);
	void Cancel();
	void Apply(const FVector2D& Mouse);
	void DrawFields(UCanvas* Canvas);
	void DrawLabel(UCanvas* Canvas, const FString& Text, const FBox2f& Rect, const FLinearColor& Color, float Size, bool bCenter) const;
	void Done(bool bOk, const FString& Error);
	FString Str(const TCHAR* Key) const;
	FNWUIElement* E(const FString& Suffix) { return Layout.FindOne(TEXT("landing"), Suffix); }
	FNWUIElement* Slot(int32 Index, const FString& Suffix);
	bool Hit(FNWUIElement* Element, const FVector2D& Mouse) const;
	void SetButton(const FString& Button, const FString& Text, bool bHover, bool bEnabled);
	bool HasPopup() const { return Screen != EScreen::Characters; }

	FNWUILayout Layout;
	TWeakObjectPtr<APlayerController> Owner;
	TWeakObjectPtr<UNWBackend> Backend;
	TSharedPtr<SNWLoginFields> Overlay;
	TSharedPtr<SWidget> OverlayHost;
	EScreen Screen = EScreen::Login;
	TArray<FField> Fields;
	FString Message;            // error/notice in the open window or above Play
	int32 Selected = -1;        // chosen slot
	int32 Hover = -1;
	bool bActive = false;
	bool bLoaded = false;
	bool bConnecting = false;
	float UIScale = 1.f;
	FBox2f LogoutRect = FBox2f(ForceInit);
	FBox2f LangRects[2] = {FBox2f(ForceInit), FBox2f(ForceInit)};   // PT, EN
	void DrawLanguagePicker(UCanvas* Canvas, const FVector2D& Mouse);
	// Backend responses that arrive after the screen is gone (map change) check this.
	TSharedRef<int32> AliveToken = MakeShared<int32>(0);
	TArray<FString> TestSteps;
	double NextTestStep = 0.0;
	bool bTestParsed = false;
};

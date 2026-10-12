#pragma once

#include "CoreMinimal.h"
#include "NWUILayout.h"

class APlayerController;
class SNWLoginFields;
class SWidget;
class UCanvas;
class UNWBackend;

/**
 * Tela de entrada do clã, montada com peças do jogo (Data/UI/login_layout.json, ver tools/ui_build_login.py):
 * painel de personagens da landingscreenv2 (3 vagas e Jogar) e o popup dela como janela de conta, criação e
 * exclusão de personagem. Os campos de texto são do jogo (NameInputField) com um SEditableText invisível por
 * cima para digitar.
 */
class NWPORT_API FNWLoginMenu
{
public:
	bool IsActive() const { return bActive; }
	void Activate(APlayerController* PC, UNWBackend* Backend);
	void Deactivate();
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	void Click(const FVector2D& Mouse);
	// Recarrega as peças do jogo no idioma atual (NWText::Lang).
	void ReloadLanguage();

	// Campos da janela aberta (usados pela sobreposição de texto).
	int32 NumFields() const { return Fields.Num(); }
	FBox2f FieldRect(int32 Index) const { return Fields.IsValidIndex(Index) ? Fields[Index].Rect : FBox2f(ForceInit); }
	bool IsPassword(int32 Index) const { return Fields.IsValidIndex(Index) && Fields[Index].bPassword; }
	const FString& FieldValue(int32 Index) const { return Fields[Index].Value; }
	void SetFieldValue(int32 Index, const FString& Value);
	float FontPixels() const { return 28.f * UIScale; }
	void Submit();

	/**
	 * Teste sem pessoa: `-nwuisteps="fill:0:teste1|fill:1:$NW_PASSWORD|click:accept|click:slot2"` roda um passo a
	 * cada 1,5 s; o clique vai pelo mesmo Click do mouse, no centro do elemento.
	 */
	void RunTestSteps();

private:
	enum class EScreen : uint8 { Login, Register, Characters, NewCharacter, ConfirmDelete };
	struct FField
	{
		FString Placeholder;
		FString Value;
		bool bPassword = false;
		FBox2f Rect = FBox2f(ForceInit);   // pixels do texto, no último desenho
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
	FString Message;            // erro/aviso na janela aberta ou acima do Jogar
	int32 Selected = -1;        // vaga escolhida
	int32 Hover = -1;
	bool bActive = false;
	bool bLoaded = false;
	bool bConnecting = false;
	float UIScale = 1.f;
	FBox2f LogoutRect = FBox2f(ForceInit);
	FBox2f LangRects[2] = {FBox2f(ForceInit), FBox2f(ForceInit)};   // PT, EN
	void DrawLanguagePicker(UCanvas* Canvas, const FVector2D& Mouse);
	// Respostas do backend chegando depois de a tela sumir (troca de mapa) checam isto.
	TSharedRef<int32> AliveToken = MakeShared<int32>(0);
	TArray<FString> TestSteps;
	double NextTestStep = 0.0;
	bool bTestParsed = false;
};

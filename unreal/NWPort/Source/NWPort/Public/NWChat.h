#pragma once

#include "CoreMinimal.h"
#include "NWPlayerController.h"

class SEditableText;
class SWidget;
class UCanvas;

/**
 * Chat no canto inferior esquerdo, no visual do chat do jogo (lyshineui/hud/chat: fundo 0,04 a 80%, moldura de
 * 1 px cinza, divisória). Enter abre o campo, Enter envia, Esc cancela. Fechado, mostra as últimas mensagens por
 * 15 s; aberto, as últimas 30.
 */
class NWPORT_API FNWChat
{
public:
	void Add(ENWChatChannel Channel, const FString& Sender, const FString& Text);
	void Draw(UCanvas* Canvas);
	void Open(ANWPlayerController* PC);
	void Close();
	bool IsOpen() const { return bOpen; }

	// Usados pelo campo de texto (Slate).
	FBox2f InputRect() const { return InputPixels; }
	float FontPixels() const { return 20.f * UIScale; }
	void Submit(const FString& Text);

private:
	struct FLine
	{
		ENWChatChannel Channel;
		FString Sender;
		FString Text;
		double Time;
	};

	TArray<FLine> Lines;
	TWeakObjectPtr<ANWPlayerController> Owner;
	TSharedPtr<SEditableText> Edit;
	TSharedPtr<SWidget> Host;
	FBox2f InputPixels = FBox2f(ForceInit);
	float UIScale = 1.f;
	bool bOpen = false;
};

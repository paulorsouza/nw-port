#pragma once

#include "CoreMinimal.h"
#include "NWPlayerController.h"

class SEditableText;
class SWidget;
class UCanvas;

/**
 * Chat in the lower-left corner, in the look of the game chat (lyshineui/hud/chat: background 0.04 at 80%, 1 px gray
 * frame, divider). Enter opens the field, Enter sends, Esc cancels. Closed, shows the last messages for
 * 15 s; open, the last 30.
 */
class NWPORT_API FNWChat
{
public:
	void Add(ENWChatChannel Channel, const FString& Sender, const FString& Text);
	void Draw(UCanvas* Canvas);
	void Open(ANWPlayerController* PC);
	void Close();
	bool IsOpen() const { return bOpen; }

	// Used by the text field (Slate).
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

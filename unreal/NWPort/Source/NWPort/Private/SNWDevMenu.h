#pragma once

#include "CoreMinimal.h"
#include "NWPlayerController.h"
#include "Widgets/SCompoundWidget.h"

// Dev menu (F10): test options run on the server and local debugging.
class SNWDevMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNWDevMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ANWPlayerController>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Key) override;

private:
	TSharedRef<SWidget> Button(const FString& Label, TFunction<FText()> State, TFunction<void()> Action);
	TSharedRef<SWidget> Header(const FString& Label);
	TWeakObjectPtr<ANWPlayerController> Owner;
};

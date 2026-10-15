#pragma once

#include "CoreMinimal.h"
#include "NWUILayout.h"

class ANWCharacter;
class UCanvas;

/**
 * Attributes tab of the game's "Character" window (Attributes from lyshineui/skills/skills.dynamicuicanvas,
 * tools/ui_build_attributes.py): rune with the available points, one row per attribute with value, pending
 * ("+N"), − and + buttons, bar up to 300 with marks every 50, Confirm and the XP bar with the level. Points stay
 * pending until Confirm (the server checks in UNWProgressionComponent::ServerSpendPoints).
 */
class NWPORT_API FNWAttributesMenu
{
public:
	bool IsOpen() const { return Character.IsValid(); }
	void Open(ANWCharacter* InCharacter);
	void Close() { Character.Reset(); Pending.Init(0, 5); }
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	void Click(const FVector2D& Mouse);

private:
	bool Load();
	FString Row(int32 i) const;
	int32 PendingTotal() const;
	FNWUILayout Layout;
	bool bLoaded = false;
	TWeakObjectPtr<ANWCharacter> Character;
	TArray<int32> Pending;
};

#pragma once

#include "CoreMinimal.h"
#include "NWUILayout.h"

class ANWCharacter;
class UCanvas;

/**
 * Gathering trade skills tab of the game's "Personagem" window (TradeSkills > GatheringSection of
 * lyshineui/skills/skills.dynamicuicanvas, tools/ui_build_tradeskills.py): one cell per trade skill with the level
 * in the middle of the circle and progress to the next level on the ring. Opens via the tab at the top of window K.
 */
class NWPORT_API FNWTradeskillMenu
{
public:
	bool IsOpen() const { return Character.IsValid(); }
	void Open(ANWCharacter* InCharacter);
	void Close() { Character.Reset(); }
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);

	/**
	 * Tabs of the K window (Maestria em Armas / Hab. Mercantes / Atributos), drawn over the open screen; Active = current
	 * tab. Returns the tab under the mouse on click (or INDEX_NONE) through TabAt.
	 */
	static void DrawTabs(UCanvas* Canvas, const FVector2D& Mouse, int32 Active);
	static int32 TabAt(const FVector2D& Mouse);

private:
	bool Load();
	FNWUILayout Layout;
	bool bLoaded = false;
	TWeakObjectPtr<ANWCharacter> Character;
	static FBox2f TabRects[3];
};

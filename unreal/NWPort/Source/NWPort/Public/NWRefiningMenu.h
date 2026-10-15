#pragma once

#include "CoreMinimal.h"
#include "NWUILayout.h"

class ANWCharacter;
class ANWStation;
class UCanvas;
struct FNWRecipe;
struct FNWItem;

/**
 * Refining screen of a station (area 4) with the game's layout: lyshineui/crafting/craftingscreenv4 in refining mode
 * (tools/ui_screens/refining.json; what turns on, off and gets filled: docs/game/ui/refining.md), the title bar from
 * stationproperties (screen_header.json) and the shortcut footer (helpbar.json).
 * States: recipe selection; roll (product count, 1 s, craftroll.lua); end (a click continues).
 */
class NWPORT_API FNWRefiningMenu
{
public:
	bool IsOpen() const { return Station.IsValid() && Character.IsValid(); }
	void Open(ANWCharacter* InCharacter, ANWStation* InStation);
	void Close() { Sound(TEXT("ui_crafting_outro")); Station.Reset(); Character.Reset(); }
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	/** true if the click belonged to the screen (when open, every click is its own). */
	bool Click(const FVector2D& Mouse);
	void Wheel(float Delta);
	/** Every frame with the left button down: drags the quantity marker and repeats − and + while held. */
	void Hold(const FVector2D& Mouse, bool bDown);
	/** Space (the game's tryskip action): max quantity on selection, skips the count, returns to the station at the end. */
	void Space();
	/** Click on CONFECCIONAR (CRAFT) (also used by the offline capture). */
	void ClickCraft();
	/** Chooses the recipe and the quantity (offline capture). */
	void Select(FName RecipeId, int32 InCount);
	/** Opens the "Improve items" tab with material Mat chosen for the item in bag slot Slot (offline capture). */
	void ShowImprove(int32 Slot, uint8 Mat) { bReroll = true; if (Slot != INDEX_NONE) { ImproveMat.Add(Slot, Mat); } }
	/** Rectangles and state of the elements whose path contains Filter, in the last draw (offline capture). */
	void Dump(const FString& Filter) const;
	/** Fixed clock for the offline capture (NWUIShotCommandlet); 0 = real clock. */
	double FixedNow = 0.0;

private:
	bool Load();
	FNWUILayout Ui, Head, Help;
	bool bLoaded = false;
	TWeakObjectPtr<ANWCharacter> Character;
	TWeakObjectPtr<ANWStation> Station;
	TArray<const FNWRecipe*> Recipes;

	// Tree: 0 learned, 1 family, 2 group, 3 recipe (craftingtreeviewitem.lua).
	struct FRow { int32 Depth = 0; FString Text; FString Key; int32 Recipe = INDEX_NONE; };
	TArray<FRow> Rows;
	TSet<FString> Collapsed;
	void BuildRows();
	TArray<TPair<FBox2f, int32>> RowRects;
	int32 Selected = 0;
	int32 Scroll = 0;
	int32 Count = 1;

	// Held button: bar marker, or − and + with repeat (button.lua OnTick).
	enum class EDrag : uint8 { None, Slider, Minus, Plus };
	EDrag Drag = EDrag::None;
	double HoldLast = 0.0;
	double HoldInitial = 0.0, HoldQuantity = 0.0, HoldPress = 0.0;
	int32 HoldStep = 1;
	void SliderTo(float MouseX);
	void Step(int32 Delta);

	enum class EState : uint8 { Select, Roll, Done };
	EState State = EState::Select;
	double RollStart = 0.0;
	int32 HaveBefore = 0;
	int32 RollBase = 0;
	// Weapon/tool: the count rises to the GS of the new item (the bag slot that did not have this item before).
	bool bGearRoll = false;
	TArray<int32> BeforeSlots;
	// Server result (UNWInteractorComponent::CraftResult): the screen waits for a Seq greater than WaitSeq.
	int32 WaitSeq = 0;
	uint8 RollKind = 255;   // 255 craft; otherwise ENWImprove
	FName RollItem;
	void StartResultRoll(FName ItemId, uint8 Kind);
	const FNWItem* RollResult() const;
	// Gear roll: t0 = item arrival; GS count from GsFrom to the
	// item's GS in 1 s; attribute, perks and gem come in one by one.
	double ResultAt = 0.0;
	int32 GsFrom = 0;
	void DrawGearRoll(const FNWRecipe& R);
	/** End of the gear roll animations (a click before that only finishes the animations). */
	double GearRollLength() const;

	FNWUIElement* E(const TCHAR* Suffix) { return Ui.FindOne(TEXT("refining"), Suffix); }
	void Fill(UCanvas* Canvas, const FNWRecipe& R);
	void DrawRows(UCanvas* Canvas, const FVector2D& Mouse);
	void DrawRoll(const FNWRecipe& R);
	void DrawHelp(UCanvas* Canvas);
	int32 MaxCount() const;
	double Now() const { return FixedNow > 0.0 ? FixedNow : FPlatformTime::Seconds(); }
	bool CanCraft(const FNWRecipe& R) const;

	// Craft a weapon, shield or tool (docs/game/crafting.md): one at a time, no quantity; GS and quality range, or the
	// training notice below the crafting level.
	void DrawCraftInfo(UCanvas* Canvas, const FNWRecipe& R, const FVector2D& Mouse);

	// "Improve items" tab (docs/game/items.md): bag items of this trade skill with reroll attribute, raise GS and unbind.
	bool bReroll = false;
	FBox2f TabRect = FBox2f(ForceInit);
	struct FImproveButton { FBox2f Rect; int32 Slot = INDEX_NONE; uint8 Kind = 0; uint8 Mat = 0; };
	TArray<FImproveButton> RerollButtons;
	// Improvement material chosen per item (bag slot -> index into FNWCrafting::ImproveMaterials) and the
	// material rectangles for the click.
	TMap<int32, uint8> ImproveMat;
	TArray<FImproveButton> MatButtons;
	void DrawReroll(UCanvas* Canvas, const FVector2D& Mouse);
	void DrawTab(UCanvas* Canvas, const FVector2D& Mouse);

	// End of the gear roll (docs/game/crafting.md "Result screen"): Continue, Equip (equippable item in the bag) and Exit
	// Station, in a column in ActiveButtonsContainer.
	enum class EDoneButton : uint8 { Continue, Equip, Exit };
	TArray<TPair<FBox2f, EDoneButton>> DoneButtons;
	void DrawDoneButtons(UCanvas* Canvas, const FVector2D& Mouse);
	// Screen sounds (docs/game/crafting.md "Result screen"; Data/Audio/ui_sounds.json, tools/make_ui_sounds.py): one variation
	// picked from each layer of the event. SoundAt plays once when T passes At (a bit marks what already played).
	void Sound(const TCHAR* Event) const;
	void SoundAt(double T, double At, int32 Bit, const FString& Event);
	uint32 SoundsDone = 0;
	/** Result item in the bag (the same item the server returned), or null. */
	const FNWItem* ResultInBag() const;
};

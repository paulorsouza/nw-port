#pragma once

#include "CoreMinimal.h"
#include "NWUILayout.h"

class ANWCharacter;
class UCanvas;
struct FNWShop;

/**
 * Shop screen (area 4) with the game layout: lyshineui/shop/shop (tools/ui_screens/shop.json), the title bar of
 * stationproperties (screen_header.json) and the footer (helpbar.json). Grid list (shopgriditem.lua) and item
 * detail with quantity and purchase (shopitemdetailpanel.lua). Price and weekly limit: tools/shops.json (NWShop.h).
 */
class NWPORT_API FNWShopMenu
{
public:
	bool IsOpen() const { return Character.IsValid() && !ShopId.IsEmpty(); }
	void Open(ANWCharacter* InCharacter, const FString& InShopId, const FString& InTitle);
	void Close() { Character.Reset(); ShopId.Reset(); }
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	bool Click(const FVector2D& Mouse);
	/** Every frame with the left button: drag the quantity handle. */
	void Hold(const FVector2D& Mouse, bool bDown);
	/** Picks the item (offline capture); NAME_None goes back to the grid. */
	void Select(FName Item, int32 InCount);
	void Dump(const FString& Filter) const;
	/** Fixed clock for the offline capture (NWUIShotCommandlet); 0 = real clock. */
	double FixedNow = 0.0;

private:
	bool Load();
	FNWUILayout Ui, Head, Help;
	bool bLoaded = false;
	TWeakObjectPtr<ANWCharacter> Character;
	FString ShopId;
	FString Title;
	/** Item in the detail view; INDEX_NONE = grid. */
	int32 Detail = INDEX_NONE;
	int32 Count = 1;
	bool bDragging = false;
	TArray<TPair<FBox2f, int32>> CellRects;

	const FNWShop* Shop() const;
	FNWUIElement* E(const TCHAR* Suffix) { return Ui.FindOne(TEXT("shop"), Suffix); }
	/** How much of the item can still be bought: week's limit, coins and weight. */
	int32 MaxCount(int32 Index, int32* OutLimitLeft = nullptr) const;
	void DrawGrid(UCanvas* Canvas, const FVector2D& Mouse);
	void FillDetail();
	void DrawHelp(UCanvas* Canvas);
	void SliderTo(float MouseX);
	double Now() const { return FixedNow > 0.0 ? FixedNow : FPlatformTime::Seconds(); }
	/** "3d 4h" / "23h 59m" until the weekly renewal (CooldownTextUpdater FORMAT_SHORTHAND, no zeros). */
	static FString TimeToReset();
};

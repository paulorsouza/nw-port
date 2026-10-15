#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "NWItems.h"
#include "NWUILayout.h"

class ANWCharacter;
class ANWLootBag;
class SEditableText;
class SWidget;
class UCanvas;

/**
 * Inventory (Tab), equipment and storage in the game style (spec: docs/game/ui/inventory.md).
 * - Inventory window: frame of the inventory/newinventory canvas (Data/UI/inventory_layout.json), list with the game
 *   sections (dynamicitemlist.lua: the item's UiItemClass) in two columns, 60x60 square or round pieces
 *   (itemlayout.lua), sorting by arrival/weight/gear score/tier, current/maximum weight.
 * - Equipment without a panel, at the equipmentv3 positions (63x63 slots, black backgrounds per group, icon tabs, level
 *   and gear score, attributes).
 * - Storage (catcontainer.lua, storage mode): a second identical window on the right; the inventory and equipment move
 *   300 px to the left. Loot bag mode (CONTAINER_MODE_LOOT_BAG): the same window with the items of the ground bag
 *   (ANWLootBag), lifetime notice and count in place of the search; take only, never store.
 * - Drag (itemdraggable.lua): 75x75 copy at the cursor, source at 0.5; compatible slots light up; the target window
 *   lights up with the mouse over it. Shift + click stores/withdraws; right click equips, uses or moves.
 */
class NWPORT_API FNWInventoryMenu
{
public:
	bool IsOpen() const { return Character.IsValid(); }
	void Open(ANWCharacter* InCharacter);
	/** Inventory with the city storage (the server has already opened the location). */
	void OpenStorage(ANWCharacter* InCharacter, const FString& InTitle);
	bool IsStorageMode() const { return IsOpen() && bStorage; }
	/** Inventory with the bag on the ground (catcontainer in loot bag mode). */
	void OpenLootBag(ANWCharacter* InCharacter, ANWLootBag* Bag);
	void Close();
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	/** Button press (the release arrives through Hold). */
	void Click(const FVector2D& Mouse, bool bRight);
	/** Every frame: left button state; drags and drops. */
	void Hold(const FVector2D& Mouse, bool bDown);
	void Wheel(float Delta, const FVector2D& Mouse);
	/** Offline capture: starts a drag from the bag (0), equipment (1) or storage (2). */
	void ShotDrag(int32 Source, int32 Slot);
	/** Offline capture: list sorting (0 inventory, 1 storage): 1 arrival, 2 weight, 3 gear score, 4 tier. */
	void ShotSort(int32 List, int32 Sort) { Lists[FMath::Clamp(List, 0, 1)].Sort = Sort; }
	/** Offline capture: opens the split (1) or destroy (2) popup; Source as in ESource: 0 bag, 1 equipment, 2 storage. */
	void ShotPopup(int32 Kind, int32 Source, int32 Slot, int32 Qty = 0);
	/** Offline capture: list search text (0 inventory, 1 storage). */
	void ShotSearch(int32 List, const FString& Text) { Lists[FMath::Clamp(List, 0, 1)].Search = Text; }
	/** List search field in pixels (the Slate text field sits on top). */
	FBox2f SearchRect(int32 List) const { return Lists[FMath::Clamp(List, 0, 1)].SearchBox; }
	void SetSearch(int32 List, const FString& Text) { Lists[FMath::Clamp(List, 0, 1)].Search = Text; }
	/** Storage tab being renamed (double click): text field rectangle; invalid = none. */
	FBox2f RenameRect() const;
	/** Enter or losing focus in the tab name field. */
	void CommitRename(const FString& Name);
	/** Offline capture: open storage tab. */
	void ShotTab(int32 Tab) { StorageTab = Tab; }

private:
	enum class ESource : uint8 { Bag, Equip, Storage };
	struct FSlotRef
	{
		ENWItemContainer Container = ENWItemContainer::Bag;
		int32 Slot = INDEX_NONE;
		FBox2f Rect = FBox2f(ForceInit);   // click area (clipped by scrolling)
		FBox2f Full = FBox2f(ForceInit);   // whole piece (drawing)
		bool bStorage = false;
		ESource Source() const { return bStorage ? ESource::Storage : Container == ENWItemContainer::Equip ? ESource::Equip : ESource::Bag; }
	};
	// A list of items (inventory or storage) with its own sections, sorting and scrolling.
	struct FSectionHit
	{
		FName Class;
		FBox2f Arrow = FBox2f(ForceInit);
		FBox2f Transfer = FBox2f(ForceInit);
	};
	struct FList
	{
		FBox2f Window = FBox2f(ForceInit);   // whole frame (drop target)
		FBox2f Area = FBox2f(ForceInit);     // scrolling list
		float Scroll = 0.f;                  // px at 1080p
		float ContentH = 0.f;
		int32 Sort = 1;                      // inventoryfilter.lua: 1 arrival, 2 weight, 3 gear score, 4 tier
		int32 SortDir = 1;                   // 1 default (largest/newest first), −1 reversed
		TSet<FName> Collapsed;
		FString Search;                      // search by name (no case or accents)
		FBox2f SearchBox = FBox2f(ForceInit);
		FBox2f SortRects[4];
		TArray<FSectionHit> Sections;
	};

	// Game popup (popup.lua): split stack (Ctrl + click), confirm destroy (drop on the ground), socket gem (drop
	// the gem on the item; ui_attach_gem / ui_replace_gem) and remove gem (Shift + right click on item; docs/game/items.md).
	enum class EPopup : uint8 { None, Split, Destroy, Socket, Unsocket };
	enum class EAction : uint8 { Cancel, Move, Drop, Destroy, Socket, Unsocket };
	struct FPopupButton
	{
		EAction Action = EAction::Cancel;
		FString Label;
		FBox2f Rect = FBox2f(ForceInit);
		bool bEnabled = true;
	};

	bool Load();
	void OpenPopup(EPopup Kind, const FSlotRef& S);
	void DrawPopup(UCanvas* Canvas, const FVector2D& Mouse);
	bool PopupClick(const FVector2D& Mouse);
	void RunAction(EAction Action);
	void SetSliderFromMouse(float X);
	/** Drop-to-ground zone (grounddroptarget): right of the inventory, only without storage. */
	FBox2f GroundZone() const;
	void DrawGround(UCanvas* Canvas, const FVector2D& Mouse);
	void ShowSearch();
	void HideSearch();
	void FillRect(UCanvas* Canvas, const FBox2f& R, const FLinearColor& Color) const;
	void DrawTile(UCanvas* Canvas, UTexture2D* Tex, const FBox2f& R, const FLinearColor& Tint, bool bAdditive = false,
		FVector2f UV0 = FVector2f(0.f, 0.f), FVector2f UV1 = FVector2f(1.f, 1.f)) const;
	void Layout(UCanvas* Canvas);
	void LayoutList(FList& L, bool bStorageItems);
	void LayoutEquipment();
	const FSlotRef* SlotAt(const FVector2D& Mouse) const;
	/** Game piece (itemlayout.lua): background and frame by rarity, icon − 6 px, tier, quantity > 1, sort value. */
	void DrawItemTile(UCanvas* Canvas, const FBox2f& R, const FNWItem* Item, bool bRound, float Alpha, bool bHover, int32 SortShown);
	void DrawWindow(UCanvas* Canvas, const FList& L, float OffsetX, bool bStorageWindow, const FVector2D& Mouse);
	void DrawList(UCanvas* Canvas, const FList& L, bool bStorageItems, const FVector2D& Mouse, const FSlotRef* Hover);
	void DrawEquipment(UCanvas* Canvas, const FVector2D& Mouse, const FSlotRef* Hover);
	void DrawDrag(UCanvas* Canvas, const FVector2D& Mouse);
	/** Draws the tooltip next to the mouse (or with the left edge at ForceX) and returns the rectangle used. */
	FBox2f DrawTooltip(UCanvas* Canvas, const FNWItem& Item, const FVector2D& Mouse, bool bEquipped, float ForceX = -1.f);
	TArray<FString> Wrap(const FString& Text, float Width, float Size, int32 Style) const;
	void DrawText(UCanvas* Canvas, const FString& Text, float X, float Y, float Size, const FLinearColor& Color, int32 Style = 0) const;
	/** Text in the game font (lyshineui/fonts/<file>.font, as in FNWUILayout), size in px at 1080p. */
	void DrawGameText(UCanvas* Canvas, const FString& Text, float X, float Y, float Size, const FLinearColor& Color, const TCHAR* Font, bool bRight = false) const;
	float GameTextWidth(const FString& Text, float Size, const TCHAR* Font) const;
	float GameScale(UFont* Font, int32 Index, float Size) const;
	void DrawTexture(UCanvas* Canvas, const FString& Path, const FBox2f& R, const FLinearColor& Tint = FLinearColor::White, bool bAdditive = false) const;
	float TextWidth(const FString& Text, float Size, int32 Style = 0) const;

	const FNWItem* ItemOf(const FSlotRef& S) const;
	bool Accepts(int32 EquipSlot, const FNWItem& Item) const;
	void Drop(const FVector2D& Mouse);
	void QuickMove(const FSlotRef& S);
	void RightClick(const FSlotRef& S);
	int32 FreeBagSlot() const;
	bool ModifierDown(const FKey& Key) const;
	/** Items of a section (to move the whole section). */
	TArray<int32> SectionSlots(FName Class, bool bStorageItems) const;

	FNWUILayout Window;
	bool bLoaded = false;
	TWeakObjectPtr<ANWCharacter> Character;
	float UIScale = 1.f;
	UCanvas* DrawCanvas = nullptr;              // canvas of the current frame (measure text outside Draw)
	mutable float LineH[8] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};   // line height of each font at 1
	FVector2f Center = FVector2f::ZeroVector;   // screen center in px (the game screens position themselves from it)
	TArray<FSlotRef> Slots;
	FList Lists[2];                             // 0 inventory, 1 storage
	float InvOffsetX = 0.f;                     // 0, or −300 with the storage (newinventory.lua OnTransitionIn)
	FVector2f EquipOrigin = FVector2f::ZeroVector;
	FBox2f EquipArea = FBox2f(ForceInit);       // universal equipment target (equipmentuniversaldroptarget)
	FBox2f TabRects[2];
	FBox2f PointRects[5] = {FBox2f(ForceInit), FBox2f(ForceInit), FBox2f(ForceInit), FBox2f(ForceInit), FBox2f(ForceInit)};
	FBox2f TakeAllRect = FBox2f(ForceInit);
	int32 EquipTab = 0;                         // 0 weapons and armor, 1 tools

	// Drag: pressed on an item (bPressed), moved more than DragStart px (bDragging).
	bool bPressed = false;
	bool bDragging = false;
	FSlotRef Pressed;
	FVector2D PressPos = FVector2D::ZeroVector;
	double HoverSince = 0.0;
	const void* HoverKey = nullptr;

	bool bStorage = false;
	FString StorageTitle;
	// Shared storage tabs (FNWItemRules::StorageTabs): the list shows only the open tab; storing goes into it;
	// dropping an item on a tab moves it there; double click renames.
	static constexpr int32 MaxTabs = 5;
	int32 StorageTab = 0;
	int32 RenameTab = INDEX_NONE;
	FBox2f TabRectsStorage[MaxTabs];
	double LastTabClick = 0.0;
	int32 LastTabClicked = INDEX_NONE;
	TSharedPtr<SEditableText> RenameEdit;
	bool ShowsTabs() const { return bStorage && !bLoot; }
	/** Tab under the mouse (INDEX_NONE outside). */
	int32 TabAt(const FVector2f& M) const;
	/** The stack goes into the second window's list (open storage tab; the bag on the ground shows everything). */
	bool InList(const FNWItem& I, bool bStorageItems) const { return !bStorageItems || !ShowsTabs() || I.Tab == StorageTab; }
	void DrawTabs(UCanvas* Canvas, const FVector2D& Mouse);
	// Loot bag mode: the second window shows LootBag instead of the storage.
	bool bLoot = false;
	TWeakObjectPtr<ANWLootBag> LootBag;
	/** Items of the second window (storage or bag on the ground). */
	const TArray<FNWItem>* ContainerItems() const;
	/** Moves between the bag and the second window; from the bag on the ground it only takes (bDeposit false). */
	void ContainerMove(bool bDeposit, int32 Slot, int32 Quantity);
	void ContainerMoveMany(bool bDeposit, const TArray<int32>& Slots);

	FBox2f ClipRect = FBox2f(ForceInit);         // drawing clip (scrolling list); invalid = no clip
	float InvLeft = 0.f;                          // left edge of the inventory Content (px at 1080p)

	EPopup Popup = EPopup::None;
	FSlotRef PopupSlot;
	int32 PopupGemSlot = INDEX_NONE;   // Socket: gem slot in the bag
	int32 PopupQty = 1, PopupMax = 1;
	TArray<FPopupButton> PopupButtons;
	FBox2f SliderRect = FBox2f(ForceInit);
	bool bSliderDrag = false;

	// Search text fields (Slate, over the canvas), one per list.
	TSharedPtr<SWidget> SearchHost;
	TSharedPtr<SEditableText> SearchEdit[2];
};

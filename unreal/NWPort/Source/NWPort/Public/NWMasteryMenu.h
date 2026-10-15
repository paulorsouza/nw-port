#pragma once

#include "CoreMinimal.h"
#include "NWUILayout.h"

class ANWCharacter;
class UCanvas;

/**
 * Game weapon mastery screen (MasteryTreeWindow of lyshineui/skills/skills.dynamicuicanvas), drawn
 * by FNWUILayout. What the game Lua did comes from here: nodes in the datasheet rows/columns, states
 * (active, passive, ultimate; owned/available), Q/R/F slots, points and tooltip.
 *
 * Click an active ability and then a slot to equip. Passives and ultimate spend points (visual only
 * for now). Tabs 1/2 in the header for the other weapon.
 */
class NWPORT_API FNWMasteryMenu
{
public:
	bool Load();
	bool IsOpen() const { return Character.IsValid(); }
	void Open(ANWCharacter* InCharacter);
	void Close() { Character.Reset(); Pending = NAME_None; }

	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	// true = click consumed; bCloseRequested = close the screen.
	bool Click(const FVector2D& Mouse, bool& bCloseRequested);

private:
	struct FNode
	{
		FString Id;
		FName Action;
		FString Kind;        // active, passive, ultimate
		int32 Tree = 0, Row = 0, Col = 0;
		FString Icon, Background, Name, Description, Parent;
		float Cooldown = 0.f;
		FString ElementPath; // node path on the screen (…/Row2/Node3)
		// Node elements on the screen (fixed after Load).
		FNWUIElement *Root = nullptr, *Rune = nullptr, *Glow = nullptr, *Bg = nullptr, *Border = nullptr, *IconBg = nullptr,
			*IconImg = nullptr, *VLine = nullptr, *HLine = nullptr, *Ultimate = nullptr, *Lock = nullptr, *Ticks = nullptr;
	};
	struct FWeapon
	{
		FString Key, Title, HudIcon;
		TArray<FString> Trees;
		TArray<FNode> Nodes;
	};

	void ApplyWeapon();
	void Refresh();
	int32 SpentInTree(int32 Tree) const;
	int32 PointsLeft() const;
	TArray<FName> Slots() const;
	FNode* NodeAt(const FVector2D& Mouse);
	void DrawTooltip(UCanvas* Canvas, const FNode& N, const FVector2D& Mouse);
	void LoadOwned();
	void SaveOwned();
	FNWUIElement* E(const FString& Suffix) { return Layout.FindOne(TEXT("mastery"), Suffix); }

	FNWUILayout Layout;
	TArray<FWeapon> Weapons;
	int32 Tab = 0;
	TWeakObjectPtr<ANWCharacter> Character;
	FName Pending;                 // active ability chosen, waiting for a slot
	TSet<FString> Owned;           // passives/ultimate marked (per weapon)
	TArray<FBox2f> TabRects;
	float UIScale = 1.f;
	bool bHoverConfirm = false;
	static constexpr int32 TotalPoints = 19;      // mastery points at game level 20
	static constexpr int32 UltimatePoints = 10;   // points in the tree to unlock the ultimate
};

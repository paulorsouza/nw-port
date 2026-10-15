#pragma once

#include "CoreMinimal.h"

class APawn;
class UCanvas;

/**
 * World map (M), like the game's `lyshineui/worldmap/magicmap`: full screen with the tiles of the game's map
 * (Data/World/worldmap.json, tools/make_worldmap.py), gradient on the edges, scale ruler, center and
 * zoom buttons on the right and the player icon turned to where they look. Mouse wheel zooms at the cursor; dragging moves.
 */
class NWPORT_API FNWWorldMap
{
public:
	bool IsOpen() const { return bOpen; }
	void Open(const APawn* Player);
	void Close() { bOpen = false; bDragging = false; }
	void Draw(UCanvas* Canvas, const FVector2D& Mouse, bool bMouseDown, const APawn* Player);
	void Click(const FVector2D& Mouse);
	void Wheel(float Delta, const FVector2D& Mouse);
	/** Right button: places the marker (waypoint) at the map point; again on top of it, removes it. */
	void RightClick(const FVector2D& Mouse);
	/** Player marker (world, m) for the compass; unset = none. */
	TOptional<FVector2D> Waypoint;
	/** Where the player last died (world, m): lastdeathposition icon on the map. */
	TOptional<FVector2D> LastDeath;

	/** Unreal position (cm, city frame) -> game world (m). */
	FVector2D ToWorld(const FVector& Location) const;
	/** Game world (m) -> Unreal plane (cm). */
	FVector2D ToUnreal(const FVector2D& World) const;
	/** Reads Data/World/worldmap.json once (the map and the compass use it). */
	void EnsureLoaded() { if (!bLoaded) Load(); }

	struct FPoi
	{
		FVector2D Pos;   // world, meters
		FString NameKey;
		FString Icon;
	};
	const TArray<FPoi>& GetPois() const { return Pois; }

private:
	struct FTile
	{
		FVector2D Min;   // southwest corner, world meters
		FString Texture;
	};
	struct FLevel
	{
		double Size = 512.0;
		TArray<FTile> Tiles;
	};
	bool Load();
	FVector2D WorldToScreen(const FVector2D& W) const;
	FVector2D ScreenToWorld(const FVector2D& P) const;
	void DrawTexture(UCanvas* Canvas, const FString& Path, const FVector2D& Pos, const FVector2D& Size,
		const FLinearColor& Tint = FLinearColor::White, float Rotation = 0.f, bool bFlipV = false, bool bFlipH = false) const;
	void DrawText(UCanvas* Canvas, const FString& Text, float X, float Y, float Size, const FLinearColor& Color, int32 Style = 0, bool bCenter = false) const;
	void DrawButton(UCanvas* Canvas, const FBox2f& R, const FString& Label, const FVector2D& Mouse) const;

	bool bLoaded = false;
	bool bOpen = false;
	FVector2D Origin = FVector2D::ZeroVector;
	double Angle = 0.0;
	FBox2D Region = FBox2D(ForceInit);
	TArray<FLevel> Levels;
	TArray<FPoi> Pois;

	FVector2D Center = FVector2D::ZeroVector;   // world, at the center of the screen
	double MetersPerPixel = 1.0;
	FVector2D ScreenSize = FVector2D(1920.0, 1080.0);
	float UIScale = 1.f;
	bool bDragging = false;
	bool bCenterOnPlayer = false;
	FVector2D DragLast = FVector2D::ZeroVector;
	FBox2f Buttons[3] = {FBox2f(ForceInit), FBox2f(ForceInit), FBox2f(ForceInit)};   // center, zoom in, zoom out
};

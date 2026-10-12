#pragma once

#include "CoreMinimal.h"

class APawn;
class UCanvas;

/**
 * Mapa do mundo (M), como o `lyshineui/worldmap/magicmap` do jogo: tela cheia com os tiles do mapa do jogo
 * (Data/World/worldmap.json, tools/make_worldmap.py), degradê nas bordas, régua de escala, botões de centralizar e
 * zoom à direita e o ícone do jogador virado para onde ele olha. Roda do mouse aproxima no cursor; arrastar move.
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
	/** Botão direito: põe o marcador (waypoint) no ponto do mapa; de novo em cima dele, tira. */
	void RightClick(const FVector2D& Mouse);
	/** Marcador do jogador (mundo, m) para a bússola; não definido = não há. */
	TOptional<FVector2D> Waypoint;
	/** Onde o jogador morreu por último (mundo, m): ícone lastdeathposition no mapa. */
	TOptional<FVector2D> LastDeath;

	/** Posição do Unreal (cm, referencial da cidade) -> mundo do jogo (m). */
	FVector2D ToWorld(const FVector& Location) const;
	/** Mundo do jogo (m) -> plano do Unreal (cm). */
	FVector2D ToUnreal(const FVector2D& World) const;
	/** Lê Data/World/worldmap.json uma vez (o mapa e a bússola usam). */
	void EnsureLoaded() { if (!bLoaded) Load(); }

	struct FPoi
	{
		FVector2D Pos;   // mundo, metros
		FString NameKey;
		FString Icon;
	};
	const TArray<FPoi>& GetPois() const { return Pois; }

private:
	struct FTile
	{
		FVector2D Min;   // canto sudoeste, metros do mundo
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

	FVector2D Center = FVector2D::ZeroVector;   // mundo, no meio da tela
	double MetersPerPixel = 1.0;
	FVector2D ScreenSize = FVector2D(1920.0, 1080.0);
	float UIScale = 1.f;
	bool bDragging = false;
	bool bCenterOnPlayer = false;
	FVector2D DragLast = FVector2D::ZeroVector;
	FBox2f Buttons[3] = {FBox2f(ForceInit), FBox2f(ForceInit), FBox2f(ForceInit)};   // centralizar, aproximar, afastar
};

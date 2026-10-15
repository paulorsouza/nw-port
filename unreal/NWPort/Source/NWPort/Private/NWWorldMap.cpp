#include "NWWorldMap.h"

#include "CanvasItem.h"
#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "NWPlayerState.h"
#include "Misc/FileHelper.h"
#include "NWText.h"
#include "NWUILayout.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const FLinearColor MapTan(FColor(0xEB, 0xE3, 0xD6));
	const FLinearColor MapGold(FColor(0xED, 0xC6, 0x3E));
	// Zoom: from 0.25 m per pixel (level 2 tiles enlarged) to 6 m per pixel (whole region and surroundings).
	constexpr double MinMpp = 0.25, MaxMpp = 6.0;
	const TCHAR* Img = TEXT("lyshineui/images/");
}

bool FNWWorldMap::Load()
{
	bLoaded = true;
	FString Raw;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Raw, *(FPaths::ProjectDir() / TEXT("Data/World/worldmap.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>& O = Root->GetArrayField(TEXT("origin"));
	Origin = FVector2D(O[0]->AsNumber(), O[1]->AsNumber());
	Angle = Root->GetNumberField(TEXT("angle"));
	const TArray<TSharedPtr<FJsonValue>>& R = Root->GetArrayField(TEXT("region"));
	Region = FBox2D(FVector2D(R[0]->AsNumber(), R[1]->AsNumber()), FVector2D(R[2]->AsNumber(), R[3]->AsNumber()));
	for (const TSharedPtr<FJsonValue>& LV : Root->GetArrayField(TEXT("levels")))
	{
		FLevel& L = Levels.AddDefaulted_GetRef();
		L.Size = LV->AsObject()->GetNumberField(TEXT("size"));
		for (const TSharedPtr<FJsonValue>& TV : LV->AsObject()->GetArrayField(TEXT("tiles")))
		{
			const TSharedPtr<FJsonObject> T = TV->AsObject();
			L.Tiles.Add({FVector2D(T->GetNumberField(TEXT("x")), T->GetNumberField(TEXT("y"))), T->GetStringField(TEXT("tex"))});
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* PoiArray = nullptr;
	if (Root->TryGetArrayField(TEXT("pois"), PoiArray))
	{
		for (const TSharedPtr<FJsonValue>& PV : *PoiArray)
		{
			const TSharedPtr<FJsonObject> P = PV->AsObject();
			Pois.Add({FVector2D(P->GetNumberField(TEXT("x")), P->GetNumberField(TEXT("y"))), P->GetStringField(TEXT("name")), P->GetStringField(TEXT("icon"))});
		}
	}
	return Levels.Num() > 0;
}

FVector2D FNWWorldMap::ToWorld(const FVector& L) const
{
	// UE = (-lx, ly) · 100; world = origin + rotation(angle) · local.
	const double Lx = -L.X / 100.0, Ly = L.Y / 100.0;
	const double C = FMath::Cos(Angle), S = FMath::Sin(Angle);
	return Origin + FVector2D(C * Lx - S * Ly, S * Lx + C * Ly);
}

FVector2D FNWWorldMap::ToUnreal(const FVector2D& W) const
{
	const FVector2D D = W - Origin;
	const double C = FMath::Cos(Angle), S = FMath::Sin(Angle);
	const double Lx = C * D.X + S * D.Y, Ly = -S * D.X + C * D.Y;
	return FVector2D(-Lx * 100.0, Ly * 100.0);
}

FVector2D FNWWorldMap::WorldToScreen(const FVector2D& W) const
{
	// North (world y) up.
	return ScreenSize * 0.5 + FVector2D(W.X - Center.X, Center.Y - W.Y) / MetersPerPixel;
}

FVector2D FNWWorldMap::ScreenToWorld(const FVector2D& P) const
{
	const FVector2D D = (P - ScreenSize * 0.5) * MetersPerPixel;
	return FVector2D(Center.X + D.X, Center.Y - D.Y);
}

void FNWWorldMap::Open(const APawn* Player)
{
	EnsureLoaded();
	bOpen = Levels.Num() > 0;
	MetersPerPixel = 1.0;
	Center = Player ? ToWorld(Player->GetActorLocation()) : Region.GetCenter();
}

void FNWWorldMap::DrawTexture(UCanvas* Canvas, const FString& Path, const FVector2D& Pos, const FVector2D& Size,
	const FLinearColor& Tint, float Rotation, bool bFlipV, bool bFlipH) const
{
	UTexture2D* Tex = FNWUILayout::GameTexture(Path);
	if (!Tex)
	{
		return;
	}
	FCanvasTileItem T(Pos, Tex->GetResource(), Size, Tint);
	T.BlendMode = SE_BLEND_Translucent;
	T.UV0 = FVector2D(bFlipH ? 1.0 : 0.0, bFlipV ? 1.0 : 0.0);
	T.UV1 = FVector2D(bFlipH ? 0.0 : 1.0, bFlipV ? 0.0 : 1.0);
	if (Rotation != 0.f)
	{
		T.Rotation = FRotator(0.f, Rotation, 0.f);
		T.PivotPoint = FVector2D(0.5, 0.5);
	}
	Canvas->DrawItem(T);
}

void FNWWorldMap::DrawText(UCanvas* Canvas, const FString& Text, float X, float Y, float Size, const FLinearColor& Color, int32 Style, bool bCenter) const
{
	UFont* Font = FNWUILayout::GameFont(Style);
	if (!Font || Text.IsEmpty())
	{
		return;
	}
	const float Scale = Size * UIScale / Font->LegacyFontSize;
	if (bCenter)
	{
		X -= Font->GetStringSize(*Text) * Scale * 0.5f;
	}
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Color);
	Item.Scale = FVector2D(Scale, Scale);
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	Canvas->DrawItem(Item);
}

void FNWWorldMap::DrawButton(UCanvas* Canvas, const FBox2f& R, const FString& Label, const FVector2D& Mouse) const
{
	// magicmap square button: dark buttonbg, heroframe border and focus on mouse hover.
	const FVector2D P(R.Min), Sz(R.GetSize());
	DrawTexture(Canvas, FString(Img) + TEXT("slices/button/buttonbg.dds"), P, Sz, FLinearColor(0.17f, 0.15f, 0.11f));
	if (R.IsInside(FVector2f(Mouse)))
	{
		DrawTexture(Canvas, FString(Img) + TEXT("slices/button/focusnew.dds"), P, Sz);
	}
	DrawTexture(Canvas, FString(Img) + TEXT("slices/button/heroframedisabled2023.dds"), P, Sz);
	DrawText(Canvas, Label, R.GetCenter().X, R.Min.Y + 9.f * UIScale, 24.f, MapTan, 1, true);
}

void FNWWorldMap::Draw(UCanvas* Canvas, const FVector2D& Mouse, bool bMouseDown, const APawn* Player)
{
	if (!bOpen)
	{
		return;
	}
	ScreenSize = FVector2D(Canvas->ClipX, Canvas->ClipY);
	UIScale = Canvas->ClipY / 1080.f;
	const float S = UIScale;

	// Dragging moves the map (starts outside the buttons).
	if (bMouseDown)
	{
		const bool bOnButton = Buttons[0].IsInside(FVector2f(Mouse)) || Buttons[1].IsInside(FVector2f(Mouse)) || Buttons[2].IsInside(FVector2f(Mouse));
		if (bDragging)
		{
			const FVector2D D = (Mouse - DragLast) * MetersPerPixel;
			Center += FVector2D(-D.X, D.Y);
		}
		else if (!bOnButton)
		{
			bDragging = true;
		}
		DragLast = Mouse;
	}
	else
	{
		bDragging = false;
	}
	if (bCenterOnPlayer && Player)
	{
		Center = ToWorld(Player->GetActorLocation());
		bCenterOnPlayer = false;
	}
	// Does not let it leave the vicinity of the region.
	Center.X = FMath::Clamp(Center.X, Region.Min.X - 256.0, Region.Max.X + 256.0);
	Center.Y = FMath::Clamp(Center.Y, Region.Min.Y - 256.0, Region.Max.Y + 256.0);

	FCanvasTileItem Bg(FVector2D::ZeroVector, ScreenSize, FLinearColor(0.02f, 0.02f, 0.02f, 1.f));
	Canvas->DrawItem(Bg);

	// Level 2 (2 px/m) up close, level 3 (1 px/m) from afar.
	const FLevel& L = Levels[MetersPerPixel > 1.0 && Levels.Num() > 1 ? 1 : 0];
	for (const FTile& T : L.Tiles)
	{
		const FVector2D TopLeft = WorldToScreen(FVector2D(T.Min.X, T.Min.Y + L.Size));
		const FVector2D Size(L.Size / MetersPerPixel);
		if (TopLeft.X > ScreenSize.X || TopLeft.Y > ScreenSize.Y || TopLeft.X + Size.X < 0.0 || TopLeft.Y + Size.Y < 0.0)
		{
			continue;
		}
		if (UTexture2D* Tex = FNWUILayout::GameTexture(T.Texture))
		{
			// +1 px so no gap opens between tiles.
			FCanvasTileItem Tile(TopLeft, Tex->GetResource(), Size + FVector2D(1.0), FLinearColor::White);
			Tile.BlendMode = SE_BLEND_Opaque;
			Canvas->DrawItem(Tile);
		}
	}

	// Edge of the port's region (what exists in the Unreal map).
	{
		const FVector2D A = WorldToScreen(FVector2D(Region.Min.X, Region.Max.Y)), B = WorldToScreen(FVector2D(Region.Max.X, Region.Min.Y));
		const FLinearColor Edge = MapGold.CopyWithNewOpacity(0.5f);
		const FVector2D Corners[] = {A, FVector2D(B.X, A.Y), B, FVector2D(A.X, B.Y)};
		for (int32 i = 0; i < 4; ++i)
		{
			FCanvasLineItem Line(Corners[i], Corners[(i + 1) % 4]);
			Line.SetColor(Edge);
			Line.LineThickness = 2.f;
			Canvas->DrawItem(Line);
		}
	}

	// POIs: the game's icon; the name shows up close or on mouse hover.
	{
		const float Sz = 40.f * S;
		const FPoi* Hover = nullptr;
		for (const FPoi& P : Pois)
		{
			const FVector2D C = WorldToScreen(P.Pos);
			if (C.X < -Sz || C.Y < -Sz || C.X > ScreenSize.X + Sz || C.Y > ScreenSize.Y + Sz)
			{
				continue;
			}
			DrawTexture(Canvas, P.Icon, C - FVector2D(Sz * 0.5f), FVector2D(Sz));
			if (FVector2D::Distance(C, Mouse) < Sz * 0.5f)
			{
				Hover = &P;
			}
			else if (MetersPerPixel <= 0.8)
			{
				DrawText(Canvas, NWText::Get(P.NameKey), C.X, C.Y + Sz * 0.5f, 15.f, MapTan, 0, true);
			}
		}
		if (Hover)
		{
			const FVector2D C = WorldToScreen(Hover->Pos);
			DrawText(Canvas, NWText::Get(Hover->NameKey), C.X, C.Y + Sz * 0.5f, 19.f, MapGold, 1, true);
		}
	}

	// Marker, last death location and group members (the game's icons).
	{
		const float Sz = 34.f * S;
		if (LastDeath)
		{
			DrawTexture(Canvas, FString(Img) + TEXT("map/icon/lastdeathposition.dds"), WorldToScreen(*LastDeath) - FVector2D(Sz * 0.5f), FVector2D(Sz));
		}
		if (Waypoint)
		{
			DrawTexture(Canvas, FString(Img) + TEXT("map/icon/waypoint.dds"), WorldToScreen(*Waypoint) - FVector2D(Sz * 0.5f, Sz), FVector2D(Sz));
		}
		const ANWPlayerState* MyState = Player ? Player->GetPlayerState<ANWPlayerState>() : nullptr;
		const AGameStateBase* GS = Player && Player->GetWorld() ? Player->GetWorld()->GetGameState() : nullptr;
		if (MyState && GS)
		{
			for (const APlayerState* PS : GS->PlayerArray)
			{
				const ANWPlayerState* Other = Cast<ANWPlayerState>(PS);
				const APawn* Pawn = Other ? Other->GetPawn() : nullptr;
				if (Other && Other != MyState && Pawn && MyState->IsGroupedWith(Other))
				{
					const FVector2D P = WorldToScreen(ToWorld(Pawn->GetActorLocation()));
					DrawTexture(Canvas, FString(Img) + TEXT("map/icon/groupmember.dds"), P - FVector2D(Sz * 0.4f), FVector2D(Sz * 0.8f));
					DrawText(Canvas, Other->GetPlayerName(), P.X, P.Y + Sz * 0.45f, 14.f, MapTan, 0, true);
				}
			}
		}
	}

	// Player: the game's icon turned to where the character looks.
	if (Player)
	{
		const FVector Loc = Player->GetActorLocation();
		const FVector2D P = WorldToScreen(ToWorld(Loc));
		const FVector Fwd = Player->GetActorForwardVector();
		const FVector2D Ahead = WorldToScreen(ToWorld(Loc + Fwd * 100.f));
		const float Heading = FMath::RadiansToDegrees(FMath::Atan2(Ahead.Y - P.Y, Ahead.X - P.X)) + 90.f;   // icon points up
		const float Sz = 44.f * S;
		DrawTexture(Canvas, FString(Img) + TEXT("map/icon/player.dds"), P - FVector2D(Sz * 0.5f), FVector2D(Sz), FLinearColor::White, Heading);
	}

	// Edge gradient (map_gradient top and bottom, map_gradientside on the sides).
	const float GH = 220.f * S, GW = 300.f * S;
	DrawTexture(Canvas, FString(Img) + TEXT("map/map_gradient.dds"), FVector2D::ZeroVector, FVector2D(ScreenSize.X, GH));
	DrawTexture(Canvas, FString(Img) + TEXT("map/map_gradient.dds"), FVector2D(0.0, ScreenSize.Y - GH), FVector2D(ScreenSize.X, GH), FLinearColor::White, 0.f, true);
	DrawTexture(Canvas, FString(Img) + TEXT("map/map_gradientside.dds"), FVector2D::ZeroVector, FVector2D(GW, ScreenSize.Y));
	DrawTexture(Canvas, FString(Img) + TEXT("map/map_gradientside.dds"), FVector2D(ScreenSize.X - GW, 0.0), FVector2D(GW, ScreenSize.Y), FLinearColor::White, 0.f, false, true);

	// Scale ruler (ScaleKey): round distance that fits in ~140 px.
	{
		const double Target = 140.0 * S * MetersPerPixel;
		const double Nice[] = {10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000};
		double Meters = Nice[0];
		for (double N : Nice)
		{
			if (N <= Target) Meters = N;
		}
		const float W = float(Meters / MetersPerPixel);
		const float X = ScreenSize.X * 0.5f - W * 0.5f, Y = ScreenSize.Y - 110.f * S;
		DrawTexture(Canvas, FString(Img) + TEXT("map/scale_key.dds"), FVector2D(X, Y), FVector2D(W, 30.f * S));
		DrawText(Canvas, Meters >= 1000.0 ? FString::Printf(TEXT("%.0f km"), Meters / 1000.0) : FString::Printf(TEXT("%.0f m"), Meters),
			ScreenSize.X * 0.5f, Y + 32.f * S, 22.f, MapTan, 0, true);
	}

	// The player's game coordinates (like the game's /coordenadas), bottom left.
	if (Player)
	{
		const FVector2D W = ToWorld(Player->GetActorLocation());
		DrawText(Canvas, FString::Printf(TEXT("[%.0f, %.0f]"), W.X, W.Y), 60.f * S, ScreenSize.Y - 80.f * S, 20.f, MapTan);
	}

	// Buttons on the right: center on the player, zoom in, zoom out.
	const float B = 50.f * S, BX = ScreenSize.X - 80.f * S, BY = ScreenSize.Y * 0.5f - 85.f * S;
	static const TCHAR* Labels[] = {TEXT("o"), TEXT("+"), TEXT("-")};
	for (int32 i = 0; i < 3; ++i)
	{
		Buttons[i] = FBox2f(FVector2f(BX, BY + i * 60.f * S), FVector2f(BX + B, BY + i * 60.f * S + B));
		DrawButton(Canvas, Buttons[i], Labels[i], Mouse);
	}
}

void FNWWorldMap::Click(const FVector2D& Mouse)
{
	if (Buttons[0].IsInside(FVector2f(Mouse)))
	{
		bDragging = false;
		bCenterOnPlayer = true;   // on the next draw, which has the player
		return;
	}
	if (Buttons[1].IsInside(FVector2f(Mouse)) || Buttons[2].IsInside(FVector2f(Mouse)))
	{
		Wheel(Buttons[1].IsInside(FVector2f(Mouse)) ? 1.f : -1.f, ScreenSize * 0.5);
	}
}

void FNWWorldMap::RightClick(const FVector2D& Mouse)
{
	if (Waypoint && FVector2D::Distance(WorldToScreen(*Waypoint), Mouse) < 24.f * UIScale)
	{
		Waypoint.Reset();
		return;
	}
	Waypoint = ScreenToWorld(Mouse);
}

void FNWWorldMap::Wheel(float Delta, const FVector2D& Mouse)
{
	// Zooms at the cursor: the point under the mouse stays still.
	const FVector2D Before = ScreenToWorld(Mouse);
	MetersPerPixel = FMath::Clamp(MetersPerPixel * (Delta > 0.f ? 0.8 : 1.25), MinMpp, MaxMpp);
	const FVector2D After = ScreenToWorld(Mouse);
	Center += Before - After;
}

#include "NWUILayout.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TextureResource.h"
#include "Engine/Engine.h"
#include "Fonts/CompositeFont.h"
#include "UObject/Package.h"

static FLinearColor ReadColor(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, float Alpha)
{
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (!O->TryGetArrayField(Field, A) || A->Num() < 3)
	{
		return FLinearColor(1, 1, 1, Alpha);
	}
	const float W = A->Num() > 3 ? (*A)[3]->AsNumber() : 1.f;
	// Values as LyShine stores them (0..1), multiplying the texture directly, like the Canvas HUD colors.
	return FLinearColor((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber(), W * Alpha);
}

static FVector4f ReadVec4(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FVector4f& Default)
{
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (!O->TryGetArrayField(Field, A) || A->Num() < 4)
	{
		return Default;
	}
	return FVector4f((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber(), (*A)[3]->AsNumber());
}

static FVector2f ReadVec2(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FVector2f& Default)
{
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (!O->TryGetArrayField(Field, A) || A->Num() < 2)
	{
		return Default;
	}
	return FVector2f((*A)[0]->AsNumber(), (*A)[1]->AsNumber());
}

bool FNWUILayout::Load(const FString& JsonFile)
{
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *JsonFile))
	{
		UE_LOG(LogTemp, Warning, TEXT("NW UI: layout not found at %s"), *JsonFile);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
	{
		return false;
	}
	Canvases.Reset();
	for (const TSharedPtr<FJsonValue>& CV : Root->GetArrayField(TEXT("canvases")))
	{
		const TSharedPtr<FJsonObject> C = CV->AsObject();
		FNWUICanvasLayout& L = Canvases.AddDefaulted_GetRef();
		L.Name = C->GetStringField(TEXT("name"));
		L.Size = ReadVec2(C, TEXT("size"), FVector2f(1920.f, 1080.f));
		for (const TSharedPtr<FJsonValue>& EV : C->GetArrayField(TEXT("elements")))
		{
			const TSharedPtr<FJsonObject> O = EV->AsObject();
			FNWUIElement& E = L.Elements.AddDefaulted_GetRef();
			E.Name = O->GetStringField(TEXT("name"));
			E.Path = O->GetStringField(TEXT("path"));
			E.Parent = O->GetIntegerField(TEXT("parent"));
			E.Anchors = ReadVec4(O, TEXT("anchors"), E.Anchors);
			E.Offsets = ReadVec4(O, TEXT("offsets"), E.Offsets);
			E.Pivot = ReadVec2(O, TEXT("pivot"), E.Pivot);
			E.Scale = ReadVec2(O, TEXT("scale"), E.Scale);
			O->TryGetNumberField(TEXT("fade"), E.Fade);
			O->TryGetBoolField(TEXT("fit_w"), E.bFitWidth);
			O->TryGetBoolField(TEXT("enabled"), E.bEnabled);
			O->TryGetBoolField(TEXT("mask"), E.bMask);
			O->TryGetBoolField(TEXT("mask_draw"), E.bMaskDraw);
			O->TryGetNumberField(TEXT("end"), E.EndIndex);
			const TSharedPtr<FJsonObject>* Img = nullptr;
			if (O->TryGetObjectField(TEXT("image"), Img))
			{
				const TSharedPtr<FJsonObject>& I = *Img;
				E.bHasImage = true;
				E.Texture = I->GetStringField(TEXT("texture")).ToLower();
				double A = 1.0;
				I->TryGetNumberField(TEXT("alpha"), A);
				E.Color = ReadColor(I, TEXT("color"), A);
				const FString Type = I->GetStringField(TEXT("type"));
				E.ImageType = Type == TEXT("sliced") ? 1 : Type == TEXT("stretched_fit") ? 4 : 0;
				int32 BlendMode = 0;
				I->TryGetNumberField(TEXT("blend"), BlendMode);
				E.Blend = uint8(BlendMode);
				const FString Fill = I->GetStringField(TEXT("fill"));
				E.FillType = Fill == TEXT("linear") ? 1 : Fill.StartsWith(TEXT("radial")) ? 2 : 0;
				I->TryGetNumberField(TEXT("fill_amount"), E.FillAmount);
				I->TryGetNumberField(TEXT("fill_start_angle"), E.FillStartAngle);
				I->TryGetBoolField(TEXT("fill_clockwise"), E.bFillClockwise);
				I->TryGetNumberField(TEXT("fill_edge_origin"), E.FillEdgeOrigin);
				const TSharedPtr<FJsonObject>* B = nullptr;
				if (I->TryGetObjectField(TEXT("borders"), B))
				{
					E.bHasBorders = true;
					E.Borders = FVector4f((*B)->GetNumberField(TEXT("left")), (*B)->GetNumberField(TEXT("right")),
						(*B)->GetNumberField(TEXT("top")), (*B)->GetNumberField(TEXT("bottom")));
				}
			}
			const TSharedPtr<FJsonObject>* Txt = nullptr;
			if (O->TryGetObjectField(TEXT("text"), Txt))
			{
				const TSharedPtr<FJsonObject>& T = *Txt;
				E.bHasText = true;
				E.Text = T->GetStringField(TEXT("text"));
				const FString Font = T->GetStringField(TEXT("font"));
				E.FontStyle = Font.Contains(TEXT("pica_italic")) ? 4 : Font.Contains(TEXT("pica")) ? 3 : Font.Contains(TEXT("caslon")) ? 5
					: Font.Contains(TEXT("semibold")) ? 1 : Font.Contains(TEXT("bold")) ? 2 : 0;
				double Spacing = 0.0;
				T->TryGetNumberField(TEXT("spacing"), Spacing);
				E.CharSpacing = float(Spacing);
				double Case = 0.0;
				T->TryGetNumberField(TEXT("case"), Case);
				E.CaseOverride = int32(Case);
				T->TryGetNumberField(TEXT("size"), E.FontSize);
				double A = 1.0;
				T->TryGetNumberField(TEXT("alpha"), A);
				E.TextColor = ReadColor(T, TEXT("color"), A);
				T->TryGetNumberField(TEXT("halign"), E.HAlign);
				T->TryGetNumberField(TEXT("valign"), E.VAlign);
				E.FontFile = Font;
				int32 Wrap = 0;
				T->TryGetNumberField(TEXT("wrap"), Wrap);
				E.bWrap = Wrap != 0;
				T->TryGetNumberField(TEXT("linespacing"), E.LineSpacing);
				T->TryGetNumberField(TEXT("shrink"), E.ShrinkToFit);
				T->TryGetNumberField(TEXT("minshrink"), E.MinShrink);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("NW UI: %d screens loaded from %s"), Canvases.Num(), *JsonFile);
	return true;
}

TArray<FNWUIElement*> FNWUILayout::Find(const FString& Canvas, const FString& PathSuffix)
{
	TArray<FNWUIElement*> Out;
	for (FNWUICanvasLayout& L : Canvases)
	{
		if (L.Name != Canvas)
		{
			continue;
		}
		for (FNWUIElement& E : L.Elements)
		{
			if (E.Path.EndsWith(PathSuffix) && (E.Path.Len() == PathSuffix.Len() || E.Path[E.Path.Len() - PathSuffix.Len() - 1] == TEXT('/')))
			{
				Out.Add(&E);
			}
		}
	}
	return Out;
}

FNWUIElement* FNWUILayout::FindOne(const FString& Canvas, const FString& PathSuffix)
{
	TArray<FNWUIElement*> All = Find(Canvas, PathSuffix);
	return All.Num() > 0 ? All[0] : nullptr;
}

FString FNWUILayout::AssetPathFor(const FString& GameTexture)
{
	const FString NoExt = FPaths::ChangeExtension(GameTexture.ToLower(), TEXT(""));
	const FString Dir = FPaths::GetPath(NoExt);
	const FString Name = TEXT("T_") + FPaths::GetCleanFilename(NoExt);
	return FString::Printf(TEXT("/Game/NW/UI/%s/%s.%s"), *Dir, *Name, *Name);
}

UFont* FNWUILayout::GameFont(int32 Style)
{
	static UFont* Cache[8] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
	Style = FMath::Clamp(Style, 0, 7);
	if (Cache[Style])
	{
		return Cache[Style];
	}
	// The editor's font importer asks for confirmation in a window and does not run in a commandlet: reads the .otf directly.
	static const TCHAR* Files[8] = {TEXT("nimbussannov-reg.otf"), TEXT("nimbussannov-sembol.otf"), TEXT("nimbussannov-bol.otf"),
		TEXT("imfepirm28p.ttf"), TEXT("imfepiit28p.ttf"), TEXT("caslonant.otf"), TEXT("nimbussannov-regita.otf"),
		TEXT("nimbussannov-med.otf")};   // Pica = IM Fell (pica_regular.font)
	const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Data/UI/Fonts") / Files[Style]);
	if (!FPaths::FileExists(File))
	{
		UE_LOG(LogTemp, Warning, TEXT("NW UI: font not found %s"), *File);
		return GEngine->GetMediumFont();
	}
	UFont* F = NewObject<UFont>(GetTransientPackage());
	F->AddToRoot();
	F->FontCacheType = EFontCacheType::Runtime;
	F->LegacyFontSize = 24;
	FTypefaceEntry& Entry = F->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.AddDefaulted_GetRef();
	Entry.Name = TEXT("Regular");
	Entry.Font = FFontData(File, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	Cache[Style] = F;
	return F;
}

UTexture2D* FNWUILayout::GameTexture(const FString& GameTexturePath)
{
	// An empty path becomes "/Game/NW/UI//T_" and Unreal crashes the process when creating the package.
	if (GameTexturePath.IsEmpty() || FPaths::GetBaseFilename(GameTexturePath).IsEmpty())
	{
		return nullptr;
	}
	static TMap<FString, UTexture2D*> Cache;
	if (UTexture2D** Found = Cache.Find(GameTexturePath))
	{
		return *Found;
	}
	UTexture2D* T = LoadObject<UTexture2D>(nullptr, *AssetPathFor(GameTexturePath));
	if (T)
	{
		T->AddToRoot();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("NW UI: texture not imported %s"), *GameTexturePath);
	}
	Cache.Add(GameTexturePath, T);
	return T;
}

void FNWUILayout::Draw(UCanvas* Canvas)
{
	for (FNWUICanvasLayout& L : Canvases)
	{
		DrawCanvas(Canvas, L);
	}
}

void FNWUILayout::DrawOne(UCanvas* Canvas, const FString& CanvasName)
{
	for (FNWUICanvasLayout& L : Canvases)
	{
		if (L.Name == CanvasName)
		{
			DrawCanvas(Canvas, L);
		}
	}
}

void FNWUILayout::DrawCanvas(UCanvas* Canvas, FNWUICanvasLayout& L)
{
	// Reference screen scaled by height; the width follows the window's aspect ratio (anchors resolve it).
	const float UIScale = Canvas->ClipY / L.Size.Y;
	const FBox2f Screen(FVector2f(0.f, 0.f), FVector2f(Canvas->ClipX / UIScale, L.Size.Y));
	const FBox2f FullClip(FVector2f(0.f, 0.f), FVector2f(Canvas->ClipX, Canvas->ClipY));
	TArray<FBox2f> Rects;
	TArray<float> Alphas;
	TArray<bool> Shown;
	// bScaleChildren: rectangle in the parent's unscaled frame and the accumulated transform (screen = K * local + B).
	TArray<FBox2f> Locals;
	TArray<FVector2f> K, B;
	if (bScaleChildren)
	{
		Locals.SetNum(L.Elements.Num());
		K.Init(FVector2f(1.f, 1.f), L.Elements.Num());
		B.Init(FVector2f(0.f, 0.f), L.Elements.Num());
	}
	Rects.SetNum(L.Elements.Num());
	Alphas.SetNum(L.Elements.Num());
	Shown.SetNum(L.Elements.Num());
	L.LastRects.SetNum(L.Elements.Num());
	L.LastClips.SetNum(L.Elements.Num());
	L.LastShown.SetNum(L.Elements.Num());
	TArray<TPair<int32, FBox2f>> Clips;   // (end of the subtree, clip in pixels)
	for (int32 i = 0; i < L.Elements.Num(); ++i)
	{
		while (Clips.Num() > 0 && i > Clips.Last().Key)
		{
			Clips.Pop();
		}
		FNWUIElement& E = L.Elements[i];
		const bool bHasParent = L.Elements.IsValidIndex(E.Parent);
		const FBox2f P = bHasParent ? (bScaleChildren ? Locals[E.Parent] : Rects[E.Parent]) : Screen;
		const FVector2f PS = P.GetSize();
		float Left = P.Min.X + E.Anchors.X * PS.X + E.Offsets.X + E.Translate.X;
		float Top = P.Min.Y + E.Anchors.Y * PS.Y + E.Offsets.Y + E.Translate.Y;
		float Right = P.Min.X + E.Anchors.Z * PS.X + E.Offsets.Z + E.Translate.X;
		float Bottom = P.Min.Y + E.Anchors.W * PS.Y + E.Offsets.W + E.Translate.Y;
		if (bExactText && E.bFitWidth && E.bHasText)
		{
			// UiLayoutFitter: text width, around the pivot.
			const float FitW = TextWidth(Canvas, E, UIScale);
			const float PX = Left + (Right - Left) * E.Pivot.X;
			Left = PX - FitW * E.Pivot.X;
			Right = Left + FitW;
		}
		float ElemScale = UIScale;   // reference px -> screen px for this element's images and text
		if (bScaleChildren)
		{
			Locals[i] = FBox2f(FVector2f(Left, Top), FVector2f(Right, Bottom));
			const FVector2f C(Left + (Right - Left) * E.Pivot.X, Top + (Bottom - Top) * E.Pivot.Y);
			const FVector2f KP = bHasParent ? K[E.Parent] : FVector2f(1.f, 1.f);
			const FVector2f BP = bHasParent ? B[E.Parent] : FVector2f(0.f, 0.f);
			K[i] = KP * E.Scale;
			B[i] = KP * (C - E.Scale * C) + BP;
			Rects[i] = FBox2f(K[i] * Locals[i].Min + B[i], K[i] * Locals[i].Max + B[i]);
			ElemScale = UIScale * K[i].Y;
		}
		else
		{
			if (E.Scale.X != 1.f || E.Scale.Y != 1.f)
			{
				const float CX = Left + (Right - Left) * E.Pivot.X, CY = Top + (Bottom - Top) * E.Pivot.Y;
				Left = CX - (CX - Left) * E.Scale.X; Right = CX + (Right - CX) * E.Scale.X;
				Top = CY - (CY - Top) * E.Scale.Y; Bottom = CY + (Bottom - CY) * E.Scale.Y;
			}
			Rects[i] = FBox2f(FVector2f(Left, Top), FVector2f(Right, Bottom));
		}
		Alphas[i] = (bHasParent ? Alphas[E.Parent] : 1.f) * E.Fade * E.AlphaMul;
		Shown[i] = (bHasParent ? Shown[E.Parent] : true) && E.Visible.Get(E.bEnabled);
		const FBox2f Px(Rects[i].Min * UIScale, Rects[i].Max * UIScale);
		FBox2f Clip = Clips.Num() > 0 ? Clips.Last().Value : FullClip;
		L.LastRects[i] = Px;
		L.LastClips[i] = Clip;
		L.LastShown[i] = Shown[i] && Alphas[i] > 0.005f;
		if (!Shown[i])
		{
			continue;
		}
		if (E.bMask)
		{
			FBox2f Inner(FVector2f(FMath::Max(Px.Min.X, Clip.Min.X), FMath::Max(Px.Min.Y, Clip.Min.Y)),
				FVector2f(FMath::Min(Px.Max.X, Clip.Max.X), FMath::Min(Px.Max.Y, Clip.Max.Y)));
			Clips.Add({E.EndIndex >= i ? E.EndIndex : i, Inner});
			if (!E.bMaskDraw)
			{
				continue;
			}
		}
		if (Alphas[i] <= 0.005f || Clip.Max.X <= Clip.Min.X || Clip.Max.Y <= Clip.Min.Y)
		{
			continue;
		}
		if (E.bHasImage)
		{
			DrawImage(Canvas, E, Px, Alphas[i], ElemScale, Clip);
		}
		if (E.bHasText)
		{
			DrawText(Canvas, E, Px, Alphas[i], ElemScale, Clip);
		}
	}
}

int32 FNWUILayout::HitTest(const FString& CanvasName, const FVector2D& Point, TFunctionRef<bool(const FNWUIElement&)> Accept) const
{
	const FVector2f Pt(Point);
	for (const FNWUICanvasLayout& L : Canvases)
	{
		if (L.Name != CanvasName)
		{
			continue;
		}
		for (int32 i = L.LastShown.Num() - 1; i >= 0; --i)
		{
			if (L.LastShown[i] && L.LastRects[i].IsInside(Pt) && L.LastClips[i].IsInside(Pt) && Accept(L.Elements[i]))
			{
				return i;
			}
		}
	}
	return INDEX_NONE;
}

const FNWUIElement* FNWUILayout::Element(const FString& CanvasName, int32 Index) const
{
	for (const FNWUICanvasLayout& L : Canvases)
	{
		if (L.Name == CanvasName && L.Elements.IsValidIndex(Index))
		{
			return &L.Elements[Index];
		}
	}
	return nullptr;
}

FBox2f FNWUILayout::LastRect(const FString& CanvasName, int32 Index) const
{
	for (const FNWUICanvasLayout& L : Canvases)
	{
		if (L.Name == CanvasName && L.LastRects.IsValidIndex(Index))
		{
			return L.LastRects[Index];
		}
	}
	return FBox2f(ForceInit);
}

bool FNWUILayout::WasShown(const FNWUIElement* Element) const
{
	for (const FNWUICanvasLayout& L : Canvases)
	{
		if (L.Elements.Num() > 0 && Element >= &L.Elements[0] && Element <= &L.Elements.Last())
		{
			const int32 Index = int32(Element - &L.Elements[0]);
			return L.LastShown.IsValidIndex(Index) && L.LastShown[Index];
		}
	}
	return false;
}

FBox2f FNWUILayout::RectOf(const FNWUIElement* Element) const
{
	for (const FNWUICanvasLayout& L : Canvases)
	{
		if (L.Elements.Num() > 0 && Element >= &L.Elements[0] && Element <= &L.Elements.Last())
		{
			const int32 Index = int32(Element - &L.Elements[0]);
			return L.LastRects.IsValidIndex(Index) ? L.LastRects[Index] : FBox2f(ForceInit);
		}
	}
	return FBox2f(ForceInit);
}

// Draw state of an image (clip and blend) used by the pieces.
static FBox2f GClip(ForceInit);
static ESimpleElementBlendMode GBlend = SE_BLEND_Translucent;

static void Tile(UCanvas* Canvas, const FTexture* Tex, const FVector2f& Pos, const FVector2f& Size, const FVector2f& UV0, const FVector2f& UV1, const FLinearColor& Color)
{
	if (Size.X <= 0.f || Size.Y <= 0.f)
	{
		return;
	}
	// Clip by the mask: cuts the rectangle and the UVs in the same proportion.
	FVector2f P0 = Pos, P1 = Pos + Size, T0 = UV0, T1 = UV1;
	if (GClip.bIsValid)
	{
		const FVector2f C0(FMath::Max(P0.X, GClip.Min.X), FMath::Max(P0.Y, GClip.Min.Y));
		const FVector2f C1(FMath::Min(P1.X, GClip.Max.X), FMath::Min(P1.Y, GClip.Max.Y));
		if (C1.X <= C0.X || C1.Y <= C0.Y)
		{
			return;
		}
		const FVector2f D = T1 - T0;
		T0 = UV0 + D * (C0 - Pos) / Size;
		T1 = UV0 + D * (C1 - Pos) / Size;
		P0 = C0;
		P1 = C1;
	}
	FCanvasTileItem Item(FVector2D(P0), Tex, FVector2D(P1 - P0), FVector2D(T0), FVector2D(T1), Color);
	Item.BlendMode = GBlend;
	Canvas->DrawItem(Item);
}

void FNWUILayout::DrawImage(UCanvas* Canvas, FNWUIElement& E, const FBox2f& Rect, float Alpha, float UIScale, const FBox2f& Clip)
{
	GClip = Clip;
	// LyShine additive = (SrcAlpha, One); Unreal's is (One, One) and lights the RGB where alpha is 0. In faithful mode it goes
	// as translucent, which for low-alpha glows stays close to the original.
	GBlend = E.Blend == 1 && !bExactText ? SE_BLEND_Additive : SE_BLEND_Translucent;
	const FString& TexPath = E.TextureOverride.IsEmpty() ? E.Texture : E.TextureOverride;
	if (E.CachedFor != TexPath)
	{
		E.CachedTexture = (!TexPath.IsEmpty() && ResolveTexture) ? ResolveTexture(TexPath) : nullptr;
		E.CachedFor = TexPath;
	}
	const FTexture* Tex = E.CachedTexture && E.CachedTexture->GetResource() ? E.CachedTexture->GetResource() : GWhiteTexture;
	FLinearColor Color = E.Tint.Get(E.Color);
	Color.A *= Alpha;
	if (Color.A <= 0.005f)
	{
		return;
	}

	FBox2f R = Rect;
	if (bExactText && E.ImageType == 4 && E.CachedTexture && E.CachedTexture->GetSizeY() > 0)
	{
		// StretchedToFit: keeps the texture's aspect ratio, centered in the rectangle.
		const float TA = float(E.CachedTexture->GetSizeX()) / float(E.CachedTexture->GetSizeY());
		const FVector2f S = Rect.GetSize(), C = Rect.GetCenter();
		const FVector2f Fit = S.X / FMath::Max(1.f, S.Y) > TA ? FVector2f(S.Y * TA, S.Y) : FVector2f(S.X, S.X / TA);
		R = FBox2f(C - Fit * 0.5f, C + Fit * 0.5f);
	}
	FVector2f UV0(0.f, 0.f), UV1(1.f, 1.f);
	const float Amount = FMath::Clamp(E.Fill.Get(E.FillAmount), 0.f, 1.f);
	if (E.FillType == 1 && Amount < 1.f)
	{
		// Linear fill: cuts the rectangle and the UVs from the origin edge.
		const FVector2f S = R.GetSize();
		switch (E.FillEdgeOrigin)
		{
		case 1: R.Max.Y = R.Min.Y + S.Y * Amount; UV1.Y = Amount; break;
		case 2: R.Min.X = R.Max.X - S.X * Amount; UV0.X = 1.f - Amount; break;
		case 3: R.Min.Y = R.Max.Y - S.Y * Amount; UV0.Y = 1.f - Amount; break;
		default: R.Max.X = R.Min.X + S.X * Amount; UV1.X = Amount; break;
		}
	}
	if (E.FillType == 2 && Amount < 1.f)
	{
		if (Amount <= 0.f)
		{
			return;
		}
		// Radial: fan from the center; 0° = top, clockwise.
		const FVector2f C = R.GetCenter();
		const FVector2f H = R.GetExtent();
		auto Edge = [&](float Deg)
		{
			const float Rad = FMath::DegreesToRadians(Deg);
			const FVector2f D(FMath::Sin(Rad), -FMath::Cos(Rad));
			const float T = FMath::Min(FMath::Abs(D.X) > 1e-4f ? H.X / FMath::Abs(D.X) : 1e9f, FMath::Abs(D.Y) > 1e-4f ? H.Y / FMath::Abs(D.Y) : 1e9f);
			return C + D * T;
		};
		auto UV = [&](const FVector2f& P) { return FVector2D((P.X - R.Min.X) / R.GetSize().X, (P.Y - R.Min.Y) / R.GetSize().Y); };
		TArray<FCanvasUVTri> Tris;
		const int32 Steps = FMath::Max(2, FMath::CeilToInt(48.f * Amount));
		const float Sign = E.bFillClockwise ? 1.f : -1.f;
		for (int32 k = 0; k < Steps; ++k)
		{
			const FVector2f A = Edge(E.FillStartAngle + Sign * 360.f * Amount * k / Steps);
			const FVector2f B = Edge(E.FillStartAngle + Sign * 360.f * Amount * (k + 1) / Steps);
			FCanvasUVTri& T = Tris.AddDefaulted_GetRef();
			T.V0_Pos = FVector2D(C); T.V0_UV = UV(C); T.V0_Color = Color;
			T.V1_Pos = FVector2D(A); T.V1_UV = UV(A); T.V1_Color = Color;
			T.V2_Pos = FVector2D(B); T.V2_UV = UV(B); T.V2_Color = Color;
		}
		FCanvasTriangleItem Item(Tris, Tex);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
		return;
	}

	if (E.ImageType == 1 && E.bHasBorders && E.CachedTexture)
	{
		// 9-slice: corners at the texture size (scaled with the screen), edges and center stretched.
		const FVector2f TexSize(E.CachedTexture->GetSizeX(), E.CachedTexture->GetSizeY());
		const float U[4] = {0.f, E.Borders.X, E.Borders.Y, 1.f};
		const float V[4] = {0.f, E.Borders.Z, E.Borders.W, 1.f};
		const FVector2f S = R.GetSize();
		float L = U[1] * TexSize.X * UIScale, Rw = (1.f - U[2]) * TexSize.X * UIScale;
		float T = V[1] * TexSize.Y * UIScale, B = (1.f - V[2]) * TexSize.Y * UIScale;
		if (L + Rw > S.X) { const float K = S.X / (L + Rw); L *= K; Rw *= K; }
		if (T + B > S.Y) { const float K = S.Y / (T + B); T *= K; B *= K; }
		const float X[4] = {R.Min.X, R.Min.X + L, R.Max.X - Rw, R.Max.X};
		const float Y[4] = {R.Min.Y, R.Min.Y + T, R.Max.Y - B, R.Max.Y};
		for (int32 i = 0; i < 3; ++i)
		{
			for (int32 j = 0; j < 3; ++j)
			{
				Tile(Canvas, Tex, FVector2f(X[i], Y[j]), FVector2f(X[i + 1] - X[i], Y[j + 1] - Y[j]),
					FVector2f(U[i], V[j]), FVector2f(U[i + 1], V[j + 1]), Color);
			}
		}
		return;
	}
	Tile(Canvas, Tex, R.Min, R.GetSize(), UV0, UV1, Color);
}

UFont* FNWUILayout::TextFont(const FNWUIElement& E) const
{
	int32 Style = E.FontStyle;
	if (bExactText)
	{
		// Canvas family: Nimbus italic and medium have their own file (GameFont 6 and 7).
		const FString& F = E.FontFile;
		Style = F.Contains(TEXT("pica_italic")) ? 4 : F.Contains(TEXT("pica")) ? 3 : F.Contains(TEXT("caslon")) ? 5
			: F.Contains(TEXT("italic")) ? 6 : F.Contains(TEXT("semibold")) ? 1 : F.Contains(TEXT("bold")) ? 2
			: F.Contains(TEXT("medium")) ? 7 : 0;
	}
	return !bExactText && Style <= 2 ? Fonts[FMath::Clamp(Style, 0, 2)] : GameFont(Style);
}

float FNWUILayout::TextScale(UCanvas* Canvas, const FNWUIElement& E, UFont* Font, float UIScale) const
{
	// LyShine size = line height (ascender + descender) in reference-screen pixels. In faithful mode, the
	// scale comes from the font's own line height (measured at 1); otherwise, from the nominal size (24).
	if (bExactText)
	{
		float W1 = 0.f, H1 = 0.f;
		Canvas->TextSize(Font, TEXT("Ag"), W1, H1, 1.f, 1.f);
		return E.FontSize * UIScale / FMath::Max(1.f, H1);
	}
	return E.FontSize * UIScale / FMath::Max(1, Font->LegacyFontSize);
}

// LyShine caseoverride. FString::ToUpper only changes ASCII ("FUNDIçãO"); FText uses the language rules.
static FString NWUICaseOverride(const FString& Raw, int32 Case)
{
	return Case == 2 ? FText::FromString(Raw).ToUpper().ToString() : Case == 1 ? FText::FromString(Raw).ToLower().ToString() : Raw;
}

float FNWUILayout::TextWidth(UCanvas* Canvas, const FNWUIElement& E, float UIScale) const
{
	const FString& Raw = E.TextOverride.IsSet() ? E.TextOverride.GetValue() : E.Text;
	UFont* Font = TextFont(E);
	if (Raw.IsEmpty() || !Font)
	{
		return 0.f;
	}
	const FString Str = NWUICaseOverride(Raw, E.CaseOverride);
	const float S = TextScale(Canvas, E, Font, UIScale);
	const float Extra = FMath::Abs(E.CharSpacing) > 1.f ? E.CharSpacing * 0.001f * E.FontSize * UIScale : 0.f;
	TArray<FString> Lines;
	Str.ParseIntoArray(Lines, TEXT("\n"), false);
	float Widest = 0.f;
	for (const FString& L : Lines)
	{
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Font, L, W, H, S, S);
		Widest = FMath::Max(Widest, W + Extra * FMath::Max(0, L.Len() - 1));
	}
	return Widest / UIScale;
}

void FNWUILayout::DrawText(UCanvas* Canvas, FNWUIElement& E, const FBox2f& Rect, float Alpha, float UIScale, const FBox2f& Clip)
{
	if (!Clip.Intersect(Rect))
	{
		return;
	}
	const FString& Raw = E.TextOverride.IsSet() ? E.TextOverride.GetValue() : E.Text;
	const FString Str = NWUICaseOverride(Raw, E.CaseOverride);
	UFont* Font = TextFont(E);
	if (Str.IsEmpty() || !Font)
	{
		return;
	}
	float S = TextScale(Canvas, E, Font, UIScale);
	const float Extra = FMath::Abs(E.CharSpacing) > 1.f ? E.CharSpacing * 0.001f * E.FontSize * UIScale : 0.f;
	auto Width = [&](const FString& Line, float Scale)
	{
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Font, Line, W, H, Scale, Scale);
		return W + (Extra != 0.f ? Extra * FMath::Max(0, Line.Len() - 1) * Scale / S : 0.f);
	};
	const float MaxW = Rect.GetSize().X;
	TArray<FString> Lines;
	if (bExactText)
	{
		Str.ParseIntoArray(Lines, TEXT("\n"), false);
	}
	else
	{
		Lines.Add(Str);
	}
	if (bExactText && E.bWrap && !E.bFitWidth && MaxW > 1.f)
	{
		// Word wrap at the element width (wraptextsetting).
		TArray<FString> Wrapped;
		for (const FString& L : Lines)
		{
			TArray<FString> Words;
			L.ParseIntoArray(Words, TEXT(" "), true);
			FString Cur;
			for (const FString& W : Words)
			{
				const FString Try = Cur.IsEmpty() ? W : Cur + TEXT(" ") + W;
				if (!Cur.IsEmpty() && Width(Try, S) > MaxW)
				{
					Wrapped.Add(Cur);
					Cur = W;
				}
				else
				{
					Cur = Try;
				}
			}
			Wrapped.Add(Cur);
		}
		Lines = MoveTemp(Wrapped);
	}
	else if (bExactText && E.ShrinkToFit != 0 && !E.bFitWidth && MaxW > 1.f)
	{
		// Shrink to fit the width (shrinktofit), down to minshrinkscale.
		float Widest = 0.f;
		for (const FString& L : Lines)
		{
			Widest = FMath::Max(Widest, Width(L, S));
		}
		if (Widest > MaxW)
		{
			S *= FMath::Max(E.MinShrink, MaxW / Widest);
		}
	}
	float LW = 0.f, LH = 0.f;
	Canvas->TextSize(Font, bExactText ? FString(TEXT("Ag")) : Str, LW, LH, S, S);
	const float Step = LH + (bExactText ? E.LineSpacing * UIScale : 0.f);
	const float BlockH = Step * (Lines.Num() - 1) + LH;
	float Y = E.VAlign == 1 ? Rect.GetCenter().Y - BlockH * 0.5f : E.VAlign == 2 ? Rect.Max.Y - BlockH : Rect.Min.Y;
	FLinearColor Color = E.Tint.Get(E.TextColor);
	Color.A *= Alpha;
	for (const FString& Line : Lines)
	{
		const float W = Width(Line, S);
		float X = E.HAlign == 1 ? Rect.GetCenter().X - W * 0.5f : E.HAlign == 2 ? Rect.Max.X - W : Rect.Min.X;
		if (Extra != 0.f)
		{
			// Letter spacing (game titles: "S M E L T E R"): letter by letter.
			for (int32 i = 0; i < Line.Len(); ++i)
			{
				const FString Ch = Line.Mid(i, 1);
				FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Ch), Font, Color);
				Item.Scale = FVector2D(S, S);
				Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.75f * Color.A));
				Canvas->DrawItem(Item);
				float CW = 0.f, CH = 0.f;
				Canvas->TextSize(Font, Ch, CW, CH, S, S);
				X += CW + Extra;
			}
		}
		else
		{
			FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Line), Font, Color);
			Item.Scale = FVector2D(S, S);
			Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.75f * Color.A));
			Canvas->DrawItem(Item);
		}
		Y += Step;
	}
}

int32 FNWUILayout::IndexOf(const FString& CanvasName, const FNWUIElement* Element) const
{
	for (const FNWUICanvasLayout& L : Canvases)
	{
		if (L.Name == CanvasName && Element >= L.Elements.GetData() && Element < L.Elements.GetData() + L.Elements.Num())
		{
			return int32(Element - L.Elements.GetData());
		}
	}
	return INDEX_NONE;
}

FBox2f FNWUILayout::DrawSubtree(UCanvas* Canvas, const FString& CanvasName, const FNWUIElement* Root, const FVector2f& Offset)
{
	FNWUICanvasLayout* LP = Canvases.FindByPredicate([&CanvasName](const FNWUICanvasLayout& X) { return X.Name == CanvasName; });
	const int32 R = IndexOf(CanvasName, Root);
	if (!LP || R == INDEX_NONE || !LP->LastRects.IsValidIndex(R))
	{
		return FBox2f(ForceInit);
	}
	FNWUICanvasLayout& L = *LP;
	const float UIScale = Canvas->ClipY / L.Size.Y;
	// End of the subtree: elements are in pre-order; the subtree goes up to the first one that does not descend from Root.
	int32 End = R;
	for (int32 i = R + 1; i < L.Elements.Num(); ++i)
	{
		int32 P = L.Elements[i].Parent;
		while (P > R)
		{
			P = L.Elements[P].Parent;
		}
		if (P != R)
		{
			break;
		}
		End = i;
	}
	const int32 Parent = L.Elements[R].Parent;
	const FBox2f ParentRect = L.LastRects.IsValidIndex(Parent)
		? FBox2f(L.LastRects[Parent].Min / UIScale, L.LastRects[Parent].Max / UIScale)
		: FBox2f(FVector2f(0.f, 0.f), FVector2f(Canvas->ClipX / UIScale, L.Size.Y));
	// Alpha inherited from the parents (Root is hidden, so the chain above it applies).
	float BaseAlpha = 1.f;
	for (int32 P = Parent; L.Elements.IsValidIndex(P); P = L.Elements[P].Parent)
	{
		BaseAlpha *= L.Elements[P].Fade * L.Elements[P].AlphaMul;
	}
	TMap<int32, FBox2f> Rects;
	TMap<int32, float> Alphas;
	TArray<TPair<int32, FBox2f>> Clips;
	const FBox2f RootClip = L.LastClips[R];
	FBox2f RowRect(ForceInit);
	for (int32 i = R; i <= End; ++i)
	{
		while (Clips.Num() > 0 && i > Clips.Last().Key)
		{
			Clips.Pop();
		}
		FNWUIElement& E = L.Elements[i];
		const bool bRoot = i == R;
		const FBox2f P = bRoot ? ParentRect : Rects.FindRef(E.Parent);
		const FVector2f PS = P.GetSize();
		const FVector2f T = E.Translate + (bRoot ? Offset : FVector2f(0.f, 0.f));
		float Left = P.Min.X + E.Anchors.X * PS.X + E.Offsets.X + T.X;
		float Top = P.Min.Y + E.Anchors.Y * PS.Y + E.Offsets.Y + T.Y;
		float Right = P.Min.X + E.Anchors.Z * PS.X + E.Offsets.Z + T.X;
		float Bottom = P.Min.Y + E.Anchors.W * PS.Y + E.Offsets.W + T.Y;
		if (bExactText && E.bFitWidth && E.bHasText)
		{
			// UiLayoutFitter: text width, around the pivot.
			const float FitW = TextWidth(Canvas, E, UIScale);
			const float PX = Left + (Right - Left) * E.Pivot.X;
			Left = PX - FitW * E.Pivot.X;
			Right = Left + FitW;
		}
		if (E.Scale.X != 1.f || E.Scale.Y != 1.f)
		{
			const float CX = Left + (Right - Left) * E.Pivot.X, CY = Top + (Bottom - Top) * E.Pivot.Y;
			Left = CX - (CX - Left) * E.Scale.X; Right = CX + (Right - CX) * E.Scale.X;
			Top = CY - (CY - Top) * E.Scale.Y; Bottom = CY + (Bottom - CY) * E.Scale.Y;
		}
		const FBox2f Rect(FVector2f(Left, Top), FVector2f(Right, Bottom));
		Rects.Add(i, Rect);
		const FBox2f Px(Rect.Min * UIScale, Rect.Max * UIScale);
		if (bRoot)
		{
			RowRect = Px;
		}
		// Root draws even when hidden in the layout (it is the template item); the children follow their own state.
		const bool bShown = bRoot ? true : E.Visible.Get(E.bEnabled);
		if (!bShown)
		{
			// Hidden child: its subtree disappears too (marks the end as an empty clip).
			int32 SubEnd = i;
			for (int32 j = i + 1; j <= End; ++j)
			{
				int32 PP = L.Elements[j].Parent;
				while (PP > i)
				{
					PP = L.Elements[PP].Parent;
				}
				if (PP != i)
				{
					break;
				}
				SubEnd = j;
			}
			Clips.Add({SubEnd, FBox2f(FVector2f(0.f, 0.f), FVector2f(0.f, 0.f))});
			continue;
		}
		const float A = (bRoot ? BaseAlpha : Alphas.FindRef(E.Parent)) * E.Fade * E.AlphaMul;
		Alphas.Add(i, A);
		FBox2f Clip = Clips.Num() > 0 ? Clips.Last().Value : RootClip;
		if (E.bMask)
		{
			FBox2f Inner(FVector2f(FMath::Max(Px.Min.X, Clip.Min.X), FMath::Max(Px.Min.Y, Clip.Min.Y)),
				FVector2f(FMath::Min(Px.Max.X, Clip.Max.X), FMath::Min(Px.Max.Y, Clip.Max.Y)));
			Clips.Add({E.EndIndex >= i ? E.EndIndex : i, Inner});
			if (!E.bMaskDraw)
			{
				continue;
			}
		}
		if (A <= 0.005f || Clip.Max.X <= Clip.Min.X || Clip.Max.Y <= Clip.Min.Y)
		{
			continue;
		}
		if (E.bHasImage)
		{
			DrawImage(Canvas, E, Px, A, UIScale, Clip);
		}
		if (E.bHasText)
		{
			DrawText(Canvas, E, Px, A, UIScale, Clip);
		}
	}
	return RowRect;
}

void FNWUILayout::Dump(const FString& Filter) const
{
	for (const FNWUICanvasLayout& C : Canvases)
	{
		for (const FNWUIElement& El : C.Elements)
		{
			if (!El.Path.Contains(Filter))
			{
				continue;
			}
			const FBox2f R = RectOf(&El);
			UE_LOG(LogTemp, Display, TEXT("NWUIDump %s shown=%d rect=(%.0f,%.0f)-(%.0f,%.0f) fade=%.2f text='%s' tex=%s"), *El.Path,
				WasShown(&El) ? 1 : 0, R.Min.X, R.Min.Y, R.Max.X, R.Max.Y, El.Fade * El.AlphaMul,
				El.TextOverride.IsSet() ? *El.TextOverride.GetValue() : *El.Text,
				El.TextureOverride.IsEmpty() ? *El.Texture : *El.TextureOverride);
		}
	}
}

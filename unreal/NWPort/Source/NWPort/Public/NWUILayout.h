#pragma once

#include "CoreMinimal.h"

class UCanvas;
class UFont;
class UTexture2D;

// Um elemento de tela LyShine (layout gerado por tools/ui_build_hud.py a partir das telas do jogo).
struct FNWUIElement
{
	FString Name;
	FString Path;
	int32 Parent = -1;
	FVector4f Anchors = FVector4f(0, 0, 1, 1);   // esquerda, cima, direita, baixo (fração do pai)
	FVector4f Offsets = FVector4f(0, 0, 0, 0);   // em unidades da tela de referência
	FVector2f Pivot = FVector2f(0.5f, 0.5f);
	FVector2f Scale = FVector2f(1.f, 1.f);
	float Fade = 1.f;
	bool bEnabled = true;
	// Máscara (UiMaskComponent): recorta a subárvore (até EndIndex); a imagem dela só aparece com bMaskDraw.
	bool bMask = false;
	bool bMaskDraw = false;
	int32 EndIndex = -1;

	bool bHasImage = false;
	FString Texture;              // caminho do jogo (lyshineui/images/...dds)
	FLinearColor Color = FLinearColor::White;
	uint8 ImageType = 0;          // 0 esticada, 1 9-slice, ...
	uint8 Blend = 0;              // 0 normal, 1 aditiva
	uint8 FillType = 0;           // 0 nenhum, 1 linear, 2 radial
	float FillAmount = 1.f;
	float FillStartAngle = 0.f;
	bool bFillClockwise = true;
	int32 FillEdgeOrigin = 0;     // 0 esquerda, 1 cima, 2 direita, 3 baixo
	bool bHasBorders = false;
	FVector4f Borders = FVector4f(0, 1, 0, 1);   // linhas do 9-slice: esquerda, direita, cima, baixo (frações)

	bool bHasText = false;
	FString Text;
	int32 FontStyle = 0;          // 0 regular, 1 semibold, 2 bold
	float FontSize = 16.f;
	FLinearColor TextColor = FLinearColor::White;
	int32 HAlign = 0;             // 0 esquerda, 1 centro, 2 direita
	int32 VAlign = 0;

	// Estado de jogo (definido pela HUD; vazio = valor do layout).
	TOptional<bool> Visible;
	TOptional<float> Fill;
	TOptional<FLinearColor> Tint;
	TOptional<FString> TextOverride;
	FString TextureOverride;
	float AlphaMul = 1.f;
	// Deslocamento em unidades da tela de referência (ex.: nós da árvore espalhados pela coluna).
	FVector2f Translate = FVector2f(0.f, 0.f);

	UTexture2D* CachedTexture = nullptr;   // mantido vivo pela HUD
	FString CachedFor;
};

struct FNWUICanvasLayout
{
	FString Name;
	FVector2f Size = FVector2f(1920.f, 1080.f);
	TArray<FNWUIElement> Elements;
	// Último desenho (pixels da tela): retângulo, recorte e visibilidade de cada elemento, para o clique.
	TArray<FBox2f> LastRects;
	TArray<FBox2f> LastClips;
	TArray<bool> LastShown;
};

/**
 * Desenha telas do jogo no Canvas do Unreal com as regras de layout do LyShine: retângulo de cada elemento
 * = retângulo do pai + âncoras + deslocamentos, na tela de referência (1920x1080) escalada pela altura.
 */
class NWPORT_API FNWUILayout
{
public:
	bool Load(const FString& JsonFile);
	bool IsLoaded() const { return Canvases.Num() > 0; }

	// Elementos cujo caminho termina com o sufixo (ex.: "WeaponAbilitySet/WeaponAbility2/AbilityIcon").
	TArray<FNWUIElement*> Find(const FString& Canvas, const FString& PathSuffix);
	FNWUIElement* FindOne(const FString& Canvas, const FString& PathSuffix);

	// Resolve textura do jogo -> asset importado (/Game/NW/UI/...); a HUD guarda a referência.
	TFunction<UTexture2D*(const FString&)> ResolveTexture;
	UFont* Fonts[3] = {nullptr, nullptr, nullptr};

	void Draw(UCanvas* Canvas);
	// Só um canvas (ex.: o mesmo campo de texto desenhado em várias posições com Translate).
	void DrawOne(UCanvas* Canvas, const FString& CanvasName);
	// Elemento visível mais à frente sob o ponto (pixels), no último desenho; Accept filtra candidatos.
	int32 HitTest(const FString& Canvas, const FVector2D& Point, TFunctionRef<bool(const FNWUIElement&)> Accept) const;
	const FNWUIElement* Element(const FString& Canvas, int32 Index) const;
	FBox2f LastRect(const FString& Canvas, int32 Index) const;
	// Retângulo (pixels) do elemento no último desenho; inválido se não desenhado.
	FBox2f RectOf(const FNWUIElement* Element) const;
	// Elemento desenhado (visível, com toda a cadeia de pais) no último desenho.
	bool WasShown(const FNWUIElement* Element) const;

	static FString AssetPathFor(const FString& GameTexture);
	// Fonte do jogo (Nimbus Sans Novus, .otf em Data/UI/Fonts) criada uma vez e mantida viva: 0 regular, 1 semibold, 2 bold.
	static UFont* GameFont(int32 Style);
	// Textura importada da UI do jogo (mantida viva).
	static UTexture2D* GameTexture(const FString& GameTexturePath);

private:
	void DrawCanvas(UCanvas* Canvas, FNWUICanvasLayout& Layout);
	void DrawImage(UCanvas* Canvas, FNWUIElement& E, const FBox2f& Rect, float Alpha, float UIScale, const FBox2f& Clip);
	void DrawText(UCanvas* Canvas, FNWUIElement& E, const FBox2f& Rect, float Alpha, float UIScale, const FBox2f& Clip);

	TArray<FNWUICanvasLayout> Canvases;
};

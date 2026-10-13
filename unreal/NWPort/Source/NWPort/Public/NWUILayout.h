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
	int32 FontStyle = 0;          // 0 regular, 1 semibold, 2 bold, 3 Pica, 4 Pica itálico, 5 Caslon (GameFont)
	float CharSpacing = 0.f;      // characterspacing do LyShine (milésimos do tamanho da fonte)
	int32 CaseOverride = 0;       // caseoverride do LyShine: 0 nenhum, 1 minúsculas, 2 maiúsculas
	FString FontFile;             // fonte do canvas (lyshineui/fonts/<nome>.font), para o modo bExactText
	bool bWrap = false;           // wraptextsetting
	float LineSpacing = 0.f;      // linespacing (pixels da tela de referência)
	int32 ShrinkToFit = 0;        // 0 não, 1 uniforme, 2 só largura
	float MinShrink = 0.f;
	bool bFitWidth = false;       // UiLayoutFitter horizontal: largura = a do texto (modo bExactText)
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
	/** Texto fiel ao LyShine (telas da esteira 4): tamanho = altura da linha em pixels (medida na própria fonte), família exata (Nimbus médio e itálico), quebra de linha, espaçamento entre linhas e encolher para caber.
	 *  Desligado, o texto sai ~1,56× maior e numa linha só (telas antigas foram ajustadas assim). */
	bool bExactText = false;
	UFont* Fonts[3] = {nullptr, nullptr, nullptr};

	void Draw(UCanvas* Canvas);
	// Só um canvas (ex.: o mesmo campo de texto desenhado em várias posições com Translate).
	void DrawOne(UCanvas* Canvas, const FString& CanvasName);
	/**
	 * Lista: redesenha a subárvore do elemento Root (item-modelo do jogo, escondido no desenho normal) deslocada por
	 * Offset (unidades da tela de referência), dentro do pai e do recorte do último Draw. Antes de chamar, a tela
	 * preenche os elementos da subárvore (TextOverride, Visible...). Devolve o retângulo da linha em pixels.
	 */
	FBox2f DrawSubtree(UCanvas* Canvas, const FString& CanvasName, const FNWUIElement* Root, const FVector2f& Offset);
	/** Índice do elemento no canvas (para DrawSubtree e filhos) ou INDEX_NONE. */
	int32 IndexOf(const FString& CanvasName, const FNWUIElement* Element) const;
	// Elemento visível mais à frente sob o ponto (pixels), no último desenho; Accept filtra candidatos.
	int32 HitTest(const FString& Canvas, const FVector2D& Point, TFunctionRef<bool(const FNWUIElement&)> Accept) const;
	const FNWUIElement* Element(const FString& Canvas, int32 Index) const;
	FBox2f LastRect(const FString& Canvas, int32 Index) const;
	// Retângulo (pixels) do elemento no último desenho; inválido se não desenhado.
	FBox2f RectOf(const FNWUIElement* Element) const;
	// Elemento desenhado (visível, com toda a cadeia de pais) no último desenho.
	bool WasShown(const FNWUIElement* Element) const;

	static FString AssetPathFor(const FString& GameTexture);
	// Fonte do jogo (.otf/.ttf em Data/UI/Fonts) criada uma vez e mantida viva: 0 Nimbus regular, 1 semibold, 2 bold,
	// 3 Pica (IM Fell, títulos), 4 Pica itálico, 5 Caslon, 6 Nimbus itálico, 7 Nimbus médio.
	static UFont* GameFont(int32 Style);
	/** Log dos elementos cujo caminho contém Filter: retângulo e estado no último desenho (captura offline). */
	void Dump(const FString& Filter) const;
	/** Largura do texto na tela de referência (linha mais larga), para o UiLayoutFitter e layouts feitos no C++. */
	float TextWidth(UCanvas* Canvas, const FNWUIElement& E, float UIScale) const;
	// Textura importada da UI do jogo (mantida viva).
	static UTexture2D* GameTexture(const FString& GameTexturePath);

private:
	void DrawCanvas(UCanvas* Canvas, FNWUICanvasLayout& Layout);
	void DrawImage(UCanvas* Canvas, FNWUIElement& E, const FBox2f& Rect, float Alpha, float UIScale, const FBox2f& Clip);
	void DrawText(UCanvas* Canvas, FNWUIElement& E, const FBox2f& Rect, float Alpha, float UIScale, const FBox2f& Clip);
	UFont* TextFont(const FNWUIElement& E) const;
	float TextScale(UCanvas* Canvas, const FNWUIElement& E, UFont* Font, float UIScale) const;

	TArray<FNWUICanvasLayout> Canvases;
};

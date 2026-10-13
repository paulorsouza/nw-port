#pragma once

#include "CoreMinimal.h"
#include "NWUILayout.h"

class ANWCharacter;
class UCanvas;
struct FNWShop;

/**
 * Tela da loja (esteira 4) com o layout do jogo: lyshineui/shop/shop (tools/ui_screens/shop.json), a barra de título de
 * stationproperties (screen_header.json) e o rodapé (helpbar.json). Lista em grade (shopgriditem.lua) e detalhe do
 * item com quantidade e compra (shopitemdetailpanel.lua). Preço e limite semanal: tools/shops.json (NWShop.h).
 */
class NWPORT_API FNWShopMenu
{
public:
	bool IsOpen() const { return Character.IsValid() && !ShopId.IsEmpty(); }
	void Open(ANWCharacter* InCharacter, const FString& InShopId, const FString& InTitle);
	void Close() { Character.Reset(); ShopId.Reset(); }
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	bool Click(const FVector2D& Mouse);
	/** A cada quadro com o botão esquerdo: arrastar o marcador da quantidade. */
	void Hold(const FVector2D& Mouse, bool bDown);
	/** Escolhe o item (captura offline); NAME_None volta à grade. */
	void Select(FName Item, int32 InCount);
	void Dump(const FString& Filter) const;
	/** Relógio fixo para a captura offline (NWUIShotCommandlet); 0 = relógio real. */
	double FixedNow = 0.0;

private:
	bool Load();
	FNWUILayout Ui, Head, Help;
	bool bLoaded = false;
	TWeakObjectPtr<ANWCharacter> Character;
	FString ShopId;
	FString Title;
	/** Item no detalhe; INDEX_NONE = grade. */
	int32 Detail = INDEX_NONE;
	int32 Count = 1;
	bool bDragging = false;
	TArray<TPair<FBox2f, int32>> CellRects;

	const FNWShop* Shop() const;
	FNWUIElement* E(const TCHAR* Suffix) { return Ui.FindOne(TEXT("shop"), Suffix); }
	/** Quanto ainda dá para comprar do item: limite da semana, moedas e peso. */
	int32 MaxCount(int32 Index, int32* OutLimitLeft = nullptr) const;
	void DrawGrid(UCanvas* Canvas, const FVector2D& Mouse);
	void FillDetail();
	void DrawHelp(UCanvas* Canvas);
	void SliderTo(float MouseX);
	double Now() const { return FixedNow > 0.0 ? FixedNow : FPlatformTime::Seconds(); }
	/** "3d 4h" / "23h 59m" até a renovação semanal (CooldownTextUpdater FORMAT_SHORTHAND, sem zeros). */
	static FString TimeToReset();
};

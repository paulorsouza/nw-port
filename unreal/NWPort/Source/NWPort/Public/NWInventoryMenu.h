#pragma once

#include "CoreMinimal.h"
#include "NWItems.h"
#include "NWUILayout.h"

class ANWCharacter;
class UCanvas;

/**
 * Inventário (Tab) no estilo do jogo. À direita, a janela de inventário do jogo (Data/UI/inventory_layout.json,
 * tools/ui_build_inventory.py) com os itens em retângulos de 122×60 (fundo itembglarge + faixa de raridade
 * itemraritybglarge + ícone, tier em romano, gear score e quantidade), agrupados por categoria, e a barra de peso.
 * À esquerda, o painel de equipamento com as abas do jogo (armas e armadura / ferramentas) e espaços quadrados
 * (framesquare + ícone do tipo quando vazio). O meio fica livre para ver o personagem.
 * Clique esquerdo escolhe um item e depois o destino (espaço de equipamento ou a bolsa); direito usa a poção ou
 * equipa/desequipa. As mudanças vão ao servidor (UNWInventoryComponent).
 */
class NWPORT_API FNWInventoryMenu
{
public:
	bool IsOpen() const { return Character.IsValid(); }
	void Open(ANWCharacter* InCharacter);
	void Close() { Character.Reset(); bPicked = false; }
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	void Click(const FVector2D& Mouse, bool bRight);

private:
	struct FSlotRef
	{
		ENWItemContainer Container = ENWItemContainer::Bag;
		int32 Slot = INDEX_NONE;
		FBox2f Rect = FBox2f(ForceInit);
	};
	struct FHeader
	{
		FString Text;
		FVector2f Pos;
	};

	bool Load();
	void Layout(UCanvas* Canvas);
	const FSlotRef* SlotAt(const FVector2D& Mouse) const;
	void DrawItemTile(UCanvas* Canvas, const FBox2f& R, const FNWItem* Item, bool bSquare, bool bHover, bool bPickedHere);
	void DrawEquipPanel(UCanvas* Canvas, const FVector2D& Mouse);
	void DrawTooltip(UCanvas* Canvas, const FNWItem& Item, const FVector2D& Mouse);
	void DrawText(UCanvas* Canvas, const FString& Text, float X, float Y, float Size, const FLinearColor& Color, int32 Style = 0) const;
	void DrawTexture(UCanvas* Canvas, const FString& Path, const FBox2f& R, const FLinearColor& Tint = FLinearColor::White, bool bAdditive = false) const;
	float TextWidth(const FString& Text, float Size, int32 Style = 0) const;

	FNWUILayout Window;
	bool bLoaded = false;
	TWeakObjectPtr<ANWCharacter> Character;
	TArray<FSlotRef> Slots;
	TArray<FHeader> Headers;
	FBox2f BagArea = FBox2f(ForceInit);
	FBox2f EquipPanel = FBox2f(ForceInit);
	FBox2f TabRects[2];
	int32 EquipTab = 0;   // 0 armas e armadura, 1 ferramentas
	bool bPicked = false;
	ENWItemContainer PickedContainer = ENWItemContainer::Bag;
	int32 PickedSlot = INDEX_NONE;
	float UIScale = 1.f;
};

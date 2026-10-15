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
 * Inventário (Tab), equipamento e armazém no estilo do jogo (spec: docs/game/ui/inventory.md).
 * - Janela do inventário: moldura do canvas inventory/newinventory (Data/UI/inventory_layout.json), lista com as seções
 *   do jogo (dynamicitemlist.lua: UiItemClass do item) em duas colunas, peças 60x60 quadradas ou redondas
 *   (itemlayout.lua), ordenação por chegada/peso/gear score/tier, peso atual/máximo.
 * - Equipamento sem painel, nas posições do equipmentv3 (espaços 63x63, fundos pretos por grupo, abas em ícone, nível
 *   e gear score, atributos).
 * - Armazém (catcontainer.lua, modo armazém): segunda janela igual à direita; o inventário e o equipamento vão 300 px
 *   para a esquerda. Modo bolsa de saque (CONTAINER_MODE_LOOT_BAG): a mesma janela com os itens da bolsa no chão
 *   (ANWLootBag), aviso de tempo de vida e contagem no lugar da busca; só pegar, nunca guardar.
 * - Arrastar (itemdraggable.lua): cópia 75x75 no cursor, origem a 0,5; espaços compatíveis acendem; a janela de destino
 *   acende com o mouse em cima. Shift + clique guarda/retira; direito equipa, usa ou move.
 */
class NWPORT_API FNWInventoryMenu
{
public:
	bool IsOpen() const { return Character.IsValid(); }
	void Open(ANWCharacter* InCharacter);
	/** Inventário com o armazém da cidade (o servidor já abriu o local). */
	void OpenStorage(ANWCharacter* InCharacter, const FString& InTitle);
	bool IsStorageMode() const { return IsOpen() && bStorage; }
	/** Inventário com a bolsa no chão (catcontainer no modo bolsa de saque). */
	void OpenLootBag(ANWCharacter* InCharacter, ANWLootBag* Bag);
	void Close();
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	/** Apertar do botão (o soltar chega por Hold). */
	void Click(const FVector2D& Mouse, bool bRight);
	/** A cada quadro: estado do botão esquerdo; arrasta e solta. */
	void Hold(const FVector2D& Mouse, bool bDown);
	void Wheel(float Delta, const FVector2D& Mouse);
	/** Captura offline: começa um arrasto da bolsa (0), equipamento (1) ou armazém (2). */
	void ShotDrag(int32 Source, int32 Slot);
	/** Captura offline: ordenação da lista (0 inventário, 1 armazém): 1 chegada, 2 peso, 3 gear score, 4 tier. */
	void ShotSort(int32 List, int32 Sort) { Lists[FMath::Clamp(List, 0, 1)].Sort = Sort; }
	/** Captura offline: abre o popup de dividir (1) ou de destruir (2) para o item da bolsa (0) ou do armazém (2). */
	void ShotPopup(int32 Kind, int32 Source, int32 Slot, int32 Qty = 0);
	/** Captura offline: texto da busca da lista (0 inventário, 1 armazém). */
	void ShotSearch(int32 List, const FString& Text) { Lists[FMath::Clamp(List, 0, 1)].Search = Text; }
	/** Campo de busca da lista em pixels (o campo de texto do Slate fica por cima). */
	FBox2f SearchRect(int32 List) const { return Lists[FMath::Clamp(List, 0, 1)].SearchBox; }
	void SetSearch(int32 List, const FString& Text) { Lists[FMath::Clamp(List, 0, 1)].Search = Text; }
	/** Aba do armazém sendo renomeada (clique duplo): retângulo do campo de texto; inválido = nenhuma. */
	FBox2f RenameRect() const;
	/** Enter ou perder o foco no campo do nome da aba. */
	void CommitRename(const FString& Name);
	/** Captura offline: aba aberta do armazém. */
	void ShotTab(int32 Tab) { StorageTab = Tab; }

private:
	enum class ESource : uint8 { Bag, Equip, Storage };
	struct FSlotRef
	{
		ENWItemContainer Container = ENWItemContainer::Bag;
		int32 Slot = INDEX_NONE;
		FBox2f Rect = FBox2f(ForceInit);   // área de clique (recortada pela rolagem)
		FBox2f Full = FBox2f(ForceInit);   // peça inteira (desenho)
		bool bStorage = false;
		ESource Source() const { return bStorage ? ESource::Storage : Container == ENWItemContainer::Equip ? ESource::Equip : ESource::Bag; }
	};
	// Uma lista de itens (inventário ou armazém) com seções, ordenação e rolagem próprias.
	struct FSectionHit
	{
		FName Class;
		FBox2f Arrow = FBox2f(ForceInit);
		FBox2f Transfer = FBox2f(ForceInit);
	};
	struct FList
	{
		FBox2f Window = FBox2f(ForceInit);   // moldura inteira (alvo de soltar)
		FBox2f Area = FBox2f(ForceInit);     // lista com rolagem
		float Scroll = 0.f;                  // px de 1080p
		float ContentH = 0.f;
		int32 Sort = 1;                      // inventoryfilter.lua: 1 chegada, 2 peso, 3 gear score, 4 tier
		int32 SortDir = 1;                   // 1 padrão (maior/mais novo primeiro), −1 invertido
		TSet<FName> Collapsed;
		FString Search;                      // busca por nome (sem maiúsculas nem acentos)
		FBox2f SearchBox = FBox2f(ForceInit);
		FBox2f SortRects[4];
		TArray<FSectionHit> Sections;
	};

	// Popup do jogo (popup.lua): dividir pilha (Ctrl + clique), confirmar destruir (soltar no chão), acoplar gema (soltar
	// a gema no item; ui_attach_gem / ui_replace_gem) e retirar gema (Shift + clique direito no item; docs/game/items.md).
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
	/** Zona de largar no chão (grounddroptarget): à direita do inventário, só sem armazém. */
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
	/** Peça do jogo (itemlayout.lua): fundo e moldura por raridade, ícone − 6 px, tier, quantidade > 1, valor da ordenação. */
	void DrawItemTile(UCanvas* Canvas, const FBox2f& R, const FNWItem* Item, bool bRound, float Alpha, bool bHover, int32 SortShown);
	void DrawWindow(UCanvas* Canvas, const FList& L, float OffsetX, bool bStorageWindow, const FVector2D& Mouse);
	void DrawList(UCanvas* Canvas, const FList& L, bool bStorageItems, const FVector2D& Mouse, const FSlotRef* Hover);
	void DrawEquipment(UCanvas* Canvas, const FVector2D& Mouse, const FSlotRef* Hover);
	void DrawDrag(UCanvas* Canvas, const FVector2D& Mouse);
	/** Desenha o tooltip ao lado do mouse (ou com a borda esquerda em ForceX) e devolve o retângulo usado. */
	FBox2f DrawTooltip(UCanvas* Canvas, const FNWItem& Item, const FVector2D& Mouse, bool bEquipped, float ForceX = -1.f);
	TArray<FString> Wrap(const FString& Text, float Width, float Size, int32 Style) const;
	void DrawText(UCanvas* Canvas, const FString& Text, float X, float Y, float Size, const FLinearColor& Color, int32 Style = 0) const;
	/** Texto na fonte do jogo (lyshineui/fonts/<arquivo>.font, como no FNWUILayout), tamanho em px de 1080p. */
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
	/** Itens de uma seção (para mover a seção inteira). */
	TArray<int32> SectionSlots(FName Class, bool bStorageItems) const;

	FNWUILayout Window;
	bool bLoaded = false;
	TWeakObjectPtr<ANWCharacter> Character;
	float UIScale = 1.f;
	UCanvas* DrawCanvas = nullptr;              // canvas do quadro atual (medir texto fora do Draw)
	mutable float LineH[8] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};   // altura da linha de cada fonte a 1
	FVector2f Center = FVector2f::ZeroVector;   // centro da tela em px (as telas do jogo se posicionam a partir dele)
	TArray<FSlotRef> Slots;
	FList Lists[2];                             // 0 inventário, 1 armazém
	float InvOffsetX = 0.f;                     // 0, ou −300 com o armazém (newinventory.lua OnTransitionIn)
	FVector2f EquipOrigin = FVector2f::ZeroVector;
	FBox2f EquipArea = FBox2f(ForceInit);       // alvo universal do equipamento (equipmentuniversaldroptarget)
	FBox2f TabRects[2];
	FBox2f PointRects[5] = {FBox2f(ForceInit), FBox2f(ForceInit), FBox2f(ForceInit), FBox2f(ForceInit), FBox2f(ForceInit)};
	FBox2f TakeAllRect = FBox2f(ForceInit);
	int32 EquipTab = 0;                         // 0 armas e armadura, 1 ferramentas

	// Arrasto: apertou num item (bPressed), passou de DragStart px (bDragging).
	bool bPressed = false;
	bool bDragging = false;
	FSlotRef Pressed;
	FVector2D PressPos = FVector2D::ZeroVector;
	double HoverSince = 0.0;
	const void* HoverKey = nullptr;

	bool bStorage = false;
	FString StorageTitle;
	// Abas do armazém compartilhado (FNWItemRules::StorageTabs): a lista mostra só a aba aberta; guardar vai para ela;
	// soltar um item numa aba passa para lá; clique duplo renomeia.
	static constexpr int32 MaxTabs = 5;
	int32 StorageTab = 0;
	int32 RenameTab = INDEX_NONE;
	FBox2f TabRectsStorage[MaxTabs];
	double LastTabClick = 0.0;
	int32 LastTabClicked = INDEX_NONE;
	TSharedPtr<SEditableText> RenameEdit;
	bool ShowsTabs() const { return bStorage && !bLoot; }
	/** Aba sob o mouse (INDEX_NONE fora). */
	int32 TabAt(const FVector2f& M) const;
	/** A pilha entra na lista da segunda janela (aba aberta do armazém; a bolsa no chão mostra tudo). */
	bool InList(const FNWItem& I, bool bStorageItems) const { return !bStorageItems || !ShowsTabs() || I.Tab == StorageTab; }
	void DrawTabs(UCanvas* Canvas, const FVector2D& Mouse);
	// Modo bolsa de saque: a segunda janela mostra LootBag em vez do armazém.
	bool bLoot = false;
	TWeakObjectPtr<ANWLootBag> LootBag;
	/** Itens da segunda janela (armazém ou bolsa no chão). */
	const TArray<FNWItem>* ContainerItems() const;
	/** Move entre a bolsa e a segunda janela; na bolsa no chão só pega (bDeposit false). */
	void ContainerMove(bool bDeposit, int32 Slot, int32 Quantity);
	void ContainerMoveMany(bool bDeposit, const TArray<int32>& Slots);

	FBox2f ClipRect = FBox2f(ForceInit);         // recorte do desenho (lista com rolagem); inválido = sem recorte
	float InvLeft = 0.f;                          // borda esquerda do Content do inventário (px de 1080p)

	EPopup Popup = EPopup::None;
	FSlotRef PopupSlot;
	int32 PopupGemSlot = INDEX_NONE;   // Socket: espaço da gema na bolsa
	int32 PopupQty = 1, PopupMax = 1;
	TArray<FPopupButton> PopupButtons;
	FBox2f SliderRect = FBox2f(ForceInit);
	bool bSliderDrag = false;

	// Campos de texto da busca (Slate, por cima do canvas), um por lista.
	TSharedPtr<SWidget> SearchHost;
	TSharedPtr<SEditableText> SearchEdit[2];
};

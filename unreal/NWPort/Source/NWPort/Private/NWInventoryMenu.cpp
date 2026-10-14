#include "NWInventoryMenu.h"
#include "NWLootBag.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCanvas.h"
#include "GameFramework/PlayerController.h"
#include "NWCharacter.h"
#include "NWCombatComponent.h"
#include "NWInteractable.h"
#include "NWInventoryComponent.h"
#include "NWItemRules.h"
#include "NWProgressionComponent.h"
#include "NWText.h"

// Spec: docs/hud/inventario.md (posições em px da tela de 1080 de altura).
static const FLinearColor InvTan(FColor(0xEB, 0xE3, 0xD6));      // COLOR_TAN_WARM
static const FLinearColor InvTanNew(FColor(0xC6, 0xB6, 0x94));   // títulos de seção, rótulos dos atributos
static const FLinearColor InvGray(FColor(0xB2, 0xAE, 0xA1));     // peso da seção, máximo do peso
static const FLinearColor InvTanBody(FColor(0xA5, 0x98, 0x7D));  // botões de ordenação
static const FLinearColor InvLine(FColor(0xFF, 0xF1, 0xCE));     // linha embaixo do título da seção
static const FLinearColor InvRed(FColor(0xFF, 0x71, 0x71));
static const FLinearColor InvDim(FColor(0x9A, 0x96, 0x8E));
static const FLinearColor InvGold(FColor(0xED, 0xC6, 0x3E));
static const TCHAR* ItemLayoutDir = TEXT("lyshineui/images/slices/itemlayout/");
static const TCHAR* EquipDir = TEXT("lyshineui/images/equipment/");
static const TCHAR* MiscDir = TEXT("lyshineui/images/icons/misc/");
// catsection.lua: peça 60x60, passo 62x65, 4 por linha em 2 colunas. Altura do cabeçalho medida no print 02 (o
// 101 do Lua conta a margem de cima da seção).
static constexpr float Cell = 60.f, CellStepX = 62.f, CellStepY = 65.f, SectionItemsY = 74.f, SectionGap = 16.f, ColumnWidth = 246.f;
static constexpr int32 PerRow = 4;
// equipmentv3: espaço 63x63; itemdraggable: cópia arrastada 75x75.
static constexpr float EquipCell = 63.f, DragSize = 75.f, DragStart = 6.f;
// catcontainer: a janela do armazém fica 316 à direita do lugar do inventário; o inventário vai 300 para a esquerda.
static constexpr float StorageShift = 316.f, InvShift = -300.f;
static constexpr float LootListTop = 214.f;   // lista da bolsa de saque, abaixo do aviso e da contagem

namespace
{
	const TCHAR* Roman(int32 Tier)
	{
		static const TCHAR* R[] = {TEXT(""), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V")};
		return R[FMath::Clamp(Tier, 0, 5)];
	}

	// Seções do jogo (dynamicitemlist.lua, defaultSectionInfos): UiItemClass, coluna esquerda, chave do título.
	struct FSectionDef { const TCHAR* Class; bool bLeft; const TCHAR* Key; };
	const FSectionDef SectionDefs[] = {
		{TEXT("Rewards"), true, TEXT("owg_rewards_title")}, {TEXT("Weapon"), true, TEXT("ui_weapons")},
		{TEXT("Armor"), true, TEXT("inv_apparrel")}, {TEXT("Runes"), true, TEXT("inv_runes")},
		{TEXT("Consumable"), true, TEXT("inv_utilities")}, {TEXT("Cooking"), true, TEXT("inv_cooking")},
		{TEXT("Bait"), true, TEXT("inv_baits")}, {TEXT("CraftingModifiers"), true, TEXT("inv_crafting_modifiers")},
		{TEXT("Material"), true, TEXT("inv_resources")},
		{TEXT("Tools"), false, TEXT("inv_tools")}, {TEXT("TuningOrbs"), false, TEXT("inv_tuningorbs")},
		{TEXT("JewelCrafting"), false, TEXT("inv_jewelcrafting")}, {TEXT("Refining"), false, TEXT("inv_refining")},
		{TEXT("Smelting"), false, TEXT("inv_smelting")}, {TEXT("Leatherworking"), false, TEXT("inv_leatherworking")},
		{TEXT("Weaving"), false, TEXT("inv_weaving")}, {TEXT("Woodworking"), false, TEXT("inv_woodworking")},
		{TEXT("Stonecutting"), false, TEXT("inv_stonecutting")}, {TEXT("Alchemy"), false, TEXT("inv_alchemy")},
		{TEXT("Dye"), false, TEXT("inv_dyes")},
	};
	constexpr int32 DefaultSection = 8;   // Material: o que não tem seção própria vai para recursos
	int32 SectionIndex(FName Class)
	{
		for (int32 i = 0; i < UE_ARRAY_COUNT(SectionDefs); ++i)
		{
			if (Class == FName(SectionDefs[i].Class))
			{
				return i;
			}
		}
		return DefaultSection;
	}
	// Raridade da peça: armas pela raridade sorteada; o resto pelo tier.
	int32 RarityOf(const FNWItemDef& D, const FNWItem& I)
	{
		return D.Type == TEXT("weapon") || D.Type == TEXT("armor") ? FMath::Min<int32>(I.Rarity, 4) : FMath::Clamp(D.Tier - 1, 0, 4);
	}

	// Espaços do port nas posições do equipmentv3 (relativas à origem do painel).
	struct FEquipPos { int32 Slot; float X, Y; const TCHAR* Empty; };
	const FEquipPos CombatSlots[] = {
		{NWEquip::Sword, 548.f, 216.f, TEXT("iconweapon")}, {NWEquip::Hatchet, 548.f, 284.f, TEXT("iconweapon")},
		{NWEquip::Shield, 30.f, 556.f, TEXT("iconshield")},
		// Armadura e joias (docs/armaduras.md).
		{NWEquip::Head, 30.f, 216.f, TEXT("iconhead")}, {NWEquip::Chest, 30.f, 284.f, TEXT("iconchest")},
		{NWEquip::Hands, 30.f, 352.f, TEXT("iconhand")}, {NWEquip::Legs, 30.f, 420.f, TEXT("iconlegs")},
		{NWEquip::Feet, 30.f, 488.f, TEXT("iconfeet")}, {NWEquip::Amulet, 548.f, 438.f, TEXT("iconamulet")},
		{NWEquip::Ring, 548.f, 506.f, TEXT("iconring")}, {NWEquip::Earring, 548.f, 574.f, TEXT("icontoken")},
	};
	const FEquipPos ToolSlots[] = {
		{NWEquip::LoggingAxe, 548.f, 351.f, TEXT("iconlogging")}, {NWEquip::Pick, 548.f, 419.f, TEXT("iconpickaxe")},
		{NWEquip::SkinningKnife, 548.f, 487.f, TEXT("iconskinning")}, {NWEquip::Sickle, 548.f, 555.f, TEXT("iconsickle")},
	};
	const TCHAR* EmptyIcon(int32 Slot)
	{
		for (const FEquipPos& E : CombatSlots) { if (E.Slot == Slot) return E.Empty; }
		for (const FEquipPos& E : ToolSlots) { if (E.Slot == Slot) return E.Empty; }
		return TEXT("iconweapon");
	}
	// Espaços do jogo que o port não tem: vazios, para o painel ter a forma do jogo.
	struct FEmptyPos { float X, Y; const TCHAR* Icon; };
	const FEmptyPos CombatEmpty[] = {{548.f, 352.f, TEXT("iconrune")}};
	const FEmptyPos ToolEmpty[] = {{548.f, 623.f, TEXT("iconfishing")}};

	// Alvo de soltar aceso (containerareadroptarget/dropzoneglow): borda de 2 px e um véu leve. O canvas da Unreal mistura
	// em linear, então preenchimento forte fica pesado demais; a borda marca o alvo.
	void DrawDropZone(UCanvas* Canvas, const FBox2f& B, const FLinearColor& C, bool bFill = true)
	{
		const float T = FMath::Max(2.f, Canvas->ClipY / 540.f);
		if (bFill)
		{
			FCanvasTileItem Fill(FVector2D(B.Min), FVector2D(B.GetSize()), FLinearColor(C.R, C.G, C.B, 0.03f));
			Fill.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Fill);
		}
		const FLinearColor Edge(C.R, C.G, C.B, 0.8f);
		const FVector2f Sz = B.GetSize();
		const FBox2f Edges[4] = {FBox2f(B.Min, B.Min + FVector2f(Sz.X, T)), FBox2f(FVector2f(B.Min.X, B.Max.Y - T), B.Max),
			FBox2f(B.Min, B.Min + FVector2f(T, Sz.Y)), FBox2f(FVector2f(B.Max.X - T, B.Min.Y), B.Max)};
		for (const FBox2f& E : Edges)
		{
			FCanvasTileItem Tile(FVector2D(E.Min), FVector2D(E.GetSize()), Edge);
			Tile.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Tile);
		}
	}

	// Busca sem maiúsculas nem acentos ("fundição" acha "Fundicao").
	FString Fold(const FString& In)
	{
		static const TCHAR* From = TEXT("áàâãäéèêëíìîïóòôõöúùûüçñ");
		static const TCHAR* To = TEXT("aaaaaeeeeiiiiooooouuuucn");
		FString Out = FText::FromString(In).ToLower().ToString();
		for (TCHAR& C : Out)
		{
			for (int32 i = 0; From[i]; ++i)
			{
				if (C == From[i])
				{
					C = To[i];
					break;
				}
			}
		}
		return Out;
	}

	// Fonte do jogo pelo nome do arquivo .font (índices do FNWUILayout::GameFont).
	int32 FontIndex(const TCHAR* Font)
	{
		const FString F(Font);
		return F.Contains(TEXT("pica_italic")) ? 4 : F.Contains(TEXT("pica")) ? 3 : F.Contains(TEXT("medium")) ? 7 : 0;
	}
}

bool FNWInventoryMenu::Load()
{
	if (!Window.Load(NWText::DataPath(TEXT("Data/UI/inventory_layout.json"))))
	{
		return false;
	}
	Window.ResolveTexture = [](const FString& P) { return FNWUILayout::GameTexture(P); };
	Window.bExactText = true;
	for (int32 i = 0; i < 3; ++i)
	{
		Window.Fonts[i] = FNWUILayout::GameFont(i);
	}
	// A janela vai por DrawSubtree no lugar certo (inventário e armazém); o desenho normal só calcula o layout.
	if (FNWUIElement* Content = Window.FindOne(TEXT("inventory"), TEXT("Inventory/Content")))
	{
		Content->Visible = false;
	}
	return true;
}

void FNWInventoryMenu::Open(ANWCharacter* InCharacter)
{
	if (!bLoaded)
	{
		bLoaded = Load();
	}
	if (bLoaded && InCharacter && InCharacter->Inventory)
	{
		Character = InCharacter;
		bPressed = bDragging = false;
		bStorage = bLoot = false;
		LootBag.Reset();
		RenameTab = INDEX_NONE;
		Lists[0].Scroll = 0.f;
		ShowSearch();
	}
}

void FNWInventoryMenu::OpenStorage(ANWCharacter* InCharacter, const FString& InTitle)
{
	Open(InCharacter);
	if (IsOpen())
	{
		bStorage = true;
		StorageTitle = InTitle;
		Lists[1].Scroll = 0.f;
	}
}

void FNWInventoryMenu::OpenLootBag(ANWCharacter* InCharacter, ANWLootBag* Bag)
{
	OpenStorage(InCharacter, NWText::Get(TEXT("inv_droppeditems")));
	if (IsOpen())
	{
		bLoot = true;
		LootBag = Bag;
	}
}

int32 FNWInventoryMenu::TabAt(const FVector2f& M) const
{
	for (int32 t = 0; t < MaxTabs; ++t)
	{
		if (TabRectsStorage[t].bIsValid && TabRectsStorage[t].IsInside(M))
		{
			return t;
		}
	}
	return INDEX_NONE;
}

FBox2f FNWInventoryMenu::RenameRect() const
{
	return IsOpen() && ShowsTabs() && RenameTab >= 0 && RenameTab < MaxTabs ? TabRectsStorage[RenameTab] : FBox2f(ForceInit);
}

void FNWInventoryMenu::CommitRename(const FString& Name)
{
	ANWCharacter* Me = Character.Get();
	if (Me && Me->Interactor && RenameTab != INDEX_NONE)
	{
		Me->Interactor->ServerStorageRenameTab(uint8(RenameTab), Name);
	}
	RenameTab = INDEX_NONE;
}

void FNWInventoryMenu::DrawTabs(UCanvas* Canvas, const FVector2D& Mouse)
{
	// Cinco abas como botões do jogo (buttonbg #2c251b + primarybuttonframe); a aberta com a moldura cheia e o texto claro.
	// Arrastando um item, a aba sob o mouse acende (soltar guarda ou passa para ela).
	const ANWCharacter* Me = Character.Get();
	const TArray<FString>* Names = Me && Me->Interactor ? &Me->Interactor->StorageTabNames : nullptr;
	const float S = UIScale;
	const int32 Hot = TabAt(FVector2f(Mouse));
	for (int32 t = 0; t < MaxTabs; ++t)
	{
		const FBox2f& R = TabRectsStorage[t];
		if (!R.bIsValid)
		{
			continue;
		}
		const bool bSel = t == StorageTab, bHot = t == Hot;
		DrawTexture(Canvas, TEXT("lyshineui/images/slices/button/buttonbg.dds"), R, FLinearColor(FColor(0x2C, 0x25, 0x1B)));
		DrawTexture(Canvas, TEXT("lyshineui/images/slices/button/primarybuttonframe.dds"), R,
			FLinearColor(1.f, 1.f, 1.f, bSel ? 1.f : bHot ? 0.8f : 0.4f));
		if (bDragging && bHot)
		{
			DrawDropZone(Canvas, R, InvTan);
		}
		if (t == RenameTab && RenameEdit.IsValid())
		{
			continue;   // o campo de texto do Slate fica por cima
		}
		FString Label = Names && Names->IsValidIndex(t) && !(*Names)[t].IsEmpty() ? (*Names)[t]
			: NWText::Get(TEXT("nw.storage.tab")).Replace(TEXT("{0}"), *FString::FromInt(t + 1));
		const float MaxW = R.GetSize().X - 12.f * S;
		while (Label.Len() > 1 && GameTextWidth(Label, 22.f, TEXT("nimbus_medium")) > MaxW)
		{
			Label.LeftChopInline(1);
		}
		DrawGameText(Canvas, Label, R.GetCenter().X - GameTextWidth(Label, 22.f, TEXT("nimbus_medium")) * 0.5f, R.Min.Y + 7.f * S, 22.f,
			bSel || bHot ? InvTan : InvGray, TEXT("nimbus_medium"));
	}
}

const TArray<FNWItem>* FNWInventoryMenu::ContainerItems() const
{
	const ANWCharacter* Me = Character.Get();
	if (bLoot)
	{
		return LootBag.IsValid() ? &LootBag->Items : nullptr;
	}
	return Me && Me->Interactor ? &Me->Interactor->StorageItems : nullptr;
}

void FNWInventoryMenu::ContainerMove(bool bDeposit, int32 Slot, int32 Quantity)
{
	ANWCharacter* Me = Character.Get();
	if (!Me || !Me->Interactor)
	{
		return;
	}
	if (!bLoot)
	{
		Me->Interactor->ServerStorageMove(bDeposit, Slot, Quantity, uint8(StorageTab));
	}
	else if (!bDeposit && LootBag.IsValid())
	{
		Me->Interactor->ServerLootTakeItem(LootBag.Get(), Slot, Quantity);
	}
}

void FNWInventoryMenu::ContainerMoveMany(bool bDeposit, const TArray<int32>& InSlots)
{
	ANWCharacter* Me = Character.Get();
	if (!Me || !Me->Interactor)
	{
		return;
	}
	if (!bLoot)
	{
		Me->Interactor->ServerStorageMoveMany(bDeposit, InSlots, uint8(StorageTab));
		return;
	}
	const TArray<FNWItem>* Items = ContainerItems();
	for (int32 Slot : bDeposit || !Items ? TArray<int32>() : InSlots)
	{
		if (const FNWItem* I = Items->FindByPredicate([Slot](const FNWItem& It) { return It.Slot == Slot; }))
		{
			Me->Interactor->ServerLootTakeItem(LootBag.Get(), Slot, I->Quantity);
		}
	}
}

void FNWInventoryMenu::Close()
{
	if (ANWCharacter* Me = Character.Get(); Me && bStorage && !bLoot && Me->Interactor)
	{
		Me->Interactor->ServerStorageClose();
	}
	Character.Reset();
	bStorage = bLoot = bPressed = bDragging = false;
	LootBag.Reset();
	Popup = EPopup::None;
	HideSearch();
}

bool FNWInventoryMenu::ModifierDown(const FKey& Key) const
{
	const ANWCharacter* Me = Character.Get();
	const APlayerController* PC = Me ? Cast<APlayerController>(Me->GetController()) : nullptr;
	return PC && PC->IsInputKeyDown(Key);
}

const FNWItem* FNWInventoryMenu::ItemOf(const FSlotRef& S) const
{
	const ANWCharacter* Me = Character.Get();
	if (!Me)
	{
		return nullptr;
	}
	if (S.bStorage)
	{
		const TArray<FNWItem>* Items = ContainerItems();
		return Items ? Items->FindByPredicate([&S](const FNWItem& I) { return I.Slot == S.Slot; }) : nullptr;
	}
	return Me->Inventory ? Me->Inventory->At(S.Container, S.Slot) : nullptr;
}

bool FNWInventoryMenu::Accepts(int32 EquipSlot, const FNWItem& Item) const
{
	const FNWItemDef* D = FNWItemCatalog::Get().Find(Item.ItemId);
	return D && NWEquip::SlotForSet(D->Set) == EquipSlot;
}

int32 FNWInventoryMenu::FreeBagSlot() const
{
	const ANWCharacter* Me = Character.Get();
	for (int32 B = 0; Me && Me->Inventory && B < UNWInventoryComponent::BagSize; ++B)
	{
		if (!Me->Inventory->At(ENWItemContainer::Bag, B))
		{
			return B;
		}
	}
	return INDEX_NONE;
}

float FNWInventoryMenu::TextWidth(const FString& Text, float Size, int32 Style) const
{
	UFont* Font = FNWUILayout::GameFont(Style);
	if (!Font)
	{
		return 0.f;
	}
	return Font->GetStringSize(*Text) * Size * UIScale / Font->LegacyFontSize;
}

void FNWInventoryMenu::DrawText(UCanvas* Canvas, const FString& Text, float X, float Y, float Size, const FLinearColor& Color, int32 Style) const
{
	UFont* Font = FNWUILayout::GameFont(Style);
	if (!Font || Text.IsEmpty())
	{
		return;
	}
	const float Scale = Size * UIScale / Font->LegacyFontSize;
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Color);
	Item.Scale = FVector2D(Scale, Scale);
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	Canvas->DrawItem(Item);
}

float FNWInventoryMenu::GameScale(UFont* Font, int32 Index, float Size) const
{
	// Tamanho do LyShine = altura da linha (FNWUILayout::TextScale no modo fiel), medida uma vez por fonte.
	if (LineH[Index] <= 0.f && DrawCanvas)
	{
		float W1 = 0.f;
		DrawCanvas->TextSize(Font, TEXT("Ag"), W1, LineH[Index], 1.f, 1.f);
	}
	return Size * UIScale / FMath::Max(1.f, LineH[Index]);
}

float FNWInventoryMenu::GameTextWidth(const FString& Text, float Size, const TCHAR* FontName) const
{
	const int32 Idx = FontIndex(FontName);
	UFont* Font = FNWUILayout::GameFont(Idx);
	if (!Font || !DrawCanvas || Text.IsEmpty())
	{
		return 0.f;
	}
	float W = 0.f, H = 0.f;
	DrawCanvas->TextSize(Font, Text, W, H, 1.f, 1.f);
	return W * GameScale(Font, Idx, Size);
}

void FNWInventoryMenu::DrawGameText(UCanvas* Canvas, const FString& Text, float X, float Y, float Size, const FLinearColor& Color, const TCHAR* FontName, bool bRight) const
{
	const int32 Idx = FontIndex(FontName);
	UFont* Font = FNWUILayout::GameFont(Idx);
	if (!Font || Text.IsEmpty())
	{
		return;
	}
	const float Scale = GameScale(Font, Idx, Size);
	if (ClipRect.bIsValid && (Y < ClipRect.Min.Y - 2.f || Y + Size * UIScale * 0.8f > ClipRect.Max.Y))
	{
		return;
	}
	const float W = bRight ? GameTextWidth(Text, Size, FontName) : 0.f;
	FCanvasTextItem Item(FVector2D(X - W, Y), FText::FromString(Text), Font, Color);
	Item.Scale = FVector2D(Scale, Scale);
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	Canvas->DrawItem(Item);
}

void FNWInventoryMenu::DrawTexture(UCanvas* Canvas, const FString& Path, const FBox2f& R, const FLinearColor& Tint, bool bAdditive) const
{
	if (UTexture2D* Tex = FNWUILayout::GameTexture(Path))
	{
		DrawTile(Canvas, Tex, R, Tint, bAdditive);
	}
}

void FNWInventoryMenu::DrawTile(UCanvas* Canvas, UTexture2D* Tex, const FBox2f& R, const FLinearColor& Tint, bool bAdditive, FVector2f UV0, FVector2f UV1) const
{
	if (!Tex)
	{
		return;
	}
	FBox2f D = R;
	if (ClipRect.bIsValid)
	{
		// Recorte da lista: corta o retângulo e as coordenadas de textura na mesma proporção.
		D = FBox2f(FVector2f::Max(R.Min, ClipRect.Min), FVector2f::Min(R.Max, ClipRect.Max));
		if (D.Max.X <= D.Min.X || D.Max.Y <= D.Min.Y)
		{
			return;
		}
		const FVector2f Size = R.GetSize(), DUV = UV1 - UV0;
		const FVector2f A = (D.Min - R.Min) / Size, B = (D.Max - R.Min) / Size;
		const FVector2f N0 = UV0 + DUV * A, N1 = UV0 + DUV * B;
		UV0 = N0;
		UV1 = N1;
	}
	FCanvasTileItem Tile(FVector2D(D.Min), Tex->GetResource(), FVector2D(D.GetSize()), FVector2D(UV0), FVector2D(UV1), Tint);
	Tile.BlendMode = bAdditive ? SE_BLEND_Additive : SE_BLEND_Translucent;
	Canvas->DrawItem(Tile);
}

void FNWInventoryMenu::FillRect(UCanvas* Canvas, const FBox2f& R, const FLinearColor& Color) const
{
	FBox2f D = R;
	if (ClipRect.bIsValid)
	{
		D = FBox2f(FVector2f::Max(R.Min, ClipRect.Min), FVector2f::Min(R.Max, ClipRect.Max));
		if (D.Max.X <= D.Min.X || D.Max.Y <= D.Min.Y)
		{
			return;
		}
	}
	FCanvasTileItem Tile(FVector2D(D.Min), FVector2D(D.GetSize()), Color);
	Tile.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Tile);
}

// ------------------------------------------------------------------------------------------------------------ layout

TArray<int32> FNWInventoryMenu::SectionSlots(FName Class, bool bStorageItems) const
{
	TArray<int32> Out;
	const ANWCharacter* Me = Character.Get();
	const TArray<FNWItem>* Items = !Me ? nullptr : bStorageItems ? ContainerItems() : (Me->Inventory ? &Me->Inventory->Items : nullptr);
	for (const FNWItem& I : Items ? *Items : TArray<FNWItem>())
	{
		const FNWItemDef* D = FNWItemCatalog::Get().Find(I.ItemId);
		if (D && (bStorageItems || I.Container == ENWItemContainer::Bag) && InList(I, bStorageItems)
			&& FName(SectionDefs[SectionIndex(D->UiClass)].Class) == Class)
		{
			Out.Add(I.Slot);
		}
	}
	return Out;
}

void FNWInventoryMenu::LayoutList(FList& L, bool bStorageItems)
{
	// dynamicitemlist.lua UpdateSizes: seções empilhadas em duas colunas (cada uma com a própria altura); seção vazia
	// some; recolhida fica só com o cabeçalho.
	const ANWCharacter* Me = Character.Get();
	const TArray<FNWItem>* Items = !Me ? nullptr : bStorageItems ? ContainerItems() : (Me->Inventory ? &Me->Inventory->Items : nullptr);
	TArray<TArray<const FNWItem*>> BySection;
	BySection.SetNum(UE_ARRAY_COUNT(SectionDefs));
	const FString Query = Fold(L.Search.TrimStartAndEnd());
	for (const FNWItem& I : Items ? *Items : TArray<FNWItem>())
	{
		const FNWItemDef* D = FNWItemCatalog::Get().Find(I.ItemId);
		if (D && (bStorageItems || I.Container == ENWItemContainer::Bag) && InList(I, bStorageItems)
			&& (Query.IsEmpty() || Fold(D->Name).Contains(Query) || Fold(D->TypeName).Contains(Query)))
		{
			BySection[SectionIndex(D->UiClass)].Add(&I);
		}
	}
	// inventoryfilter.lua: a chave escolhida, maior primeiro (clicar de novo inverte). O port não guarda a hora de
	// chegada: o espaço mais alto vale como o mais novo.
	const int32 Sort = L.Sort, Dir = L.SortDir;
	auto Key = [Sort](const FNWItem& I) -> double
	{
		const FNWItemDef* D = FNWItemCatalog::Get().Find(I.ItemId);
		return Sort == 2 ? (D ? D->Weight * I.Quantity : 0.f) : Sort == 3 ? double(I.GearScore) : Sort == 4 ? double(D ? D->Tier : 0) : double(I.Slot);
	};
	const float S = UIScale;
	const float AreaX = L.Area.Min.X / S, AreaY = L.Area.Min.Y / S, AreaW = L.Area.GetSize().X / S;
	const float ColX[2] = {AreaX, AreaX + 0.5147f * AreaW};
	float ColY[2] = {0.f, 0.f};
	L.Sections.Reset();
	for (int32 s = 0; s < BySection.Num(); ++s)
	{
		TArray<const FNWItem*>& Sec = BySection[s];
		if (Sec.IsEmpty())
		{
			continue;
		}
		Sec.StableSort([&](const FNWItem& A, const FNWItem& B)
		{
			const double KA = Key(A), KB = Key(B);
			return KA != KB ? (Dir > 0 ? KA > KB : KA < KB) : A.Slot > B.Slot;
		});
		const int32 Col = SectionDefs[s].bLeft ? 0 : 1;
		const FName Class(SectionDefs[s].Class);
		const bool bCollapsed = L.Collapsed.Contains(Class);
		const float X = ColX[Col];
		const float Top = AreaY + ColY[Col] - L.Scroll;
		FSectionHit Hit;
		Hit.Class = Class;
		Hit.Arrow = FBox2f(FVector2f(X - 8.f, Top) * S, FVector2f(X + 22.f, Top + 30.f) * S);
		Hit.Transfer = FBox2f(FVector2f(X + ColumnWidth - 30.f, Top + 38.f) * S, FVector2f(X + ColumnWidth, Top + 66.f) * S);
		L.Sections.Add(Hit);
		const int32 Rows = bCollapsed ? 0 : FMath::DivideAndRoundUp(Sec.Num(), PerRow);
		for (int32 i = 0; i < Sec.Num() && !bCollapsed; ++i)
		{
			const float IX = X + (i % PerRow) * CellStepX;
			const float IY = Top + SectionItemsY + (i / PerRow) * CellStepY;
			const FBox2f R(FVector2f(IX, IY) * S, FVector2f(IX + Cell, IY + Cell) * S);
			const FBox2f Visible(FVector2f(R.Min.X, FMath::Max(R.Min.Y, L.Area.Min.Y)), FVector2f(R.Max.X, FMath::Min(R.Max.Y, L.Area.Max.Y)));
			if (Visible.Max.Y <= Visible.Min.Y)
			{
				continue;   // fora da área com rolagem
			}
			FSlotRef Ref;
			Ref.Container = ENWItemContainer::Bag;
			Ref.Slot = Sec[i]->Slot;
			Ref.bStorage = bStorageItems;
			Ref.Rect = Visible;
			Ref.Full = R;
			Slots.Add(Ref);
		}
		ColY[Col] += SectionItemsY + Rows * CellStepY + SectionGap;
	}
	L.ContentH = FMath::Max(ColY[0], ColY[1]);
	L.Scroll = FMath::Clamp(L.Scroll, 0.f, FMath::Max(0.f, L.ContentH - L.Area.GetSize().Y / S));
}

void FNWInventoryMenu::LayoutEquipment()
{
	const float S = UIScale;
	// Abas em ícone (ViewsTabbedList [-2,74], botões 55x55, margem 9).
	for (int32 t = 0; t < 2; ++t)
	{
		const FVector2f P = EquipOrigin + FVector2f(30.f + t * 64.f, 74.f);
		TabRects[t] = FBox2f(P * S, (P + FVector2f(55.f)) * S);
	}
	auto Add = [&](int32 Slot, float X, float Y)
	{
		FSlotRef R;
		R.Container = ENWItemContainer::Equip;
		R.Slot = Slot;
		const FVector2f P = EquipOrigin + FVector2f(X, Y);
		R.Rect = R.Full = FBox2f(P * S, (P + FVector2f(EquipCell)) * S);
		Slots.Add(R);
	};
	for (const FEquipPos& P : EquipTab == 0 ? MakeArrayView(CombatSlots) : MakeArrayView(ToolSlots))
	{
		Add(P.Slot, P.X, P.Y);
	}
	// Alvo universal: o lado inteiro do equipamento.
	EquipArea = FBox2f((EquipOrigin + FVector2f(0.f, 40.f)) * S, (EquipOrigin + FVector2f(640.f, 1040.f)) * S);
}

void FNWInventoryMenu::Layout(UCanvas* Canvas)
{
	const float S = UIScale;
	Slots.Reset();
	// Content do canvas: âncora no centro + 6,5 (x 966 de 1920), largura 623.
	const float W0 = Canvas->ClipX / S * 0.5f + 6.5f;
	InvLeft = W0;
	if (bLoot && !LootBag.IsValid())
	{
		bStorage = bLoot = false;   // a bolsa sumiu (vazia ou 3 min): fica só o inventário
	}
	InvOffsetX = bStorage ? InvShift : 0.f;
	auto SetWindow = [S](FList& L, float Left)
	{
		L.Window = FBox2f(FVector2f(Left + 22.f, 63.f) * S, FVector2f(Left + 609.f, 995.f) * S);
		L.Area = FBox2f(FVector2f(Left + 36.f, 184.f) * S, FVector2f(Left + 609.f, 926.f) * S);
		L.SearchBox = FBox2f(FVector2f(Left + 36.f, 113.f) * S, FVector2f(Left + 396.f, 158.f) * S);
		for (int32 i = 0; i < 4; ++i)
		{
			const float X = Left + 455.f + i * 34.f;
			L.SortRects[i] = FBox2f(FVector2f(X, 113.f) * S, FVector2f(X + 34.f, 158.f) * S);
		}
	};
	SetWindow(Lists[0], W0 + InvOffsetX);
	LayoutList(Lists[0], false);
	if (bStorage)
	{
		SetWindow(Lists[1], W0 + StorageShift);
		if (bLoot)
		{
			// Bolsa de saque: sem busca nem ordenação; aviso e contagem no lugar (LootBagInfoContainer), lista mais abaixo.
			Lists[1].SearchBox = FBox2f(ForceInit);
			for (FBox2f& R : Lists[1].SortRects)
			{
				R = FBox2f(ForceInit);
			}
			Lists[1].Area.Min.Y = LootListTop * S;
		}
		else
		{
			// Abas embaixo da busca, a lista desce.
			const float L0 = W0 + StorageShift + 36.f, L1 = W0 + StorageShift + 593.f, Gap = 6.f;
			const float W = (L1 - L0 - Gap * (MaxTabs - 1)) / MaxTabs;
			const int32 N = FMath::Clamp(FNWItemRules::Get().StorageTabs, 1, MaxTabs);
			for (int32 t = 0; t < MaxTabs; ++t)
			{
				const float X = L0 + t * (W + Gap);
				TabRectsStorage[t] = t < N ? FBox2f(FVector2f(X, 174.f) * S, FVector2f(X + W, 212.f) * S) : FBox2f(ForceInit);
			}
			Lists[1].Area.Min.Y = 222.f * S;
		}
		LayoutList(Lists[1], true);
		TakeAllRect = FBox2f(FVector2f(W0 + StorageShift + 425.f, 935.f) * S, FVector2f(W0 + StorageShift + 595.f, 985.f) * S);
	}
	// Equipamento: armadura em x≈311 de 1920 (print 02), 300 px à esquerda com o armazém.
	EquipOrigin = FVector2f(W0 - 655.f + InvOffsetX, 0.f);
	LayoutEquipment();
}

const FNWInventoryMenu::FSlotRef* FNWInventoryMenu::SlotAt(const FVector2D& Mouse) const
{
	return Slots.FindByPredicate([&](const FSlotRef& S) { return S.Rect.IsInside(FVector2f(Mouse)); });
}

// ------------------------------------------------------------------------------------------------------------ desenho

void FNWInventoryMenu::DrawItemTile(UCanvas* Canvas, const FBox2f& R, const FNWItem* Item, bool bRound, float Alpha, bool bHover, int32 SortShown)
{
	const FNWItemDef* D = Item ? FNWItemCatalog::Get().Find(Item->ItemId) : nullptr;
	if (!D)
	{
		return;
	}
	// itemlayout.lua SetRarityBgColor: moldura itemBg{Square|Circle}{r} e fundo itemRarityBg{Square|Circle}{r}.
	const TCHAR* Shape = bRound ? TEXT("circle") : TEXT("square");
	const int32 Rarity = RarityOf(*D, *Item);
	const FLinearColor A(1.f, 1.f, 1.f, Alpha);
	DrawTexture(Canvas, FString::Printf(TEXT("%sitemraritybg%s%d.dds"), ItemLayoutDir, Shape, Rarity), R, A);
	DrawTexture(Canvas, FString::Printf(TEXT("%sitembg%s%d.dds"), ItemLayoutDir, Shape, Rarity), R, A);
	const float K = R.GetSize().X / (Cell * UIScale);   // escala da peça (60 = 1)
	const float S = UIScale * K;
	if (UTexture2D* Icon = FNWUILayout::GameTexture(D->Icon))
	{
		// Ícone = peça − 6 px, centrado.
		const float Side = R.GetSize().X - 6.f * S;
		const FVector2f C = R.GetCenter();
		DrawTile(Canvas, Icon, FBox2f(C - FVector2f(Side * 0.5f), C + FVector2f(Side * 0.5f)), A);
	}
	// Canto de cima: peso ao ordenar por peso, gear score ao ordenar por gear score; senão o tier (Pica itálico 14).
	const FLinearColor TextA(InvTan.R, InvTan.G, InvTan.B, Alpha);
	if (SortShown == 2)
	{
		DrawTexture(Canvas, FString(MiscDir) + TEXT("icon_weight.dds"), FBox2f(R.Min + FVector2f(4.f, 6.f) * S, R.Min + FVector2f(14.f, 16.f) * S), TextA);
		DrawGameText(Canvas, FString::Printf(TEXT("%.1f"), D->Weight * Item->Quantity), R.Min.X + 16.f * S, R.Min.Y + 2.f * S, 17.f * K, TextA, TEXT("pica_regular"));
	}
	else if (SortShown == 3 && Item->GearScore > 0)
	{
		DrawGameText(Canvas, FString::FromInt(Item->GearScore), R.Min.X + 4.f * S, R.Min.Y + 2.f * S, 17.f * K, TextA, TEXT("pica_regular"));
	}
	else
	{
		DrawGameText(Canvas, Roman(D->Tier), R.Min.X + 5.f * S, R.Min.Y + 1.f * S, 14.f * K, TextA, TEXT("pica_italic"));
	}
	// Gema acoplada: ícone dela no canto de baixo à direita, sobre o soquete do tooltip (docs/itens.md "Slot").
	if (const FNWItemDef* GD = Item->Gem.IsNone() ? nullptr : FNWItemCatalog::Get().Find(Item->Gem))
	{
		// Pouco mais de metade da peça (34 de 60), para a gema ser reconhecível na bolsa.
		const FBox2f GB(FVector2f(R.Max.X - 34.f * S, R.Max.Y - 34.f * S), FVector2f(R.Max.X, R.Max.Y));
		DrawTexture(Canvas, TEXT("lyshineui/images/tooltip/frame_gemsocket.dds"), GB, A);
		if (UTexture2D* GemIcon = FNWUILayout::GameTexture(GD->Icon))
		{
			DrawTile(Canvas, GemIcon, FBox2f(GB.Min + FVector2f(2.f * S), GB.Max - FVector2f(2.f * S)), A);
		}
	}
	// Quantidade só acima de 1, sobre frame_lootticker no canto de baixo à direita.
	if (Item->Quantity > 1)
	{
		const FString Q = FString::FromInt(Item->Quantity);
		const float TW = GameTextWidth(Q, 22.f * K, TEXT("pica_regular"));
		const FBox2f QB(FVector2f(R.Max.X - TW - 10.f * S, R.Max.Y - 21.f * S), FVector2f(R.Max.X + 2.f * S, R.Max.Y + 2.f * S));
		DrawTexture(Canvas, TEXT("lyshineui/images/frame_lootticker.dds"), QB, FLinearColor(1.f, 1.f, 1.f, 0.85f * Alpha));
		DrawGameText(Canvas, Q, R.Max.X - 3.f * S, R.Max.Y - 22.f * S, 22.f * K, FLinearColor(1.f, 1.f, 1.f, Alpha), TEXT("pica_regular"), true);
	}
	if (bHover)
	{
		// EnableHighlight: pulsa entre 0,5 e 1.
		const float Pulse = 0.75f + 0.25f * FMath::Cos(float(FPlatformTime::Seconds()) * 2.f * PI / 1.3f);
		DrawTexture(Canvas, FString::Printf(TEXT("%sitemhighlight%s.dds"), ItemLayoutDir, Shape), R, FLinearColor(1.f, 1.f, 1.f, Pulse));
	}
}

void FNWInventoryMenu::DrawWindow(UCanvas* Canvas, const FList& L, float OffsetX, bool bStorageWindow, const FVector2D& Mouse)
{
	// Moldura, cabeçalho, ícone, título e moedas do canvas newinventory, deslocados para o lugar da janela.
	const ANWCharacter* Me = Character.Get();
	const float S = UIScale;
	FNWUIElement* Content = Window.FindOne(TEXT("inventory"), TEXT("Inventory/Content"));
	if (!Content)
	{
		return;
	}
	if (FNWUIElement* Title = Window.FindOne(TEXT("inventory"), TEXT("Content/InventoryTitle")))
	{
		Title->TextOverride = bStorageWindow ? StorageTitle : TOptional<FString>();
	}
	if (FNWUIElement* Icon = Window.FindOne(TEXT("inventory"), TEXT("Content/InventoryIcon")))
	{
		// catcontainer: ícone do galpão 36x36 no lugar do ícone da mochila.
		Icon->TextureOverride = !bStorageWindow ? FString() : bLoot ? TEXT("lyshineui/images/catcontainer/icon_dropped_items.dds")
			: TEXT("lyshineui/images/catcontainer/icon_storage_shed.dds");
	}
	if (FNWUIElement* Coins = Window.FindOne(TEXT("inventory"), TEXT("Content/CurrencyText")))
	{
		Coins->Visible = !bStorageWindow;
		Coins->TextOverride = UNWInventoryComponent::FormatCoins(Me && Me->Inventory ? Me->Inventory->Coins : 0);
	}
	Window.DrawSubtree(Canvas, TEXT("inventory"), Content, FVector2f(OffsetX, 0.f));

	// Peso (weightbar): valor Pica 28 #ebe3d6 + "/ máximo" Pica 28 #b2aea1, ícone 16x16, à direita do cabeçalho.
	float Cur = 0.f, Max = 1.f;
	if (bStorageWindow && bLoot)
	{
		// Bolsa no chão: peso dela contra o limite de itens no chão do jogador.
		Cur = LootBag.IsValid() ? LootBag->Weight() : 0.f;
		Max = ANWLootBag::MaxGroundWeight;
	}
	else if (bStorageWindow && Me && Me->Interactor)
	{
		Cur = Me->Interactor->StorageWeight();
		Max = Me->Interactor->StorageCapacity;
	}
	else if (!bStorageWindow && Me && Me->Inventory)
	{
		Cur = Me->Inventory->TotalWeight();
		Max = Me->Inventory->MaxWeight;
	}
	const FString MaxText = FString::Printf(TEXT("/ %.1f"), Max), CurText = FString::Printf(TEXT("%.1f"), Cur);
	const float Right = L.Window.Max.X - 18.f * S, Y = 66.f * S;
	const float MaxW = GameTextWidth(MaxText, 28.f, TEXT("pica_regular"));
	const float CurW = GameTextWidth(CurText, 28.f, TEXT("pica_regular"));
	DrawGameText(Canvas, MaxText, Right, Y, 28.f, InvGray, TEXT("pica_regular"), true);
	DrawGameText(Canvas, CurText, Right - MaxW - 6.f * S, Y, 28.f, Cur > Max ? InvRed : InvTan, TEXT("pica_regular"), true);
	const float IX = Right - MaxW - CurW - 30.f * S;
	DrawTexture(Canvas, FString(MiscDir) + TEXT("icon_weight.dds"), FBox2f(FVector2f(IX, Y + 7.f * S), FVector2f(IX + 16.f * S, Y + 23.f * S)), Cur > Max ? InvRed : InvTan);

	// Ordenação (itemfilterbutton.lua): #a5987d, selecionado e hover #ebe3d6, barra 34x2 embaixo do selecionado.
	static const TCHAR* SortIcons[4] = {TEXT("icon_filter_chrono"), TEXT("icon_weight"), TEXT("icon_filter_gearscore"), TEXT("icon_filter_tier")};
	static const FVector2f SortSize[4] = {FVector2f(33.f, 33.f), FVector2f(22.f, 20.f), FVector2f(30.f, 30.f), FVector2f(30.f, 30.f)};
	const bool bLootWindow = bStorageWindow && bLoot;   // bolsa de saque: sem ordenação nem busca
	for (int32 i = 0; i < (bLootWindow ? 0 : 4); ++i)
	{
		const FBox2f& B = L.SortRects[i];
		const bool bSel = L.Sort == i + 1;
		const bool bHot = B.IsInside(FVector2f(Mouse));
		const FVector2f C = B.GetCenter();
		DrawTexture(Canvas, FString::Printf(TEXT("%s%s.dds"), MiscDir, SortIcons[i]), FBox2f(C - SortSize[i] * 0.5f * S, C + SortSize[i] * 0.5f * S),
			bSel || bHot ? InvTan : InvTanBody);
		if (bSel)
		{
			FCanvasTileItem Bar(FVector2D(B.Min.X, B.Max.Y - 2.f * S), FVector2D(B.GetSize().X, 2.f * S), InvTan);
			Canvas->DrawItem(Bar);
		}
	}
	// Busca (textinputsearch): fundo 360x45, lupa, "Buscar" em Nimbus médio 25 #b2aea1. O texto digitado é do campo do
	// Slate por cima; sem ele (captura offline), o canvas desenha o texto.
	if (!bLootWindow)
	{
		const FBox2f& B = L.SearchBox;
		const int32 Li = bStorageWindow ? 1 : 0;
		const bool bFocus = SearchEdit[Li].IsValid() && SearchEdit[Li]->HasKeyboardFocus();
		DrawTexture(Canvas, bFocus ? TEXT("lyshineui/images/slices/textinputsearch/primarysearchbgfocus.dds") : TEXT("lyshineui/images/slices/textinputsearch/primarysearchbg.dds"), B);
		DrawTexture(Canvas, TEXT("lyshineui/images/socialpane/icon_magnifying_glass.dds"),
			FBox2f(FVector2f(B.Min.X + 12.f * S, B.GetCenter().Y - 10.f * S), FVector2f(B.Min.X + 32.f * S, B.GetCenter().Y + 10.f * S)), InvGray);
		if (L.Search.IsEmpty() && !bFocus)
		{
			DrawGameText(Canvas, NWText::Get(TEXT("ui_searchitem")), B.Min.X + 44.f * S, B.Min.Y + 9.f * S, 25.f, InvGray, TEXT("nimbus_medium"));
		}
		else if (!SearchEdit[Li].IsValid())
		{
			DrawGameText(Canvas, L.Search, B.Min.X + 44.f * S, B.Min.Y + 9.f * S, 25.f, InvTan, TEXT("nimbus_medium"));
		}
	}
	if (bLootWindow)
	{
		// LootBagInfoContainer: aviso de tempo de vida (Nimbus 23 #c8c8c8, quebra por palavra, entrelinha −4) e a contagem
		// "N itens" embaixo.
		const FLinearColor Info(FColor(0xC8, 0xC8, 0xC8));
		const float InfoX = L.Area.Min.X, InfoW = L.Area.GetSize().X - 24.f * S;
		float InfoY = 108.f * S;
		FString Line;
		TArray<FString> Words;
		NWText::Get(TEXT("inv_discard_life_warning")).ParseIntoArrayWS(Words);
		for (int32 w = 0; w <= Words.Num(); ++w)
		{
			const FString Try = w < Words.Num() ? (Line.IsEmpty() ? Words[w] : Line + TEXT(" ") + Words[w]) : FString();
			if (w == Words.Num() || (!Line.IsEmpty() && GameTextWidth(Try, 23.f, TEXT("nimbus_regular")) > InfoW))
			{
				DrawGameText(Canvas, Line, InfoX, InfoY, 23.f, Info, TEXT("nimbus_regular"));
				InfoY += 23.f * S;
				Line = w < Words.Num() ? Words[w] : FString();
			}
			else
			{
				Line = Try;
			}
		}
		int32 N = 0;
		for (const FNWItem& I : ContainerItems() ? *ContainerItems() : TArray<FNWItem>())
		{
			N += I.Quantity;
		}
		const FString Count = N == 1 ? NWText::Get(TEXT("ui_loot_bag_1_item"))
			: NWText::Get(TEXT("ui_loot_bag_items")).Replace(TEXT("{amount}"), *FString::FromInt(N));
		DrawGameText(Canvas, Count, InfoX, InfoY + 8.f * S, 23.f, Info, TEXT("nimbus_regular"));
	}
	// Linha embaixo da barra de ordenação, como o fundo da busca do jogo.
	FCanvasTileItem Sep(FVector2D(L.Area.Min.X, (bLootWindow ? LootListTop - 18.f : 166.f) * S), FVector2D(L.Area.GetSize().X - 16.f * S, 1.f * S), FLinearColor(InvLine.R, InvLine.G, InvLine.B, 0.25f));
	Sep.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Sep);
	if (bStorageWindow && ShowsTabs())
	{
		DrawTabs(Canvas, Mouse);
	}

	if (bStorageWindow)
	{
		// Pegar tudo (catcontainer ButtonTakeAll 170x50): buttonbg #2c251b + primarybuttonframe, Pica.
		const bool bHot = TakeAllRect.IsInside(FVector2f(Mouse));
		DrawTexture(Canvas, TEXT("lyshineui/images/slices/button/buttonbg.dds"), TakeAllRect, FLinearColor(FColor(0x2C, 0x25, 0x1B)));
		DrawTexture(Canvas, TEXT("lyshineui/images/slices/button/primarybuttonframe.dds"), TakeAllRect, FLinearColor(1.f, 1.f, 1.f, bHot ? 1.f : 0.75f));
		const FString T = NWText::Get(TEXT("interact_takeall"));
		DrawGameText(Canvas, T, TakeAllRect.GetCenter().X - GameTextWidth(T, 26.f, TEXT("pica_regular")) * 0.5f, TakeAllRect.Min.Y + 11.f * S, 26.f,
			bHot ? FLinearColor::White : InvTan, TEXT("pica_regular"));
	}
}

void FNWInventoryMenu::DrawList(UCanvas* Canvas, const FList& L, bool bStorageItems, const FVector2D& Mouse, const FSlotRef* Hover)
{
	const float S = UIScale;
	const FVector2f M(Mouse);
	// Tudo da lista recortado na área com rolagem.
	auto Inside = [&L](const FBox2f& B) { return B.Min.Y >= L.Area.Min.Y - 1.f && B.Max.Y <= L.Area.Max.Y + 1.f; };
	ClipRect = L.Area;
	for (const FSectionHit& H : L.Sections)
	{
		const int32 Idx = SectionIndex(H.Class);
		const bool bCollapsed = L.Collapsed.Contains(H.Class);
		const float Left = H.Arrow.Min.X + 8.f * S;
		// Título: seta dropdownarrowwhite 18x18 (−90° recolhida) + Pica 27 #c6b694 em maiúsculas; linha #fff1ce.
		if (Inside(FBox2f(H.Arrow.Min, H.Arrow.Max + FVector2f(0.f, 8.f * S))))
		{
			if (UTexture2D* Arrow = FNWUILayout::GameTexture(FString(MiscDir) + TEXT("dropdownarrowwhite.dds")))
			{
				const FVector2f AC = H.Arrow.GetCenter();
				FCanvasTileItem Tile(FVector2D(AC - FVector2f(9.f * S)), Arrow->GetResource(), FVector2D(18.f * S, 18.f * S),
					H.Arrow.IsInside(M) ? InvTan : InvTanNew);
				Tile.BlendMode = SE_BLEND_Translucent;
				Tile.Rotation = FRotator(0.f, bCollapsed ? -90.f : 0.f, 0.f);
				Tile.PivotPoint = FVector2D(0.5f, 0.5f);
				Canvas->DrawItem(Tile);
			}
			const FString Title = FText::FromString(NWText::Get(SectionDefs[Idx].Key)).ToUpper().ToString();
			DrawGameText(Canvas, Title, H.Arrow.Max.X + 2.f * S, H.Arrow.Min.Y + 1.f * S, 27.f, InvTanNew, TEXT("pica_regular"));
		}
		FillRect(Canvas, FBox2f(FVector2f(Left, H.Arrow.Max.Y + 3.f * S), FVector2f(Left + ColumnWidth * S, H.Arrow.Max.Y + 4.f * S)),
			FLinearColor(InvLine.R, InvLine.G, InvLine.B, 0.35f));
		// 2ª linha: peso da seção (Pica 27 #b2aea1 com icon_weight 12x12) e o botão de mover a seção.
		float W = 0.f;
		for (int32 Slot : SectionSlots(H.Class, bStorageItems))
		{
			FSlotRef R;
			R.Slot = Slot;
			R.bStorage = bStorageItems;
			const FNWItem* I = ItemOf(R);
			if (const FNWItemDef* D = I ? FNWItemCatalog::Get().Find(I->ItemId) : nullptr)
			{
				W += D->Weight * I->Quantity;
			}
		}
		const FString WT = FString::Printf(TEXT("%.1f"), W);
		const bool bTransfer = bStorage && (bStorageItems || !bLoot);   // bolsa no chão: só pegar
		const float WR = bTransfer ? H.Transfer.Min.X - 8.f * S : H.Transfer.Max.X;
		DrawGameText(Canvas, WT, WR, H.Transfer.Min.Y, 24.f, InvGray, TEXT("pica_regular"), true);
		const float IX = WR - GameTextWidth(WT, 24.f, TEXT("pica_regular")) - 16.f * S;
		DrawTexture(Canvas, FString(MiscDir) + TEXT("icon_weight.dds"), FBox2f(FVector2f(IX, H.Transfer.Min.Y + 9.f * S), FVector2f(IX + 12.f * S, H.Transfer.Min.Y + 21.f * S)), InvGray);
		if (bTransfer)
		{
			// TransferAll: icon_smallArrowNarrow, apontando para o outro lado (espelhado no inventário).
			const bool bHot = H.Transfer.IsInside(M);
			const FVector2f C = H.Transfer.GetCenter();
			DrawTile(Canvas, FNWUILayout::GameTexture(FString(MiscDir) + TEXT("icon_smallarrownarrow.dds")), FBox2f(C - FVector2f(12.f * S), C + FVector2f(12.f * S)),
				bHot ? FLinearColor::White : InvTanBody, false, FVector2f(bStorageItems ? 0.f : 1.f, 0.f), FVector2f(bStorageItems ? 1.f : 0.f, 1.f));
		}
	}
	// Peças.
	for (const FSlotRef& R : Slots)
	{
		if (R.Container != ENWItemContainer::Bag || R.bStorage != bStorageItems)
		{
			continue;
		}
		const FNWItem* Item = ItemOf(R);
		const FNWItemDef* D = Item ? FNWItemCatalog::Get().Find(Item->ItemId) : nullptr;
		const bool bSource = bDragging && Pressed.Source() == R.Source() && Pressed.Slot == R.Slot;
		DrawItemTile(Canvas, R.Full, Item, D && D->bRound, bSource ? 0.5f : 1.f, Hover == &R && !bDragging, L.Sort);
	}
	ClipRect = FBox2f(ForceInit);
	// Barra de rolagem fina quando a lista passa da área.
	const float AreaH = L.Area.GetSize().Y / S;
	if (L.ContentH > AreaH)
	{
		const float X = L.Area.Max.X - 6.f * S;
		FCanvasTileItem Track(FVector2D(X, L.Area.Min.Y), FVector2D(3.f * S, L.Area.GetSize().Y), FLinearColor(1.f, 1.f, 1.f, 0.06f));
		Track.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Track);
		const float TH = L.Area.GetSize().Y * AreaH / L.ContentH;
		const float TY = L.Area.Min.Y + (L.Area.GetSize().Y - TH) * (L.Scroll / FMath::Max(1.f, L.ContentH - AreaH));
		FCanvasTileItem Thumb(FVector2D(X, TY), FVector2D(3.f * S, TH), InvTanBody);
		Canvas->DrawItem(Thumb);
	}
	// Alvo de soltar (containerareadroptarget): dropzoneglow acende com o mouse em cima, vindo do outro lado.
	const bool bFromOther = bStorageItems ? Pressed.Source() != ESource::Storage && !bLoot : Pressed.Source() != ESource::Bag;
	if (bDragging && bFromOther && L.Window.IsInside(M))
	{
		DrawDropZone(Canvas, L.Window, InvTan);
	}
}

void FNWInventoryMenu::DrawEquipment(UCanvas* Canvas, const FVector2D& Mouse, const FSlotRef* Hover)
{
	const ANWCharacter* Me = Character.Get();
	const UNWInventoryComponent* Inv = Me->Inventory;
	const float S = UIScale;
	auto Box = [&](float X, float Y, float W, float H) { return FBox2f((EquipOrigin + FVector2f(X, Y)) * S, (EquipOrigin + FVector2f(X + W, Y + H)) * S); };
	const FString Backdrop = FString(EquipDir) + TEXT("equipmentstatsbackdrop.dds");
	const FLinearColor Half(1.f, 1.f, 1.f, 0.5f);

	// Abas em ícone.
	static const TCHAR* Tabs[] = {TEXT("tabweaponsarmor"), TEXT("tabtools")};
	for (int32 t = 0; t < 2; ++t)
	{
		const bool bHot = TabRects[t].IsInside(FVector2f(Mouse));
		DrawTexture(Canvas, FString::Printf(TEXT("%s%s%s.dds"), EquipDir, Tabs[t], EquipTab == t ? TEXT("selected") : TEXT("unselected")),
			TabRects[t], FLinearColor(1.f, 1.f, 1.f, EquipTab == t || bHot ? 1.f : 0.75f));
	}

	// Fundos pretos por grupo (equipmentstatsbackdrop a 0,5) e espaços do jogo que o port não tem, vazios.
	auto EmptySlot = [&](float X, float Y, const TCHAR* IconName)
	{
		DrawTexture(Canvas, FString(EquipDir) + TEXT("equipmentbg.dds"), Box(X - 2.f, Y - 1.f, 70.f, 69.f));
		DrawTexture(Canvas, FString::Printf(TEXT("%s%s.dds"), EquipDir, IconName), Box(X, Y, EquipCell, EquipCell), FLinearColor(1.f, 1.f, 1.f, 0.2f));
	};
	if (EquipTab == 0)
	{
		DrawTexture(Canvas, Backdrop, Box(22.f, 202.f, 98.f, 436.f), Half);
		DrawTexture(Canvas, Backdrop, Box(540.f, 202.f, 82.f, 228.f), Half);
		DrawTexture(Canvas, Backdrop, Box(540.f, 428.f, 82.f, 228.f), Half);
		for (const FEmptyPos& P : CombatEmpty)
		{
			EmptySlot(P.X, P.Y, P.Icon);
		}
	}
	else
	{
		DrawTexture(Canvas, Backdrop, Box(540.f, 338.f, 82.f, 366.f), Half);
		for (const FEmptyPos& P : ToolEmpty)
		{
			EmptySlot(P.X, P.Y, P.Icon);
		}
	}

	// Espaços do port: fundo, item (ou ícone vazio) e icon_item_selected nos compatíveis durante o arrasto.
	const FNWItem* Dragged = bDragging ? ItemOf(Pressed) : nullptr;
	for (const FSlotRef& R : Slots)
	{
		if (R.Container != ENWItemContainer::Equip)
		{
			continue;
		}
		const FVector2f P = R.Rect.Min / S - EquipOrigin;
		DrawTexture(Canvas, FString(EquipDir) + TEXT("equipmentbg.dds"), Box(P.X - 2.f, P.Y - 1.f, 70.f, 69.f));
		const FNWItem* Item = Inv->At(ENWItemContainer::Equip, R.Slot);
		const bool bSource = bDragging && Pressed.Source() == ESource::Equip && Pressed.Slot == R.Slot;
		if (Item)
		{
			DrawItemTile(Canvas, R.Full, Item, false, bSource ? 0.5f : 1.f, Hover == &R && !bDragging, 1);
		}
		else
		{
			DrawTexture(Canvas, FString::Printf(TEXT("%s%s.dds"), EquipDir, EmptyIcon(R.Slot)), R.Rect, FLinearColor(1.f, 1.f, 1.f, 0.2f));
		}
		if (Dragged && !bSource && Pressed.Source() == ESource::Bag && Accepts(R.Slot, *Dragged))
		{
			DrawTexture(Canvas, TEXT("lyshineui/images/icon_item_selected.dds"), Box(P.X - 3.5f, P.Y - 3.f, 70.f, 69.f));
		}
	}

	// Nível e gear score (Pica 48) no alto à direita.
	const UNWProgressionComponent* Prog = Me->Progression;
	DrawTexture(Canvas, TEXT("lyshineui/images/icons/objectives/reward_xp.dds"), Box(524.f, 103.f, 48.f, 48.f));
	DrawGameText(Canvas, FString::FromInt(Prog ? Prog->Level : 1), (EquipOrigin.X + 576.f) * S, 103.f * S, 48.f, InvTan, TEXT("pica_regular"));
	int32 Sum = 0, Count = 0;
	for (int32 Slot = 0; Slot < NWEquip::Count; ++Slot)   // armas, armadura e joias (ferramentas não contam)
	{
		if (const FNWItem* I = NWEquip::IsTool(Slot) ? nullptr : Inv->At(ENWItemContainer::Equip, Slot); I && I->GearScore > 0)
		{
			Sum += I->GearScore;
			++Count;
		}
	}
	DrawTexture(Canvas, FString(MiscDir) + TEXT("icon_gearscore_dropshadow.dds"), Box(528.f, 156.f, 40.f, 40.f));
	DrawGameText(Canvas, Count ? FString::FromInt(Sum / Count) : TEXT("0"), (EquipOrigin.X + 576.f) * S, 150.f * S, 48.f, InvTan, TEXT("pica_regular"));

	// Atributos (equipmentstatrow: rótulo Nimbus médio 25 #c6b694, valor Pica 28), "+" com pontos livres.
	for (FBox2f& PR : PointRects)
	{
		PR = FBox2f(ForceInit);
	}
	if (Prog && EquipTab == 0)
	{
		DrawTexture(Canvas, FString(EquipDir) + TEXT("equipmentstatsbackdrop2023.dds"), Box(10.f, 810.f, 174.f, 136.f), FLinearColor(1.f, 1.f, 1.f, 0.29f));
		static const TCHAR* Short[] = {TEXT("ui_strength_short"), TEXT("ui_dexterity_short"), TEXT("ui_intelligence_short"), TEXT("ui_focus_short"), TEXT("ui_constitution_short")};
		const int32 Free = Prog->UnspentPoints();
		for (int32 a = 0; a < 5; ++a)
		{
			const float Y = 818.f + a * 24.f;
			DrawGameText(Canvas, FText::FromString(NWText::Get(Short[a])).ToUpper().ToString(), (EquipOrigin.X + 32.f) * S, Y * S, 25.f, InvTanNew, TEXT("nimbus_medium"));
			DrawGameText(Canvas, FString::FromInt(Prog->Attribute(ENWAttribute(a))), (EquipOrigin.X + 106.f) * S, (Y - 2.f) * S, 28.f, InvTan, TEXT("pica_regular"));
			if (Free > 0)
			{
				PointRects[a] = Box(150.f, Y + 2.f, 20.f, 20.f);
				const bool bHot = PointRects[a].IsInside(FVector2f(Mouse));
				DrawTexture(Canvas, TEXT("lyshineui/images/slices/hint/hintframefilled.dds"), PointRects[a], FLinearColor(1.f, 1.f, 1.f, bHot ? 1.f : 0.8f));
				DrawGameText(Canvas, TEXT("+"), PointRects[a].GetCenter().X - 5.f * S, (Y - 1.f) * S, 24.f, InvGold, TEXT("pica_regular"));
			}
		}
	}

	// Alvo universal (equipmentuniversaldroptarget): acende com o mouse em cima ao arrastar da bolsa; vermelho se não equipa.
	if (Dragged && Pressed.Source() == ESource::Bag && EquipArea.IsInside(FVector2f(Mouse)))
	{
		const FNWItemDef* D = FNWItemCatalog::Get().Find(Dragged->ItemId);
		const bool bOk = D && NWEquip::SlotForSet(D->Set) != INDEX_NONE;
		DrawDropZone(Canvas, EquipArea, bOk ? InvTan : FLinearColor(FColor(0xFF, 0x2E, 0x2E)));
	}
}

void FNWInventoryMenu::DrawDrag(UCanvas* Canvas, const FVector2D& Mouse)
{
	// itemdraggable OnDragStart: cópia 75x75 centrada no cursor.
	const FNWItem* Item = ItemOf(Pressed);
	const FNWItemDef* D = Item ? FNWItemCatalog::Get().Find(Item->ItemId) : nullptr;
	if (!D)
	{
		return;
	}
	const FVector2f M(Mouse);
	const float H = DragSize * 0.5f * UIScale;
	DrawItemTile(Canvas, FBox2f(M - FVector2f(H), M + FVector2f(H)), Item, D->bRound && Pressed.Source() != ESource::Equip, 1.f, false, 1);
}

void FNWInventoryMenu::Draw(UCanvas* Canvas, const FVector2D& Mouse)
{
	const ANWCharacter* Me = Character.Get();
	if (!Me || !Me->Inventory)
	{
		return;
	}
	DrawCanvas = Canvas;
	UIScale = Canvas->ClipY / 1080.f;
	Center = FVector2f(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

	// Escurece a tela de leve, deixando o personagem no meio à vista.
	FCanvasTileItem Dim(FVector2D::ZeroVector, FVector2D(Canvas->ClipX, Canvas->ClipY), FLinearColor(0.f, 0.f, 0.f, 0.25f));
	Dim.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Dim);

	Window.DrawOne(Canvas, TEXT("inventory"));   // só o layout: o Content está escondido
	Layout(Canvas);
	DrawWindow(Canvas, Lists[0], InvOffsetX, false, Mouse);
	if (bStorage)
	{
		DrawWindow(Canvas, Lists[1], StorageShift, true, Mouse);
	}
	const FSlotRef* Hover = SlotAt(Mouse);
	DrawList(Canvas, Lists[0], false, Mouse, Hover);
	if (bStorage)
	{
		DrawList(Canvas, Lists[1], true, Mouse, Hover);
	}
	DrawEquipment(Canvas, Mouse, Hover);
	if (bDragging)
	{
		DrawGround(Canvas, Mouse);
		DrawDrag(Canvas, Mouse);
		return;
	}
	if (Popup != EPopup::None)
	{
		DrawPopup(Canvas, Mouse);
		return;
	}
	// Tooltip depois de 0,1 s parado sobre a peça.
	const void* Key = Hover ? static_cast<const void*>(ItemOf(*Hover)) : nullptr;
	if (Key != HoverKey)
	{
		HoverKey = Key;
		HoverSince = FPlatformTime::Seconds();
	}
	if (Hover && Key && FPlatformTime::Seconds() - HoverSince >= 0.1)
	{
		const FNWItem* Item = ItemOf(*Hover);
		const FBox2f R = DrawTooltip(Canvas, *Item, Mouse, Hover->Source() == ESource::Equip);
		// Comparação: item da bolsa que vai num espaço ocupado mostra o equipado ao lado.
		const FNWItemDef* D = FNWItemCatalog::Get().Find(Item->ItemId);
		const int32 EquipSlot = D && Hover->Source() == ESource::Bag ? NWEquip::SlotForSet(D->Set) : INDEX_NONE;
		if (const FNWItem* Equipped = EquipSlot != INDEX_NONE ? Me->Inventory->At(ENWItemContainer::Equip, EquipSlot) : nullptr; Equipped && R.bIsValid)
		{
			const float Gap = 14.f * UIScale, W = R.GetSize().X;
			const float CX = R.Min.X - Gap - W >= 0.f ? R.Min.X - Gap - W : R.Max.X + Gap;
			DrawTooltip(Canvas, *Equipped, FVector2D(Mouse.X, R.Min.Y + 40.f * UIScale), true, CX);
		}
	}
}

// ------------------------------------------------------------------------------------------------------------ entrada

void FNWInventoryMenu::Click(const FVector2D& Mouse, bool bRight)
{
	ANWCharacter* Me = Character.Get();
	if (!Me || !Me->Inventory)
	{
		return;
	}
	const FVector2f M(Mouse);
	if (FSlateApplication::IsInitialized() && ((SearchEdit[0].IsValid() && SearchEdit[0]->HasKeyboardFocus()) || (SearchEdit[1].IsValid() && SearchEdit[1]->HasKeyboardFocus())))
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
	if (Popup != EPopup::None)
	{
		if (!bRight)
		{
			PopupClick(Mouse);
		}
		return;
	}
	if (!bRight)
	{
		for (int32 a = 0; a < 5; ++a)
		{
			if (Me->Progression && PointRects[a].bIsValid && PointRects[a].IsInside(M))
			{
				Me->Progression->ServerSpendPoint(ENWAttribute(a));
				return;
			}
		}
		for (int32 t = 0; t < 2; ++t)
		{
			if (TabRects[t].IsInside(M))
			{
				EquipTab = t;
				return;
			}
		}
		if (const int32 T = ShowsTabs() ? TabAt(M) : INDEX_NONE; T != INDEX_NONE)
		{
			// Clique troca de aba; clique duplo na aba aberta renomeia.
			const double Now = FPlatformTime::Seconds();
			if (T == StorageTab && LastTabClicked == T && Now - LastTabClick < 0.4)
			{
				RenameTab = T;
				if (RenameEdit.IsValid() && FSlateApplication::IsInitialized())
				{
					const TArray<FString>* Names = Me->Interactor ? &Me->Interactor->StorageTabNames : nullptr;
					RenameEdit->SetText(FText::FromString(Names && Names->IsValidIndex(T) ? (*Names)[T] : FString()));
					FSlateApplication::Get().SetKeyboardFocus(RenameEdit);
				}
			}
			else if (T != StorageTab)
			{
				StorageTab = T;
				Lists[1].Scroll = 0.f;
				RenameTab = INDEX_NONE;
			}
			LastTabClicked = T;
			LastTabClick = Now;
			return;
		}
		for (int32 l = 0; l < (bStorage ? 2 : 1); ++l)
		{
			FList& L = Lists[l];
			for (int32 i = 0; i < 4; ++i)
			{
				if (L.SortRects[i].IsInside(M))
				{
					// inventoryfilter: o modo ativo inverte; outro modo volta à direção padrão.
					L.SortDir = L.Sort == i + 1 ? -L.SortDir : 1;
					L.Sort = i + 1;
					return;
				}
			}
			for (const FSectionHit& H : L.Sections)
			{
				if (!L.Area.IsInside(M))
				{
					continue;
				}
				if (H.Arrow.IsInside(M))
				{
					if (!L.Collapsed.Remove(H.Class))
					{
						L.Collapsed.Add(H.Class);
					}
					return;
				}
				if (bStorage && (l == 1 || !bLoot) && H.Transfer.IsInside(M))
				{
					// TransferAll: a seção inteira para o outro lado.
					ContainerMoveMany(l == 0, SectionSlots(H.Class, l == 1));
					return;
				}
			}
		}
		if (bStorage && TakeAllRect.IsInside(M) && Me->Interactor)
		{
			if (bLoot)
			{
				if (LootBag.IsValid())
				{
					Me->Interactor->ServerLootTake(LootBag.Get());
				}
				return;
			}
			TArray<int32> All;
			for (const FNWItem& I : Me->Interactor->StorageItems)
			{
				if (InList(I, true))   // Pegar tudo da aba aberta
				{
					All.Add(I.Slot);
				}
			}
			Me->Interactor->ServerStorageMoveMany(false, All, uint8(StorageTab));
			return;
		}
	}
	const FSlotRef* S = SlotAt(Mouse);
	if (!S || !ItemOf(*S))
	{
		return;
	}
	if (bRight)
	{
		RightClick(*S);
		return;
	}
	// Ctrl + clique: dividir a pilha (InvokeStackSplitter; não abre com 1).
	if (ModifierDown(EKeys::LeftControl) || ModifierDown(EKeys::RightControl))
	{
		if (S->Source() != ESource::Equip && ItemOf(*S)->Quantity > 1)
		{
			OpenPopup(EPopup::Split, *S);
		}
		return;
	}
	// Shift + clique: guardar/retirar rápido.
	if (ModifierDown(EKeys::LeftShift) || ModifierDown(EKeys::RightShift))
	{
		QuickMove(*S);
		return;
	}
	// Apertou num item: vira arrasto quando o mouse andar (Hold).
	bPressed = true;
	bDragging = false;
	Pressed = *S;
	PressPos = Mouse;
}

void FNWInventoryMenu::Hold(const FVector2D& Mouse, bool bDown)
{
	if (IsOpen() && Popup == EPopup::Split)
	{
		bSliderDrag = bSliderDrag && bDown;
		if (bSliderDrag)
		{
			SetSliderFromMouse(Mouse.X);
		}
		return;
	}
	if (!IsOpen() || !bPressed)
	{
		return;
	}
	if (bDown)
	{
		if (!bDragging && FVector2D::Distance(Mouse, PressPos) > DragStart * UIScale && ItemOf(Pressed))
		{
			bDragging = true;
		}
		return;
	}
	if (bDragging)
	{
		Drop(Mouse);
	}
	bPressed = bDragging = false;
}

void FNWInventoryMenu::ShotDrag(int32 Source, int32 Slot)
{
	Pressed = FSlotRef();
	Pressed.Container = Source == 1 ? ENWItemContainer::Equip : ENWItemContainer::Bag;
	Pressed.bStorage = Source == 2;
	Pressed.Slot = Slot;
	bPressed = bDragging = true;
}

void FNWInventoryMenu::Drop(const FVector2D& Mouse)
{
	ANWCharacter* Me = Character.Get();
	const FNWItem* Item = ItemOf(Pressed);
	if (!Me || !Me->Inventory || !Item)
	{
		return;
	}
	UNWInventoryComponent* Inv = Me->Inventory;
	const FVector2f M(Mouse);
	const FSlotRef* Target = SlotAt(Mouse);
	switch (Pressed.Source())
	{
	case ESource::Bag:
		if (const FNWItem* TI = Target && Target->Source() != ESource::Storage ? ItemOf(*Target) : nullptr;
			TI && FNWItemRules::Get().Gems.Contains(Item->ItemId) && !(Target->Source() == ESource::Bag && Target->Slot == Pressed.Slot))
		{
			// Gema ou vidro rúnico solto num item: confirma antes de acoplar (o servidor confere se encaixa).
			const FNWItemDef* TD = FNWItemCatalog::Get().Find(TI->ItemId);
			if (TD && FNWItemRules::Get().CanSocket(*TD, *TI, Item->ItemId))
			{
				if (TI->Gem.IsNone())
				{
					PopupGemSlot = Pressed.Slot;
					OpenPopup(EPopup::Socket, *Target);
				}
				else
				{
					Inv->ServerSocket(Pressed.Slot, Target->Container, Target->Slot);   // recusa e avisa: retirar antes
				}
			}
		}
		else if (Target && Target->Source() == ESource::Equip && Accepts(Target->Slot, *Item))
		{
			Inv->ServerMove(ENWItemContainer::Bag, Pressed.Slot, ENWItemContainer::Equip, Target->Slot);
		}
		else if (const int32 T = ShowsTabs() ? TabAt(M) : INDEX_NONE; T != INDEX_NONE)
		{
			// Soltar na aba: guarda direto nela.
			const int32 Was = StorageTab;
			StorageTab = T;
			ContainerMove(true, Pressed.Slot, Item->Quantity);
			StorageTab = Was;
		}
		else if (bStorage && Lists[1].Window.IsInside(M))
		{
			ContainerMove(true, Pressed.Slot, Item->Quantity);   // na bolsa no chão não guarda
		}
		else if (!bStorage && GroundZone().IsInside(M))
		{
			// Solta a pilha numa bolsa no chão (ANWLootBag), como o grounddroptarget do jogo.
			Inv->ServerDrop(Pressed.Slot, Item->Quantity);
		}
		else if (EquipArea.IsInside(M))
		{
			// Alvo universal: equipa no espaço do item (EquipItemToBestSlot).
			const FNWItemDef* D = FNWItemCatalog::Get().Find(Item->ItemId);
			if (const int32 To = D ? NWEquip::SlotForSet(D->Set) : INDEX_NONE; To != INDEX_NONE)
			{
				Inv->ServerMove(ENWItemContainer::Bag, Pressed.Slot, ENWItemContainer::Equip, To);
				EquipTab = NWEquip::IsTool(To) ? 1 : 0;
			}
		}
		break;
	case ESource::Equip:
		if (Lists[0].Window.IsInside(M))
		{
			if (const int32 B = FreeBagSlot(); B != INDEX_NONE)
			{
				Inv->ServerMove(ENWItemContainer::Equip, Pressed.Slot, ENWItemContainer::Bag, B);
			}
		}
		break;
	case ESource::Storage:
		if (const int32 T = ShowsTabs() ? TabAt(M) : INDEX_NONE; T != INDEX_NONE && Me->Interactor)
		{
			Me->Interactor->ServerStorageSetTab(Pressed.Slot, uint8(T));   // passa a pilha para a aba
		}
		else if (Lists[0].Window.IsInside(M))
		{
			ContainerMove(false, Pressed.Slot, Item->Quantity);
		}
		break;
	}
}

void FNWInventoryMenu::RightClick(const FSlotRef& S)
{
	// Atalho do port (o jogo abre o menu de ações): com o armazém, move; senão equipa, usa ou desequipa.
	ANWCharacter* Me = Character.Get();
	const FNWItem* Item = ItemOf(S);
	const FNWItemDef* D = Item ? FNWItemCatalog::Get().Find(Item->ItemId) : nullptr;
	if (!Me || !Me->Inventory || !D)
	{
		return;
	}
	UNWInventoryComponent* Inv = Me->Inventory;
	if (S.Source() != ESource::Storage && !Item->Gem.IsNone() && (ModifierDown(EKeys::LeftShift) || ModifierDown(EKeys::RightShift)))
	{
		OpenPopup(EPopup::Unsocket, S);   // Shift + clique direito: retirar a gema
	}
	else if (S.Source() == ESource::Storage || (bStorage && !bLoot && S.Source() == ESource::Bag))
	{
		QuickMove(S);
	}
	else if (S.Source() == ESource::Equip)
	{
		if (const int32 B = FreeBagSlot(); B != INDEX_NONE)
		{
			Inv->ServerMove(ENWItemContainer::Equip, S.Slot, ENWItemContainer::Bag, B);
		}
	}
	else if (D->Heal > 0.f || D->HealPercent > 0.f)
	{
		Inv->ServerUse(S.Slot);
	}
	else if (const int32 To = NWEquip::SlotForSet(D->Set); To != INDEX_NONE)
	{
		Inv->ServerMove(ENWItemContainer::Bag, S.Slot, ENWItemContainer::Equip, To);
		EquipTab = NWEquip::IsTool(To) ? 1 : 0;
	}
}

void FNWInventoryMenu::QuickMove(const FSlotRef& S)
{
	ANWCharacter* Me = Character.Get();
	const FNWItem* Item = ItemOf(S);
	if (!Me || !Me->Interactor || !Item || !bStorage)
	{
		return;
	}
	if (S.Source() == ESource::Bag || S.Source() == ESource::Storage)
	{
		ContainerMove(S.Source() == ESource::Bag, S.Slot, Item->Quantity);
	}
}

void FNWInventoryMenu::Wheel(float Delta, const FVector2D& Mouse)
{
	for (int32 l = 0; l < (bStorage ? 2 : 1); ++l)
	{
		FList& L = Lists[l];
		if (L.Window.IsInside(FVector2f(Mouse)))
		{
			const float Max = FMath::Max(0.f, L.ContentH - L.Area.GetSize().Y / UIScale);
			L.Scroll = FMath::Clamp(L.Scroll - Delta * CellStepY, 0.f, Max);
		}
	}
}

// ------------------------------------------------------------------------------------------------------------ popup e chão

FBox2f FNWInventoryMenu::GroundZone() const
{
	// grounddroptarget: com o inventário sozinho fica em x 1576..1899 de 1920 (610 à direita do Content).
	return FBox2f(FVector2f(InvLeft + 610.f, 62.f) * UIScale, FVector2f(InvLeft + 933.f, 995.f) * UIScale);
}

void FNWInventoryMenu::DrawGround(UCanvas* Canvas, const FVector2D& Mouse)
{
	if (bStorage || Pressed.Source() != ESource::Bag)
	{
		return;   // só arrastando da bolsa e sem contêiner aberto
	}
	const float S = UIScale;
	const FBox2f Z = GroundZone();
	const bool bHot = Z.IsInside(FVector2f(Mouse));
	if (bHot)
	{
		DrawDropZone(Canvas, Z, InvTan, false);
	}
	// @inv_dropTargetMessage em Pica itálico 30 #dddddd entre dois dropitemtrim 220x27 (o de baixo invertido).
	const FString T = NWText::Get(TEXT("inv_droptargetmessage"));
	const float CX = Z.GetCenter().X, CY = Z.GetCenter().Y;
	const FLinearColor Text(FColor(0xDD, 0xDD, 0xDD));
	UTexture2D* Trim = FNWUILayout::GameTexture(TEXT("lyshineui/images/inventory/dropitemtrim.dds"));
	const FLinearColor TrimTint(1.f, 1.f, 1.f, 1.f);
	DrawTile(Canvas, Trim, FBox2f(FVector2f(CX - 110.f * S, CY - 50.f * S), FVector2f(CX + 110.f * S, CY - 23.f * S)), TrimTint);
	DrawTile(Canvas, Trim, FBox2f(FVector2f(CX - 110.f * S, CY + 23.f * S), FVector2f(CX + 110.f * S, CY + 50.f * S)), TrimTint, false, FVector2f(0.f, 1.f), FVector2f(1.f, 0.f));
	DrawGameText(Canvas, T, CX - GameTextWidth(T, 30.f, TEXT("pica_italic")) * 0.5f, CY - 17.f * S, 30.f, bHot ? FLinearColor::White : Text, TEXT("pica_italic"));
}

void FNWInventoryMenu::OpenPopup(EPopup Kind, const FSlotRef& S)
{
	const FNWItem* Item = ItemOf(S);
	if (!Item)
	{
		return;
	}
	Popup = Kind;
	PopupSlot = S;
	PopupMax = Item->Quantity;
	PopupQty = Kind == EPopup::Split ? PopupMax - PopupMax / 2 : PopupMax;   // popup.lua: q − floor(q/2)
	bSliderDrag = false;
	PopupButtons.Reset();
	auto Add = [this](EAction A, const TCHAR* Key, bool bEnabled = true)
	{
		FPopupButton& B = PopupButtons.AddDefaulted_GetRef();
		B.Action = A;
		B.Label = NWText::Get(Key);
		B.bEnabled = bEnabled;
	};
	Add(EAction::Cancel, TEXT("ui_cancel"));
	if (Kind == EPopup::Destroy)
	{
		Add(EAction::Destroy, TEXT("ui_confirm"));
	}
	else if (Kind == EPopup::Socket || Kind == EPopup::Unsocket)
	{
		// Retirar (ou trocar) paga o custo; sem ele o botão fica apagado.
		int32 Qty = 0;
		const FName Cost = Item->Gem.IsNone() ? NAME_None : FNWItemRules::Get().UnsocketCost(Item->Gem, Qty);
		const ANWCharacter* Me = Character.Get();
		const bool bPay = Cost.IsNone() || (Me && Me->Inventory && Me->Inventory->CountOf(Cost) >= Qty);
		Add(Kind == EPopup::Socket ? EAction::Socket : EAction::Unsocket, TEXT("ui_confirm"), bPay);
	}
	else if (bStorage && !(bLoot && S.Source() == ESource::Bag))
	{
		Add(EAction::Move, S.Source() == ESource::Storage ? TEXT("ui_tooltip_take") : TEXT("ui_move"));
	}
	else
	{
		Add(EAction::Drop, TEXT("ui_tooltip_drop"));
	}
}

void FNWInventoryMenu::SetSliderFromMouse(float X)
{
	if (SliderRect.bIsValid && PopupMax > 1)
	{
		const float T = FMath::Clamp((X - SliderRect.Min.X) / FMath::Max(1.f, SliderRect.GetSize().X), 0.f, 1.f);
		PopupQty = FMath::Clamp(1 + FMath::RoundToInt(T * (PopupMax - 1)), 1, PopupMax);
	}
}

void FNWInventoryMenu::RunAction(EAction Action)
{
	ANWCharacter* Me = Character.Get();
	const FNWItem* Item = ItemOf(PopupSlot);
	const EPopup Kind = Popup;
	Popup = EPopup::None;
	if (!Me || !Item || Action == EAction::Cancel)
	{
		return;
	}
	switch (Action)
	{
	case EAction::Move:
		ContainerMove(PopupSlot.Source() == ESource::Bag, PopupSlot.Slot, PopupQty);
		break;
	case EAction::Drop:
		// Soltar a parte escolhida numa bolsa no chão (ANWLootBag).
		if (Me->Inventory && PopupSlot.Source() == ESource::Bag)
		{
			Me->Inventory->ServerDrop(PopupSlot.Slot, PopupQty);
		}
		break;
	case EAction::Destroy:
		if (Me->Inventory && PopupSlot.Source() == ESource::Bag)
		{
			Me->Inventory->ServerDestroy(PopupSlot.Slot, PopupQty);
		}
		break;
	case EAction::Socket:
		if (Me->Inventory && PopupGemSlot != INDEX_NONE)
		{
			Me->Inventory->ServerSocket(PopupGemSlot, PopupSlot.Container, PopupSlot.Slot);
		}
		break;
	case EAction::Unsocket:
		if (Me->Inventory)
		{
			Me->Inventory->ServerUnsocket(PopupSlot.Container, PopupSlot.Slot);
		}
		break;
	default:
		break;
	}
	(void)Kind;
}

bool FNWInventoryMenu::PopupClick(const FVector2D& Mouse)
{
	const FVector2f M(Mouse);
	for (const FPopupButton& B : PopupButtons)
	{
		if (B.Rect.IsInside(M))
		{
			if (B.bEnabled)
			{
				RunAction(B.Action);
			}
			return true;
		}
	}
	if (Popup == EPopup::Split && SliderRect.ExpandBy(FVector2f(14.f * UIScale)).IsInside(M))
	{
		bSliderDrag = true;
		SetSliderFromMouse(Mouse.X);
	}
	return true;
}

void FNWInventoryMenu::DrawPopup(UCanvas* Canvas, const FVector2D& Mouse)
{
	const FNWItem* Item = ItemOf(PopupSlot);
	const FNWItemDef* D = Item ? FNWItemCatalog::Get().Find(Item->ItemId) : nullptr;
	if (!D)
	{
		Popup = EPopup::None;
		return;
	}
	PopupMax = Item->Quantity;
	PopupQty = FMath::Clamp(PopupQty, 1, PopupMax);
	const float S = UIScale;
	const FVector2f M(Mouse);
	// popup.lua: fundo escurece a 0,8; caixa centrada.
	FillRect(Canvas, FBox2f(FVector2f(0.f), FVector2f(Canvas->ClipX, Canvas->ClipY)), FLinearColor(0.f, 0.f, 0.f, 0.6f));
	const bool bSplit = Popup == EPopup::Split;
	const bool bGem = Popup == EPopup::Socket || Popup == EPopup::Unsocket;
	const FNWItem* GemStack = Popup == EPopup::Socket ? ItemOf(FSlotRef{ENWItemContainer::Bag, PopupGemSlot}) : nullptr;
	if (Popup == EPopup::Socket && !GemStack)
	{
		Popup = EPopup::None;
		return;
	}
	const FVector2f Size = FVector2f(560.f, bSplit ? 360.f : 350.f) * S;
	const FVector2f C(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const FBox2f Box(C - Size * 0.5f, C + Size * 0.5f);
	DrawTexture(Canvas, TEXT("lyshineui/images/popup/popupbg.dds"), Box);
	FillRect(Canvas, Box, FLinearColor(0.f, 0.f, 0.f, 0.8f));
	DrawDropZone(Canvas, Box, FLinearColor(InvTanBody.R, InvTanBody.G, InvTanBody.B, 0.5f));

	const TCHAR* TitleKey = bSplit ? TEXT("ui_split_stack") : Popup == EPopup::Unsocket ? TEXT("nw.item.gem_remove")
		: Popup == EPopup::Socket ? TEXT("ui_attach_gem") : TEXT("ui_destroybounditem");
	const FString Title = FText::FromString(NWText::Get(TitleKey)).ToUpper().ToString();
	DrawGameText(Canvas, Title, C.X - GameTextWidth(Title, 32.f, TEXT("pica_regular")) * 0.5f, Box.Min.Y + 22.f * S, 32.f, InvTan, TEXT("pica_regular"));
	FillRect(Canvas, FBox2f(FVector2f(Box.Min.X + 40.f * S, Box.Min.Y + 66.f * S), FVector2f(Box.Max.X - 40.f * S, Box.Min.Y + 67.f * S)),
		FLinearColor(InvLine.R, InvLine.G, InvLine.B, 0.35f));

	const float Tile = 75.f * S;   // ItemLayout a 1,25
	FNWItem Copy = *Item;
	auto TileAt = [&](float X, float Y, int32 Qty)
	{
		Copy.Quantity = Qty;
		DrawItemTile(Canvas, FBox2f(FVector2f(X - Tile * 0.5f, Y), FVector2f(X + Tile * 0.5f, Y + Tile)), &Copy, D->bRound, 1.f, false, 1);
		const FString W = FString::Printf(TEXT("%.1f"), D->Weight * Qty);
		const float TW = GameTextWidth(W, 24.f, TEXT("pica_regular"));
		DrawTexture(Canvas, FString(MiscDir) + TEXT("icon_weight.dds"), FBox2f(FVector2f(X - TW * 0.5f - 9.f * S, Y + Tile + 15.f * S), FVector2f(X - TW * 0.5f + 3.f * S, Y + Tile + 27.f * S)), InvGray);
		DrawGameText(Canvas, W, X - TW * 0.5f + 7.f * S, Y + Tile + 8.f * S, 24.f, InvGray, TEXT("pica_regular"));
	};
	if (bGem)
	{
		// Item à esquerda, gema à direita (a que entra ou a que sai); embaixo o efeito da gema e o custo de retirar.
		SliderRect = FBox2f(ForceInit);
		const FName GemId = GemStack ? GemStack->ItemId : Item->Gem;
		const float TY = Box.Min.Y + 86.f * S;
		DrawItemTile(Canvas, FBox2f(FVector2f(C.X - 110.f * S - Tile * 0.5f, TY), FVector2f(C.X - 110.f * S + Tile * 0.5f, TY + Tile)), Item, D->bRound, 1.f, false, 1);
		FNWItem GemCopy;
		GemCopy.ItemId = GemId;
		GemCopy.Quantity = 1;
		const FNWItemDef* GD = FNWItemCatalog::Get().Find(GemId);
		DrawItemTile(Canvas, FBox2f(FVector2f(C.X + 110.f * S - Tile * 0.5f, TY), FVector2f(C.X + 110.f * S + Tile * 0.5f, TY + Tile)), &GemCopy, GD && GD->bRound, 1.f, false, 1);
		const float AX = Popup == EPopup::Socket ? 1.f : -1.f;   // seta: gema entra no item (socket) ou sai (retirar)
		FCanvasTriangleItem Tri(FVector2D(C.X - 12.f * S * AX, TY + 37.f * S), FVector2D(C.X + 8.f * S * AX, TY + 25.f * S),
			FVector2D(C.X + 8.f * S * AX, TY + 49.f * S), GWhiteTexture);
		Tri.SetColor(InvTanBody);
		Canvas->DrawItem(Tri);
		float LY = TY + Tile + 14.f * S;
		auto Line = [&](const FString& T, const FLinearColor& Color, const TCHAR* Font)
		{
			DrawGameText(Canvas, T, C.X - GameTextWidth(T, 23.f, Font) * 0.5f, LY, 23.f, Color, Font);
			LY += 28.f * S;
		};
		Line(GD ? GD->Name : GemId.ToString(), InvTan, TEXT("nimbus_medium"));
		Line(FString::Join(FNWItemRules::Get().GemLines(GemId), TEXT("  ")), InvGray, TEXT("nimbus_regular"));
		int32 Qty = 0;
		const FName Cost = Item->Gem.IsNone() ? NAME_None : FNWItemRules::Get().UnsocketCost(Item->Gem, Qty);
		if (!Cost.IsNone())
		{
			const FNWItemDef* CD = FNWItemCatalog::Get().Find(Cost);
			const ANWCharacter* Me = Character.Get();
			const int32 Have = Me && Me->Inventory ? Me->Inventory->CountOf(Cost) : 0;
			Line(NWText::Format(TEXT("nw.item.gem_cost_line"), {FString::FromInt(Qty), CD ? CD->Name : Cost.ToString(), FString::FromInt(Have)}),
				Have >= Qty ? InvGray : FLinearColor(0.85f, 0.3f, 0.25f), TEXT("nimbus_regular"));
		}
	}
	else if (bSplit)
	{
		// Fica (esquerda) e leva (direita), slider de 1 até a pilha.
		TileAt(C.X - 110.f * S, Box.Min.Y + 86.f * S, PopupMax - PopupQty);
		TileAt(C.X + 110.f * S, Box.Min.Y + 86.f * S, PopupQty);
		FCanvasTriangleItem Tri(FVector2D(C.X + 12.f * S, Box.Min.Y + 123.f * S), FVector2D(C.X - 8.f * S, Box.Min.Y + 111.f * S),
			FVector2D(C.X - 8.f * S, Box.Min.Y + 135.f * S), GWhiteTexture);
		Tri.SetColor(InvTanBody);
		Canvas->DrawItem(Tri);
		SliderRect = FBox2f(FVector2f(Box.Min.X + 70.f * S, Box.Min.Y + 230.f * S), FVector2f(Box.Max.X - 70.f * S, Box.Min.Y + 236.f * S));
		const float T = PopupMax > 1 ? float(PopupQty - 1) / float(PopupMax - 1) : 1.f;
		const float HX = SliderRect.Min.X + T * SliderRect.GetSize().X;
		FillRect(Canvas, SliderRect, FLinearColor(1.f, 1.f, 1.f, 0.12f));
		FillRect(Canvas, FBox2f(SliderRect.Min, FVector2f(HX, SliderRect.Max.Y)), InvTanBody);
		const bool bHot = bSliderDrag || FBox2f(FVector2f(HX - 14.f * S, SliderRect.Min.Y - 14.f * S), FVector2f(HX + 14.f * S, SliderRect.Max.Y + 14.f * S)).IsInside(M);
		DrawTexture(Canvas, bHot ? TEXT("lyshineui/images/slices/slider/sliderhandlefocus.dds") : TEXT("lyshineui/images/slices/slider/sliderhandle2023.dds"),
			FBox2f(FVector2f(HX - 14.f * S, SliderRect.GetCenter().Y - 14.f * S), FVector2f(HX + 14.f * S, SliderRect.GetCenter().Y + 14.f * S)));
	}
	else
	{
		SliderRect = FBox2f(ForceInit);
		TileAt(C.X, Box.Min.Y + 86.f * S, PopupQty);
		const FString Name = D->Name;
		DrawGameText(Canvas, Name, C.X - GameTextWidth(Name, 25.f, TEXT("nimbus_medium")) * 0.5f, Box.Min.Y + 204.f * S, 25.f, InvTan, TEXT("nimbus_medium"));
	}

	// Botões 170x50 (buttonbg #2c251b + primarybuttonframe), centrados embaixo.
	const float BW = 170.f * S, BH = 50.f * S, Gap = 20.f * S;
	float X = C.X - (PopupButtons.Num() * BW + (PopupButtons.Num() - 1) * Gap) * 0.5f;
	for (FPopupButton& B : PopupButtons)
	{
		B.Rect = FBox2f(FVector2f(X, Box.Max.Y - BH - 26.f * S), FVector2f(X + BW, Box.Max.Y - 26.f * S));
		X += BW + Gap;
		const bool bHot = B.bEnabled && B.Rect.IsInside(M);
		const float A = B.bEnabled ? 1.f : 0.4f;
		DrawTexture(Canvas, TEXT("lyshineui/images/slices/button/buttonbg.dds"), B.Rect, FLinearColor(FColor(0x2C, 0x25, 0x1B)));
		DrawTexture(Canvas, TEXT("lyshineui/images/slices/button/primarybuttonframe.dds"), B.Rect, FLinearColor(1.f, 1.f, 1.f, (bHot ? 1.f : 0.75f) * A));
		DrawGameText(Canvas, B.Label, B.Rect.GetCenter().X - GameTextWidth(B.Label, 26.f, TEXT("pica_regular")) * 0.5f, B.Rect.Min.Y + 11.f * S, 26.f,
			bHot ? FLinearColor::White : FLinearColor(InvTan.R, InvTan.G, InvTan.B, A), TEXT("pica_regular"));
	}
}

void FNWInventoryMenu::ShotPopup(int32 Kind, int32 Source, int32 Slot, int32 Qty)
{
	FSlotRef R;
	R.Container = Source == 1 ? ENWItemContainer::Equip : ENWItemContainer::Bag;
	R.bStorage = Source == 2;
	R.Slot = Slot;
	// 3: acoplar (Qty = espaço da gema na bolsa); 4: retirar.
	if (Kind == 3)
	{
		PopupGemSlot = Qty;
	}
	OpenPopup(Kind == 2 ? EPopup::Destroy : Kind == 3 ? EPopup::Socket : Kind == 4 ? EPopup::Unsocket : EPopup::Split, R);
	if (Qty > 0 && Kind < 3)
	{
		PopupQty = Qty;
	}
}

// ------------------------------------------------------------------------------------------------------------ busca

namespace
{
	// Dois campos de texto transparentes por cima dos fundos de busca do canvas (bolsa e armazém).
	class SNWInventorySearch : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SNWInventorySearch) {}
			SLATE_ARGUMENT(FNWInventoryMenu*, Menu)
			SLATE_ARGUMENT(TFunction<bool()>, StorageShown)
		SLATE_END_ARGS()

		TSharedPtr<SEditableText> Edit[2];
		TSharedPtr<SEditableText> Rename;

		void Construct(const FArguments& Args)
		{
			FNWInventoryMenu* Menu = Args._Menu;
			TFunction<bool()> StorageShown = Args._StorageShown;
			TSharedRef<SCanvas> Canvas = SNew(SCanvas);
			for (int32 i = 0; i < 2; ++i)
			{
				Canvas->AddSlot()
					.Position(TAttribute<FVector2D>::CreateLambda([this, Menu, i]()
					{
						const FBox2f B = Menu->SearchRect(i);
						return FVector2D(B.Min.X + B.GetSize().Y * 0.98f, B.Min.Y) / Scale();
					}))
					.Size(TAttribute<FVector2D>::CreateLambda([this, Menu, i]()
					{
						const FBox2f B = Menu->SearchRect(i);
						return FVector2D(B.GetSize().X - B.GetSize().Y * 1.2f, B.GetSize().Y) / Scale();
					}))
					[
						SNew(SBox)
						.VAlign(VAlign_Center)
						.Visibility(TAttribute<EVisibility>::CreateLambda([Menu, i, StorageShown]()
						{
							return Menu->SearchRect(i).bIsValid && (i == 0 || StorageShown()) ? EVisibility::Visible : EVisibility::Collapsed;
						}))
						[
							SAssignNew(Edit[i], SEditableText)
							.Font(TAttribute<FSlateFontInfo>::CreateLambda([this, Menu, i]()
							{
								// Nimbus médio 25 da tela de 1080 (altura da linha ≈ 0,8 do tamanho em pontos do Slate).
								const float Px = Menu->SearchRect(i).GetSize().Y * 25.f / 45.f;
								return FSlateFontInfo(FNWUILayout::GameFont(7), FMath::Max(8, FMath::RoundToInt(Px / Scale() * 0.8f)));
							}))
							.ColorAndOpacity(FSlateColor(FLinearColor(FColor(0xEB, 0xE3, 0xD6))))
							.ClearKeyboardFocusOnCommit(true)
							.OnTextChanged_Lambda([Menu, i](const FText& T) { Menu->SetSearch(i, T.ToString()); })
						]
					];
			}
			// Nome da aba do armazém (clique duplo): Enter ou sair do campo grava.
			Canvas->AddSlot()
				.Position(TAttribute<FVector2D>::CreateLambda([this, Menu]() { return FVector2D(Menu->RenameRect().Min) / Scale(); }))
				.Size(TAttribute<FVector2D>::CreateLambda([this, Menu]() { return FVector2D(Menu->RenameRect().GetSize()) / Scale(); }))
				[
					SNew(SBox)
					.VAlign(VAlign_Center)
					.Padding(FMargin(6.f, 0.f))
					.Visibility(TAttribute<EVisibility>::CreateLambda([Menu]() { return Menu->RenameRect().bIsValid ? EVisibility::Visible : EVisibility::Collapsed; }))
					[
						SAssignNew(Rename, SEditableText)
						.Font(TAttribute<FSlateFontInfo>::CreateLambda([this, Menu]()
						{
							const float Px = Menu->RenameRect().GetSize().Y * 22.f / 38.f;
							return FSlateFontInfo(FNWUILayout::GameFont(7), FMath::Max(8, FMath::RoundToInt(Px / Scale() * 0.8f)));
						}))
						.ColorAndOpacity(FSlateColor(FLinearColor(FColor(0xEB, 0xE3, 0xD6))))
						.ClearKeyboardFocusOnCommit(true)
						.SelectAllTextWhenFocused(true)
						.OnTextCommitted_Lambda([Menu](const FText& T, ETextCommit::Type) { if (Menu->RenameRect().bIsValid) { Menu->CommitRename(T.ToString()); } })
					]
				];
			ChildSlot[Canvas];
		}

	private:
		float Scale() const
		{
			const float S = GetCachedGeometry().Scale;
			return S > 0.f ? S : 1.f;
		}
	};
}

void FNWInventoryMenu::ShowSearch()
{
	if (SearchHost.IsValid() || !GEngine || !GEngine->GameViewport || !FSlateApplication::IsInitialized())
	{
		return;   // já aberto, ou sem viewport (captura offline)
	}
	TSharedRef<SNWInventorySearch> W = SNew(SNWInventorySearch).Menu(this).StorageShown([this]() { return bStorage; });
	SearchEdit[0] = W->Edit[0];
	SearchEdit[1] = W->Edit[1];
	RenameEdit = W->Rename;
	for (int32 i = 0; i < 2; ++i)
	{
		SearchEdit[i]->SetText(FText::FromString(Lists[i].Search));
	}
	SearchHost = W;
	GEngine->GameViewport->AddViewportWidgetContent(W, 40);
}

void FNWInventoryMenu::HideSearch()
{
	if (SearchHost.IsValid() && GEngine && GEngine->GameViewport)
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
		GEngine->GameViewport->RemoveViewportWidgetContent(SearchHost.ToSharedRef());
	}
	SearchHost.Reset();
	SearchEdit[0].Reset();
	SearchEdit[1].Reset();
	RenameEdit.Reset();
}

// ------------------------------------------------------------------------------------------------------------ tooltip

TArray<FString> FNWInventoryMenu::Wrap(const FString& Text, float Width, float Size, int32 Style) const
{
	TArray<FString> Out;
	TArray<FString> Paragraphs;
	Text.ParseIntoArray(Paragraphs, TEXT("\n"), false);
	for (const FString& P : Paragraphs)
	{
		TArray<FString> Words;
		P.ParseIntoArrayWS(Words);
		FString Line;
		for (const FString& W : Words)
		{
			const FString Try = Line.IsEmpty() ? W : Line + TEXT(" ") + W;
			if (!Line.IsEmpty() && TextWidth(Try, Size, Style) > Width)
			{
				Out.Add(Line);
				Line = W;
			}
			else
			{
				Line = Try;
			}
		}
		Out.Add(Line);
	}
	return Out;
}

FBox2f FNWInventoryMenu::DrawTooltip(UCanvas* Canvas, const FNWItem& Item, const FVector2D& Mouse, bool bEquipped, float ForceX)
{
	// Tooltip do jogo (lyshineui/tooltip, DynamicTooltip): 360 de largura; cabeçalho de 104 com o fundo da raridade,
	// moldura do ícone 90×90 e tier; nome, raridade e tipo à direita; divisor dourado; seções separadas por linha.
	const FNWItemDef* D = FNWItemCatalog::Get().Find(Item.ItemId);
	if (!D)
	{
		return FBox2f(ForceInit);
	}
	static const TCHAR* Dir = TEXT("lyshineui/images/tooltip/");
	const float S = UIScale;
	const FLinearColor Body(0.62f, 0.6f, 0.52f), Divider(0.61f, 0.58f, 0.25f), Line(0.2f, 0.2f, 0.2f);
	const int32 Rarity = D->Type == TEXT("weapon") || D->Type == TEXT("armor") ? FMath::Min<int32>(Item.Rarity, 4) : FMath::Clamp(D->Tier - 1, 0, 4);

	// Seções do corpo: gear score, atributos, descrição, peso.
	// Icon: perk com ícone e o slot de gema (moldura do soquete, dynamictooltip_perk.lua) à esquerda do texto.
	struct FRow { FString Text; FLinearColor Color; float Size; int32 Style; float Gap; FString Icon = FString(); };
	TArray<FRow> Rows;
	auto Section = [&Rows]() { Rows.Add({FString(), FLinearColor::Transparent, 0.f, -1, 0.f}); };
	if (D->Type == TEXT("weapon") && D->Damage > 0.f)
	{
		Rows.Add({FString::Printf(TEXT("%.0f  %s"), D->Damage, *NWText::Get(TEXT("ui_tooltip_basedamage"))), FLinearColor(0.67f, 0.67f, 0.67f), 23.f, 0, 4.f});
	}
	if (!D->Gather.IsNone() && D->Efficiency > 1.f)
	{
		Rows.Add({FString::Printf(TEXT("+%.0f%%  %s"), (D->Efficiency - 1.f) * 100.f, *NWText::Get(TEXT("ui_tooltip_gatherspeed"))), FLinearColor(0.67f, 0.67f, 0.67f), 23.f, 0, 4.f});
	}
	// Item fabricado (docs/itens.md): atributo, qualidade (entre resistências e perks), perks com o valor da qualidade e
	// o slot de gema.
	const FNWItemRules& IR = FNWItemRules::Get();
	// Armadura: resistência física e mágica (iguais; docs/itens.md "Armaduras e joias"), no lugar do dano base.
	if (const float Res = IR.Resistance(*D, Item.Quality); Res > 0.f)
	{
		for (const TCHAR* Key : {TEXT("nw.item.res_physical"), TEXT("nw.item.res_magic")})
		{
			Rows.Add({FString::Printf(TEXT("%.1f  %s"), Res, *NWText::Get(Key)), FLinearColor(0.67f, 0.67f, 0.67f), 23.f, 0, 4.f});
		}
	}
	const FLinearColor Perk(0.86f, 0.82f, 0.68f), Muted(0.55f, 0.53f, 0.47f);
	if (Item.Attr != 255 && IR.Attributes.IsValidIndex(Item.Attr) && Item.GearScore > 0)
	{
		Rows.Add({FString::Printf(TEXT("+%d  %s"), IR.AttrValue(Item.GearScore), *NWText::Get(TEXT("ui_") + IR.Attributes[Item.Attr].ToString())),
			FLinearColor(0.67f, 0.67f, 0.67f), 23.f, 0, 4.f});
	}
	if (Item.Quality >= 0.f)
	{
		Section();
		Rows.Add({NWText::Format(TEXT("nw.item.quality"), {FString::Printf(TEXT("%.1f"), Item.Quality)}), FLinearColor(0.93f, 0.78f, 0.25f), 23.f, 0, 2.f});
	}
	if (Item.Perks.Num() > 0)
	{
		Section();
		for (FName P : Item.Perks)
		{
			const FNWItemRules::FPerk* PD = IR.Perks.Find(P);
			Rows.Add({IR.PerkText(P, Item.Quality, true), Perk, 23.f, 1, 0.f, PD ? PD->Icon : FString()});
			for (const FString& L : Wrap(IR.PerkText(P, Item.Quality, false), 330.f * S, 21.f, 0))
			{
				Rows.Add({L, Body, 21.f, 0, 0.f});
			}
		}
	}
	// Um slot em arma, escudo e armadura; joias e ferramentas não têm (docs/itens.md). Com gema: ícone e nome dela e os
	// efeitos (dynamictooltip_perk.lua mostra a gema como perk).
	if (FNWItemRules::HasSocket(*D, Item))
	{
		if (Item.Perks.Num() == 0)
		{
			Section();
		}
		if (const FNWItemDef* GD = Item.Gem.IsNone() ? nullptr : FNWItemCatalog::Get().Find(Item.Gem))
		{
			Rows.Add({GD->Name, Perk, 23.f, 1, 0.f, GD->Icon});
			for (const FString& L : IR.GemLines(Item.Gem))
			{
				Rows.Add({L, Body, 21.f, 0, 0.f});
			}
		}
		else
		{
			Rows.Add({NWText::Get(TEXT("nw.item.gem_empty")), Muted, 23.f, 1, 4.f, TEXT("lyshineui/images/tooltip/frame_gemsocket.dds")});
		}
	}
	// Gema ou vidro rúnico solto: os efeitos e onde encaixa.
	if (IR.Gems.Contains(Item.ItemId))
	{
		Section();
		for (const FString& L : IR.GemLines(Item.ItemId))
		{
			Rows.Add({L, Perk, 22.f, 0, 0.f});
		}
		Rows.Add({NWText::Get(IR.Gems[Item.ItemId].Class == TEXT("arma") ? TEXT("nw.item.gem_for_weapon") : TEXT("nw.item.gem_for_armor")), Muted, 21.f, 0, 2.f});
	}
	// Gemas: a descrição do jogo fala dos efeitos do jogo; vale a linha de efeitos do port acima.
	// Joia fabricada: a descrição do jogo fala de soquete, e joia não tem slot no port.
	const FString ItemClass = FNWItemRules::EquipClass(*D);
	const bool bTrinket = ItemClass == TEXT("amuleto") || ItemClass == TEXT("anel") || ItemClass == TEXT("brinco");
	if (!D->Description.IsEmpty() && !IR.Gems.Contains(Item.ItemId) && !bTrinket)
	{
		if (Rows.Num())
		{
			Section();
		}
		for (const FString& L : Wrap(D->Description, 330.f * S, 23.f, 0))
		{
			Rows.Add({L, Body, 23.f, 0, 0.f});
		}
	}
	Section();
	if (Item.Bind > 0)
	{
		Rows.Add({NWText::Get(Item.Bind == 2 ? TEXT("nw.item.bound") : TEXT("nw.item.bind_equip")), Muted, 21.f, 0, 0.f});
	}
	if (!Item.Signer.IsEmpty())
	{
		Rows.Add({NWText::Format(TEXT("nw.item.signed"), {Item.Signer}), FLinearColor(0.93f, 0.78f, 0.25f), 21.f, 2, 0.f});
	}
	Rows.Add({NWText::Get(TEXT("ui_tooltip_weight")).Replace(TEXT("{value}"), *FString::Printf(TEXT("%.1f"), D->Weight * Item.Quantity)), Body, 22.f, 0, 0.f});

	const float W = 360.f;
	float BodyH = 10.f + (Item.GearScore > 0 ? 50.f : 0.f);
	for (const FRow& R : Rows)
	{
		BodyH += R.Style < 0 ? 16.f : 27.f + R.Gap;
	}
	const float HeaderH = 106.f, TopH = bEquipped ? 40.f : 0.f;
	const float H = HeaderH + BodyH + 8.f;
	// Ao lado do item, para o lado com espaço; nunca para fora da tela.
	const float PW = W * S, PH = (H + TopH) * S;
	const float X = ForceX >= 0.f ? ForceX : Mouse.X > Canvas->ClipX * 0.5f ? Mouse.X - PW - 30.f * S : Mouse.X + 30.f * S;
	const float Y0 = FMath::Clamp(float(Mouse.Y) - 40.f * S, 20.f * S, Canvas->ClipY - PH - 20.f * S);
	const float Y = Y0 + TopH * S;
	auto Box = [&](float L, float T, float R, float B) { return FBox2f(FVector2f(X + L * S, Y + T * S), FVector2f(X + R * S, Y + B * S)); };

	DrawTexture(Canvas, FString(Dir) + TEXT("tooltip_bg_new.dds"), Box(-18.f, -16.f - TopH, W + 17.f, H + 17.f), FLinearColor(0.f, 0.f, 0.f, 0.8f));
	if (bEquipped)
	{
		DrawTexture(Canvas, FString(Dir) + TEXT("tooltip_bgframenew.dds"), Box(0.f, -TopH - 1.f, W, 0.f), FLinearColor(0.35f, 0.33f, 0.14f));
		DrawText(Canvas, NWText::Get(TEXT("ui_equipped")), X + 8.f * S, Y - 37.f * S, 24.f, FLinearColor::White, 2);
	}
	// Cabeçalho.
	FCanvasTileItem Black(FVector2D(X, Y), FVector2D(PW, HeaderH * S), FLinearColor::Black);
	Canvas->DrawItem(Black);
	DrawTexture(Canvas, FString::Printf(TEXT("%stooltip_header_bg_%d.dds"), Dir, Rarity), Box(0.f, 0.f, W, 104.f));
	if (Rarity == 4)
	{
		DrawTexture(Canvas, FString(Dir) + TEXT("tooltip_legendary_overlay.dds"), Box(0.f, 0.f, W, 104.f), FLinearColor(1.f, 1.f, 1.f, 0.5f));
	}
	if (UTexture2D* Icon = D->Icon.IsEmpty() ? nullptr : FNWUILayout::GameTexture(D->Icon))
	{
		FCanvasTileItem T(FVector2D(X + 10.f * S, Y + 9.f * S), Icon->GetResource(), FVector2D(86.f * S, 86.f * S), FLinearColor::White);
		T.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(T);
	}
	DrawTexture(Canvas, Rarity > 0 ? FString::Printf(TEXT("%stooltip_itemframe_rarity%d.dds"), Dir, Rarity)
		: FString(Dir) + TEXT("tooltip_itemframe_rarity_none.dds"), Box(8.f, 7.f, 98.f, 97.f));
	DrawText(Canvas, Roman(D->Tier), X + 12.f * S, Y + 9.f * S, 23.f, FLinearColor(0.62f, 0.58f, 0.48f));
	const TArray<FString> NameLines = Wrap(D->Name, 245.f * S, 27.f, 1);
	for (int32 i = 0; i < FMath::Min(NameLines.Num(), 2); ++i)
	{
		DrawText(Canvas, NameLines[i], X + 106.f * S, Y + (3.f + i * 25.f) * S, NameLines.Num() > 1 ? 24.f : 27.f, FLinearColor(0.9f, 0.88f, 0.81f), 1);
	}
	DrawText(Canvas, NWText::Get(FString::Printf(TEXT("raritylevel%d_displayname"), Rarity)), X + 106.f * S, Y + 52.f * S, 21.f, NWRarityColor(Rarity));
	DrawText(Canvas, D->TypeName, X + 106.f * S, Y + 75.f * S, 21.f, FLinearColor::White);
	DrawTexture(Canvas, FString(Dir) + TEXT("tooltip_divider.dds"), Box(0.f, 103.f, W, 106.f), Divider);

	// Corpo.
	float CY = HeaderH + 10.f;
	if (Item.GearScore > 0)
	{
		DrawTexture(Canvas, TEXT("lyshineui/images/icons/misc/icon_gearscore.dds"), Box(12.f, CY, 52.f, CY + 40.f));
		DrawText(Canvas, FString::FromInt(Item.GearScore), X + 56.f * S, Y + (CY - 8.f) * S, 50.f, FLinearColor(0.62f, 0.58f, 0.49f));
		CY += 50.f;
	}
	for (const FRow& R : Rows)
	{
		if (R.Style < 0)
		{
			DrawTexture(Canvas, TEXT("lyshineui/images/slices/sectionheader/sectionheaderline.dds"), Box(15.f, CY + 6.f, W - 15.f, CY + 10.f), Line);
			CY += 16.f;
			continue;
		}
		const float TextX = R.Icon.IsEmpty() ? 15.f : 47.f;
		if (!R.Icon.IsEmpty())
		{
			DrawTexture(Canvas, R.Icon, Box(15.f, CY + 1.f, 41.f, CY + 27.f));
		}
		DrawText(Canvas, R.Text, X + TextX * S, Y + CY * S, R.Size, R.Color, R.Style);
		CY += 27.f + R.Gap;
	}
	DrawTexture(Canvas, FString(Dir) + TEXT("tooltip_bgframenew.dds"), Box(0.f, 0.f, W, H), FLinearColor(0.35f, 0.33f, 0.14f));
	return FBox2f(FVector2f(X, Y0), FVector2f(X + PW, Y0 + PH));
}


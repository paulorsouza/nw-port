#include "NWInventoryMenu.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "NWCharacter.h"
#include "NWInventoryComponent.h"
#include "NWText.h"

static const FLinearColor InvTan(FColor(0xEB, 0xE3, 0xD6));
static const FLinearColor InvDim(FColor(0x9A, 0x96, 0x8E));
static const FLinearColor InvGold(FColor(0xED, 0xC6, 0x3E));
static const TCHAR* ItemLayoutDir = TEXT("lyshineui/images/slices/itemlayout/");
static const TCHAR* EquipDir = TEXT("lyshineui/images/equipment/");
// Peça de item do jogo (ItemDraggable/ItemLayout): 122 x 60; espaço de equipamento 72 x 72 (framesquare).
static constexpr float TileW = 122.f, TileH = 60.f, TileGap = 10.f, EquipSize = 84.f;
static constexpr float MaxWeight = UNWInventoryComponent::MaxWeight;

namespace
{
	const TCHAR* Roman(int32 Tier)
	{
		static const TCHAR* R[] = {TEXT(""), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V")};
		return R[FMath::Clamp(Tier, 0, 5)];
	}

	// Ordem das categorias na bolsa, como no jogo: armas e ferramentas, consumíveis, recursos.
	int32 Category(const FNWItemDef& D)
	{
		return D.Type == TEXT("weapon") ? 0 : D.Type == TEXT("consumable") ? 1 : 2;
	}

	// Ícone do tipo no espaço vazio (lyshineui/images/equipment/icon*.dds).
	const TCHAR* SlotIcon(int32 Slot)
	{
		switch (Slot)
		{
		case NWEquip::Shield: return TEXT("iconshield");
		case NWEquip::Pick: return TEXT("iconpickaxe");
		case NWEquip::Sickle: return TEXT("iconsickle");
		case NWEquip::LoggingAxe: return TEXT("iconlogging");
		case NWEquip::SkinningKnife: return TEXT("iconskinning");
		default: return TEXT("iconweapon");
		}
	}
}

bool FNWInventoryMenu::Load()
{
	if (!Window.Load(NWText::DataPath(TEXT("Data/UI/inventory_layout.json"))))
	{
		return false;
	}
	Window.ResolveTexture = [](const FString& P) { return FNWUILayout::GameTexture(P); };
	for (int32 i = 0; i < 3; ++i)
	{
		Window.Fonts[i] = FNWUILayout::GameFont(i);
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
		bPicked = false;
	}
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

void FNWInventoryMenu::DrawTexture(UCanvas* Canvas, const FString& Path, const FBox2f& R, const FLinearColor& Tint, bool bAdditive) const
{
	if (UTexture2D* Tex = FNWUILayout::GameTexture(Path))
	{
		FCanvasTileItem Tile(FVector2D(R.Min), Tex->GetResource(), FVector2D(R.GetSize()), Tint);
		Tile.BlendMode = bAdditive ? SE_BLEND_Additive : SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}
}

void FNWInventoryMenu::Layout(UCanvas* Canvas)
{
	const ANWCharacter* Me = Character.Get();
	const UNWInventoryComponent* Inv = Me ? Me->Inventory.Get() : nullptr;
	const float S = UIScale;
	Slots.Reset();
	Headers.Reset();

	// ---- bolsa: dentro da moldura da janela do jogo, abaixo do cabeçalho e acima do rodapé das moedas.
	const FNWUIElement* Frame = Window.FindOne(TEXT("inventory"), TEXT("Content/FrameMultiBg"));
	const FBox2f F = Frame ? Window.RectOf(Frame) : FBox2f(FVector2f(Canvas->ClipX * 0.5f, 0.f), FVector2f(Canvas->ClipX, Canvas->ClipY));
	BagArea = FBox2f(FVector2f(F.Min.X + 16.f * S, F.Min.Y + 128.f * S), FVector2f(F.Max.X - 12.f * S, F.Max.Y - 76.f * S));
	const float TW = TileW * S, TH = TileH * S, Gap = TileGap * S;
	const int32 Columns = FMath::Max(1, FMath::FloorToInt((BagArea.GetSize().X + Gap) / (TW + Gap)));
	const float GridX = BagArea.Min.X + (BagArea.GetSize().X - (Columns * TW + (Columns - 1) * Gap)) * 0.5f;
	if (Inv)
	{
		TArray<const FNWItem*> Bag;
		for (const FNWItem& I : Inv->Items)
		{
			if (I.Container == ENWItemContainer::Bag && FNWItemCatalog::Get().Find(I.ItemId))
			{
				Bag.Add(&I);
			}
		}
		Bag.Sort([](const FNWItem& A, const FNWItem& B)
		{
			const FNWItemDef& DA = *FNWItemCatalog::Get().Find(A.ItemId);
			const FNWItemDef& DB = *FNWItemCatalog::Get().Find(B.ItemId);
			if (Category(DA) != Category(DB)) return Category(DA) < Category(DB);
			if (DA.Tier != DB.Tier) return DA.Tier > DB.Tier;
			if (A.GearScore != B.GearScore) return A.GearScore > B.GearScore;
			return DA.Name < DB.Name;
		});
		static const TCHAR* CategoryKeys[] = {TEXT("nw.inv.cat_weapons"), TEXT("ui_consumable"), TEXT("nw.inv.cat_resources")};
		float Y = BagArea.Min.Y;
		int32 Current = -1, Col = 0;
		for (const FNWItem* I : Bag)
		{
			const int32 Cat = Category(*FNWItemCatalog::Get().Find(I->ItemId));
			if (Cat != Current)
			{
				if (Current != -1)
				{
					Y += TH + Gap;
				}
				Current = Cat;
				Col = 0;
				Headers.Add({NWText::Get(CategoryKeys[Cat]), FVector2f(GridX, Y)});
				Y += 30.f * S;
			}
			else if (Col == Columns)
			{
				Col = 0;
				Y += TH + Gap;
			}
			FSlotRef R;
			R.Container = ENWItemContainer::Bag;
			R.Slot = I->Slot;
			const float X = GridX + Col * (TW + Gap);
			R.Rect = FBox2f(FVector2f(X, Y), FVector2f(X + TW, Y + TH));
			Slots.Add(R);
			++Col;
		}
	}

	// ---- equipamento: painel à esquerda com as abas do jogo; o meio da tela fica para o personagem.
	EquipPanel = FBox2f(FVector2f(70.f, 95.f) * S, FVector2f(520.f, 985.f) * S);
	const float TabSize = 56.f * S;
	for (int32 t = 0; t < 2; ++t)
	{
		const float X = EquipPanel.Max.X - (2 - t) * (TabSize + 8.f * S) - 14.f * S;
		TabRects[t] = FBox2f(FVector2f(X, EquipPanel.Min.Y + 22.f * S), FVector2f(X + TabSize, EquipPanel.Min.Y + 22.f * S + TabSize));
	}
	const int32 First = EquipTab == 0 ? 0 : NWEquip::FirstTool;
	const int32 Last = EquipTab == 0 ? NWEquip::FirstTool : NWEquip::Count;
	float Y = EquipPanel.Min.Y + 120.f * S;
	for (int32 i = First; i < Last; ++i)
	{
		FSlotRef R;
		R.Container = ENWItemContainer::Equip;
		R.Slot = i;
		const float X = EquipPanel.Min.X + 34.f * S;
		R.Rect = FBox2f(FVector2f(X, Y), FVector2f(X + EquipSize * S, Y + EquipSize * S));
		Slots.Add(R);
		Y += (EquipSize + 26.f) * S;
	}
}

const FNWInventoryMenu::FSlotRef* FNWInventoryMenu::SlotAt(const FVector2D& Mouse) const
{
	return Slots.FindByPredicate([&](const FSlotRef& S) { return S.Rect.IsInside(FVector2f(Mouse)); });
}

void FNWInventoryMenu::DrawItemTile(UCanvas* Canvas, const FBox2f& R, const FNWItem* Item, bool bSquare, bool bHover, bool bPickedHere)
{
	const FNWItemDef* D = Item ? FNWItemCatalog::Get().Find(Item->ItemId) : nullptr;
	// Raridade da faixa: armas pela raridade sorteada; o resto pelo tier, como no jogo (comum até T2, depois sobe).
	const int32 Rarity = !D ? 0 : D->Type == TEXT("weapon") ? FMath::Min<int32>(Item->Rarity, 4) : FMath::Clamp(D->Tier - 1, 0, 4);
	const FString Shape = bSquare ? TEXT("square") : TEXT("large");
	const float Alpha = bPickedHere ? 0.45f : 1.f;
	// Fundo preto com borda da raridade + faixa pintada da raridade (itembg* / itemraritybg*).
	DrawTexture(Canvas, FString::Printf(TEXT("%sitembg%s%d.dds"), ItemLayoutDir, *Shape, D ? Rarity : 0), R, FLinearColor(1.f, 1.f, 1.f, D ? Alpha : 0.6f));
	if (!D)
	{
		return;
	}
	DrawTexture(Canvas, FString::Printf(TEXT("%sitemraritybg%s%d.dds"), ItemLayoutDir, *Shape, Rarity), R, FLinearColor(1.f, 1.f, 1.f, Alpha));
	if (UTexture2D* Icon = FNWUILayout::GameTexture(D->Icon))
	{
		// Ícone quadrado centrado (no retângulo do jogo o ícone ocupa a altura).
		const float Side = R.GetSize().Y * 0.94f;
		const FVector2f C = R.GetCenter();
		const FBox2f IR(C - FVector2f(Side * 0.5f), C + FVector2f(Side * 0.5f));
		FCanvasTileItem Tile(FVector2D(IR.Min), Icon->GetResource(), FVector2D(IR.GetSize()), FLinearColor(1.f, 1.f, 1.f, Alpha));
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}
	const float S = UIScale;
	// Tier em romano (canto superior esquerdo) e gear score (abaixo dele), como ItemTier/ItemSortValueText.
	DrawText(Canvas, Roman(D->Tier), R.Min.X + 5.f * S, R.Min.Y + 1.f * S, 15.f, InvTan, 1);
	if (Item->GearScore > 0)
	{
		DrawText(Canvas, FString::FromInt(Item->GearScore), R.Min.X + 5.f * S, R.Max.Y - 19.f * S, 13.f, InvGold, 0);
	}
	if (Item->Quantity > 1)
	{
		const FString Q = FString::FromInt(Item->Quantity);
		const float W = TextWidth(Q, 15.f, 1);
		const FBox2f QB(FVector2f(R.Max.X - W - 12.f * S, R.Max.Y - 22.f * S), R.Max);
		DrawTexture(Canvas, TEXT("lyshineui/images/slices/itemlayout/itemnumberbg.dds"), QB, FLinearColor(1.f, 1.f, 1.f, 0.9f * Alpha));
		DrawText(Canvas, Q, R.Max.X - W - 6.f * S, R.Max.Y - 20.f * S, 15.f, InvTan, 1);
	}
	if (bHover || bPickedHere)
	{
		DrawTexture(Canvas, FString::Printf(TEXT("%sitemhighlight%s.dds"), ItemLayoutDir, *Shape), R,
			FLinearColor(1.f, 1.f, 1.f, bPickedHere ? 1.f : 0.7f), true);
	}
}

void FNWInventoryMenu::DrawEquipPanel(UCanvas* Canvas, const FVector2D& Mouse)
{
	const ANWCharacter* Me = Character.Get();
	const UNWInventoryComponent* Inv = Me->Inventory;
	const float S = UIScale;
	DrawTexture(Canvas, TEXT("lyshineui/images/inventory/contentbg.dds"), EquipPanel, FLinearColor(1.f, 1.f, 1.f, 0.96f));
	DrawText(Canvas, NWText::Get(TEXT("nw.inv.equipment")), EquipPanel.Min.X + 34.f * S, EquipPanel.Min.Y + 30.f * S, 30.f, InvTan, 1);
	static const TCHAR* Tabs[] = {TEXT("tabweaponsarmor"), TEXT("tabtools")};
	for (int32 t = 0; t < 2; ++t)
	{
		const bool bHover = TabRects[t].IsInside(FVector2f(Mouse));
		DrawTexture(Canvas, FString::Printf(TEXT("%s%s%s.dds"), EquipDir, Tabs[t], EquipTab == t ? TEXT("selected") : TEXT("unselected")),
			TabRects[t], FLinearColor(1.f, 1.f, 1.f, EquipTab == t || bHover ? 1.f : 0.75f));
	}
	DrawText(Canvas, NWText::Get(EquipTab == 0 ? TEXT("nw.inv.tab_weapons") : TEXT("nw.inv.tab_tools")),
		EquipPanel.Min.X + 34.f * S, EquipPanel.Min.Y + 76.f * S, 17.f, InvDim);
	static const TCHAR* SlotNames[] = {TEXT("nw.inv.slot_sword"), TEXT("nw.inv.slot_hatchet"), TEXT("nw.inv.shield"),
		TEXT("2hpick_groupname"), TEXT("sickle_groupname"), TEXT("2haxe_groupname"), TEXT("1hskinningknife_groupname")};
	for (const FSlotRef& R : Slots)
	{
		if (R.Container != ENWItemContainer::Equip)
		{
			continue;
		}
		const FNWItem* Item = Inv->At(ENWItemContainer::Equip, R.Slot);
		const bool bHover = R.Rect.IsInside(FVector2f(Mouse));
		const bool bPickedHere = bPicked && PickedContainer == ENWItemContainer::Equip && PickedSlot == R.Slot;
		// Moldura do espaço do jogo; vazio mostra o ícone do tipo apagado.
		const FBox2f Outer(R.Rect.Min - FVector2f(6.f * S), R.Rect.Max + FVector2f(6.f * S));
		DrawTexture(Canvas, FString::Printf(TEXT("%sequipmentbg.dds"), EquipDir), Outer);
		DrawTexture(Canvas, FString::Printf(TEXT("%sframesquare.dds"), EquipDir), Outer);
		if (Item)
		{
			DrawItemTile(Canvas, R.Rect, Item, true, bHover, bPickedHere);
		}
		else
		{
			const FVector2f C = R.Rect.GetCenter();
			const float Side = 48.f * S;
			DrawTexture(Canvas, FString::Printf(TEXT("%s%s.dds"), EquipDir, SlotIcon(R.Slot)),
				FBox2f(C - FVector2f(Side * 0.5f), C + FVector2f(Side * 0.5f)), FLinearColor(1.f, 1.f, 1.f, bHover ? 0.8f : 0.45f));
			if (bHover)
			{
				DrawTexture(Canvas, TEXT("lyshineui/images/slices/itemlayout/itemhighlightsquare.dds"), R.Rect, FLinearColor(1.f, 1.f, 1.f, 0.6f), true);
			}
		}
		// Nome do tipo e, se houver, o item e o gear score.
		const float TX = R.Rect.Max.X + 20.f * S;
		DrawText(Canvas, NWText::Get(SlotNames[R.Slot]), TX, R.Rect.Min.Y + 6.f * S, 15.f, InvDim);
		if (const FNWItemDef* D = Item ? FNWItemCatalog::Get().Find(Item->ItemId) : nullptr)
		{
			DrawText(Canvas, D->Name, TX, R.Rect.Min.Y + 28.f * S, 19.f,
				D->Type == TEXT("weapon") ? NWRarityColor(Item->Rarity).Desaturate(-0.2f) : InvTan, 1);
			if (Item->GearScore > 0)
			{
				DrawText(Canvas, NWText::Format(TEXT("nw.inv.gear_score"), {FString::FromInt(Item->GearScore)}), TX, R.Rect.Min.Y + 54.f * S, 15.f, InvGold);
			}
		}
	}
}

void FNWInventoryMenu::DrawTooltip(UCanvas* Canvas, const FNWItem& Item, const FVector2D& Mouse)
{
	const FNWItemDef* D = FNWItemCatalog::Get().Find(Item.ItemId);
	if (!D)
	{
		return;
	}
	TArray<TPair<FString, FLinearColor>> Lines;
	Lines.Add({D->Name, D->Type == TEXT("weapon") ? NWRarityColor(Item.Rarity) : InvTan});
	Lines.Add({NWText::Format(TEXT("nw.inv.tier"), {FString::FromInt(D->Tier)}), InvDim});
	if (Item.GearScore > 0)
	{
		Lines.Add({NWText::Format(TEXT("nw.inv.gear_score"), {FString::FromInt(Item.GearScore)}), InvGold});
	}
	if (D->Weight > 0.f)
	{
		Lines.Add({FString::Printf(TEXT("%s %.1f"), *NWText::Get(TEXT("nw.inv.weight")), D->Weight * Item.Quantity), InvDim});
	}
	if (!D->Description.IsEmpty())
	{
		// Quebra simples em ~44 caracteres por linha.
		FString Rest = D->Description.Replace(TEXT("\n"), TEXT(" "));
		while (!Rest.IsEmpty())
		{
			int32 Cut = FMath::Min(44, Rest.Len());
			if (Cut < Rest.Len())
			{
				const int32 Space = Rest.Left(Cut).Find(TEXT(" "), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				Cut = Space > 10 ? Space : Cut;
			}
			Lines.Add({Rest.Left(Cut).TrimStartAndEnd(), InvTan});
			Rest = Rest.Mid(Cut).TrimStart();
		}
	}
	const float S = UIScale;
	const float W = 380.f * S, LineH = 26.f * S, Pad = 14.f * S;
	const float H = Pad * 2 + LineH * Lines.Num();
	// Ao lado do item, para o lado com espaço.
	float X = Mouse.X > Canvas->ClipX * 0.5f ? Mouse.X - W - 24.f * S : Mouse.X + 24.f * S;
	float Y = FMath::Min(float(Mouse.Y), Canvas->ClipY - H - 10.f);
	DrawTexture(Canvas, TEXT("lyshineui/images/inventory/contentbg.dds"), FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + H)));
	// Faixa da raridade no topo, como o cabeçalho do tooltip do jogo.
	const int32 Rarity = D->Type == TEXT("weapon") ? FMath::Min<int32>(Item.Rarity, 4) : FMath::Clamp(D->Tier - 1, 0, 4);
	DrawTexture(Canvas, FString::Printf(TEXT("%sitemraritybglarge%d.dds"), ItemLayoutDir, Rarity),
		FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + Pad + LineH * 1.4f)), FLinearColor(1.f, 1.f, 1.f, 0.8f));
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		DrawText(Canvas, Lines[i].Key, X + Pad, Y + Pad + i * LineH, i == 0 ? 21.f : 17.f, Lines[i].Value, i == 0 ? 1 : 0);
	}
}

void FNWInventoryMenu::Draw(UCanvas* Canvas, const FVector2D& Mouse)
{
	const ANWCharacter* Me = Character.Get();
	if (!Me || !Me->Inventory)
	{
		return;
	}
	UIScale = Canvas->ClipY / 1080.f;
	const float S = UIScale;
	const UNWInventoryComponent* Inv = Me->Inventory;

	// Escurece as bordas da tela, deixando o personagem no meio à vista.
	FCanvasTileItem Dim(FVector2D::ZeroVector, FVector2D(Canvas->ClipX, Canvas->ClipY), FLinearColor(0.f, 0.f, 0.f, 0.35f));
	Dim.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Dim);

	if (FNWUIElement* Coins = Window.FindOne(TEXT("inventory"), TEXT("Content/CurrencyText")))
	{
		Coins->TextOverride = FString::Printf(TEXT("%lld"), Inv->Coins);
	}
	Window.DrawOne(Canvas, TEXT("inventory"));
	Layout(Canvas);

	// Peso, à direita do título (WeightBar do jogo): ícone + "atual / máximo".
	float Weight = 0.f;
	for (const FNWItem& I : Inv->Items)
	{
		if (const FNWItemDef* D = FNWItemCatalog::Get().Find(I.ItemId))
		{
			Weight += D->Weight * I.Quantity;
		}
	}
	const FString WeightText = FString::Printf(TEXT("%.1f / %.0f"), Weight, MaxWeight);
	const float WX = BagArea.Max.X - TextWidth(WeightText, 17.f) - 6.f * S;
	const float WY = BagArea.Min.Y - 42.f * S;
	DrawTexture(Canvas, TEXT("lyshineui/images/inventory/iconweight.dds"),
		FBox2f(FVector2f(WX - 18.f * S, WY + 2.f * S), FVector2f(WX - 6.f * S, WY + 18.f * S)));
	DrawText(Canvas, WeightText, WX, WY, 17.f, Weight > MaxWeight ? FLinearColor(FColor(0xFF, 0x71, 0x71)) : InvTan);
	// Barra fina sob o cabeçalho.
	const FBox2f Bar(FVector2f(BagArea.Min.X, BagArea.Min.Y - 14.f * S), FVector2f(BagArea.Max.X, BagArea.Min.Y - 10.f * S));
	FCanvasTileItem BarBg(FVector2D(Bar.Min), FVector2D(Bar.GetSize()), FLinearColor(1.f, 1.f, 1.f, 0.08f));
	BarBg.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(BarBg);
	FCanvasTileItem BarFill(FVector2D(Bar.Min), FVector2D(Bar.GetSize().X * FMath::Clamp(Weight / MaxWeight, 0.f, 1.f), Bar.GetSize().Y), InvGold.CopyWithNewOpacity(0.8f));
	BarFill.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(BarFill);

	for (const FHeader& H : Headers)
	{
		DrawText(Canvas, H.Text, H.Pos.X, H.Pos.Y + 2.f * S, 18.f, InvDim, 1);
		DrawTexture(Canvas, TEXT("lyshineui/images/inventory/headerbg.dds"),
			FBox2f(FVector2f(H.Pos.X, H.Pos.Y + 24.f * S), FVector2f(BagArea.Max.X, H.Pos.Y + 27.f * S)), FLinearColor(1.f, 1.f, 1.f, 0.6f));
	}
	const FSlotRef* Hover = SlotAt(Mouse);
	for (const FSlotRef& R : Slots)
	{
		if (R.Container == ENWItemContainer::Bag)
		{
			const bool bPickedHere = bPicked && PickedContainer == ENWItemContainer::Bag && PickedSlot == R.Slot;
			DrawItemTile(Canvas, R.Rect, Inv->At(R.Container, R.Slot), false, Hover == &R, bPickedHere);
		}
	}
	DrawEquipPanel(Canvas, Mouse);
	if (Hover)
	{
		if (const FNWItem* Item = Inv->At(Hover->Container, Hover->Slot))
		{
			DrawTooltip(Canvas, *Item, Mouse);
		}
	}
}

void FNWInventoryMenu::Click(const FVector2D& Mouse, bool bRight)
{
	ANWCharacter* Me = Character.Get();
	if (!Me || !Me->Inventory)
	{
		return;
	}
	UNWInventoryComponent* Inv = Me->Inventory;
	for (int32 t = 0; t < 2; ++t)
	{
		if (!bRight && TabRects[t].IsInside(FVector2f(Mouse)))
		{
			EquipTab = t;
			bPicked = false;
			return;
		}
	}
	auto FreeBagSlot = [Inv]()
	{
		for (int32 B = 0; B < UNWInventoryComponent::BagSize; ++B)
		{
			if (!Inv->At(ENWItemContainer::Bag, B))
			{
				return B;
			}
		}
		return int32(INDEX_NONE);
	};
	const FSlotRef* S = SlotAt(Mouse);
	if (!S)
	{
		// Item de equipamento escolhido e clique na bolsa: desequipa.
		if (!bRight && bPicked && PickedContainer == ENWItemContainer::Equip && BagArea.IsInside(FVector2f(Mouse)))
		{
			if (const int32 B = FreeBagSlot(); B != INDEX_NONE)
			{
				Inv->ServerMove(ENWItemContainer::Equip, PickedSlot, ENWItemContainer::Bag, B);
			}
		}
		bPicked = false;
		return;
	}
	const FNWItem* Item = Inv->At(S->Container, S->Slot);
	if (bRight)
	{
		bPicked = false;
		const FNWItemDef* D = Item ? FNWItemCatalog::Get().Find(Item->ItemId) : nullptr;
		if (!D)
		{
			return;
		}
		if (S->Container == ENWItemContainer::Equip)
		{
			if (const int32 B = FreeBagSlot(); B != INDEX_NONE)
			{
				Inv->ServerMove(ENWItemContainer::Equip, S->Slot, ENWItemContainer::Bag, B);
			}
		}
		else if (D->Heal > 0.f)
		{
			Inv->ServerUse(S->Slot);
		}
		else if (const int32 To = NWEquip::SlotForSet(D->Set); To != INDEX_NONE)
		{
			Inv->ServerMove(ENWItemContainer::Bag, S->Slot, ENWItemContainer::Equip, To);
			EquipTab = To >= NWEquip::FirstTool ? 1 : 0;
		}
		return;
	}
	if (!bPicked)
	{
		if (Item)
		{
			bPicked = true;
			PickedContainer = S->Container;
			PickedSlot = S->Slot;
		}
		return;
	}
	bPicked = false;
	if (PickedContainer == S->Container && PickedSlot == S->Slot)
	{
		return;
	}
	if (S->Container == ENWItemContainer::Equip || PickedContainer == ENWItemContainer::Equip)
	{
		// Equipar, trocar de espaço ou devolver à bolsa (o servidor confere se o item serve no espaço).
		Inv->ServerMove(PickedContainer, PickedSlot, S->Container, S->Slot);
	}
	else if (Item)
	{
		// Bolsa com bolsa: a lista é ordenada pelo jogo; vale como escolher o outro item.
		bPicked = true;
		PickedContainer = S->Container;
		PickedSlot = S->Slot;
	}
}

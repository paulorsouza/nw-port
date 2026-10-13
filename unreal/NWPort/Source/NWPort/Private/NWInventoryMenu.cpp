#include "NWInventoryMenu.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "NWCharacter.h"
#include "NWCombatComponent.h"
#include "NWInventoryComponent.h"
#include "NWProgressionComponent.h"
#include "NWText.h"

static const FLinearColor InvTan(FColor(0xEB, 0xE3, 0xD6));
static const FLinearColor InvDim(FColor(0x9A, 0x96, 0x8E));
static const FLinearColor InvGold(FColor(0xED, 0xC6, 0x3E));
static const TCHAR* ItemLayoutDir = TEXT("lyshineui/images/slices/itemlayout/");
static const TCHAR* EquipDir = TEXT("lyshineui/images/equipment/");
// Peça de item do jogo (ItemDraggable/ItemLayout): 122 x 60; espaço de equipamento 72 x 72 (framesquare).
static constexpr float TileW = 122.f, TileH = 60.f, TileGap = 10.f, EquipSize = 84.f;

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

void FNWInventoryMenu::DrawStatus(UCanvas* Canvas)
{
	// Status no rodapé do equipamento (o "Atributos" da equipmentv3): nível e XP, os cinco atributos (com "+" quando
	// há pontos para distribuir), pontuação média das armas, saúde, vigor e peso.
	const ANWCharacter* Me = Character.Get();
	const UNWInventoryComponent* Inv = Me->Inventory;
	const UNWProgressionComponent* P = Me->Progression;
	const float S = UIScale;
	const FBox2f Box(FVector2f(EquipPanel.Min.X + 24.f * S, EquipPanel.Max.Y - 336.f * S), FVector2f(EquipPanel.Max.X - 24.f * S, EquipPanel.Max.Y - 36.f * S));
	DrawTexture(Canvas, FString::Printf(TEXT("%sequipmentstatsbackdrop2023.dds"), EquipDir), Box, FLinearColor(1.f, 1.f, 1.f, 0.85f));
	const float L = Box.Min.X + 24.f * S, R = Box.Max.X - 24.f * S, RowH = 27.f * S;
	float Y = Box.Min.Y + 16.f * S;
	auto Row = [&](const FString& Key, const FString& Value, const FLinearColor& Color)
	{
		DrawText(Canvas, Key, L, Y, 16.f, InvDim);
		DrawText(Canvas, Value, R - TextWidth(Value, 17.f, 1), Y - 1.f * S, 17.f, Color, 1);
		Y += RowH;
	};
	auto Separator = [&]()
	{
		DrawTexture(Canvas, TEXT("lyshineui/images/slices/sectionheader/sectionheaderline.dds"),
			FBox2f(FVector2f(L, Y + 2.f * S), FVector2f(R, Y + 5.f * S)), FLinearColor(0.35f, 0.35f, 0.35f));
		Y += 10.f * S;
	};
	for (FBox2f& PR : PointRects)
	{
		PR = FBox2f(ForceInit);
	}
	if (P)
	{
		const FString Xp = P->Level >= P->MaxLevel() ? FString(TEXT("-"))
			: FString::Printf(TEXT("%lld / %lld"), P->Xp - P->XpForLevel(P->Level), P->XpForLevel(P->Level + 1) - P->XpForLevel(P->Level));
		DrawText(Canvas, FString::Printf(TEXT("%s %d"), *NWText::Get(TEXT("ui_level")), P->Level), L, Y - 1.f * S, 18.f, InvTan, 1);
		DrawText(Canvas, Xp, R - TextWidth(Xp, 15.f), Y + 1.f * S, 15.f, InvDim);
		Y += RowH;
		const int32 Free = P->UnspentPoints();
		if (Free > 0)
		{
			DrawText(Canvas, FString::Printf(TEXT("%s: %d"), *NWText::Get(TEXT("ui_attribute_point")), Free), L, Y, 15.f, InvGold);
			Y += RowH - 4.f * S;
		}
		Separator();
		static const TCHAR* Names[] = {TEXT("ui_strength"), TEXT("ui_dexterity"), TEXT("ui_intelligence"), TEXT("ui_focus"), TEXT("ui_constitution")};
		for (int32 a = 0; a < 5; ++a)
		{
			const FString Value = FString::FromInt(P->Attribute(ENWAttribute(a)));
			const float VX = R - (Free > 0 ? 30.f * S : 0.f) - TextWidth(Value, 17.f, 1);
			DrawText(Canvas, NWText::Get(Names[a]), L, Y, 16.f, InvDim);
			DrawText(Canvas, Value, VX, Y - 1.f * S, 17.f, InvTan, 1);
			if (Free > 0)
			{
				PointRects[a] = FBox2f(FVector2f(R - 22.f * S, Y - 1.f * S), FVector2f(R, Y + 21.f * S));
				DrawTexture(Canvas, TEXT("lyshineui/images/slices/hint/hintframefilled.dds"), PointRects[a]);
				DrawText(Canvas, TEXT("+"), PointRects[a].GetCenter().X - TextWidth(TEXT("+"), 17.f, 2) * 0.5f, Y, 17.f, InvGold, 2);
			}
			Y += RowH;
		}
		Separator();
	}
	int32 Sum = 0, Count = 0;
	for (int32 Slot = 0; Slot < NWEquip::FirstTool; ++Slot)
	{
		if (const FNWItem* I = Inv->At(ENWItemContainer::Equip, Slot); I && I->GearScore > 0)
		{
			Sum += I->GearScore;
			++Count;
		}
	}
	const UNWCombatComponent* C = Me->Combat;
	Row(NWText::Get(TEXT("ui_buyorder_gear_score_label")), Count ? FString::FromInt(Sum / Count) : TEXT("-"), InvGold);
	Row(NWText::Get(TEXT("ui_health")), FString::Printf(TEXT("%.0f / %.0f"), C->Health, C->MaxHealth), InvTan);
	Row(NWText::Get(TEXT("ui_stamina")), FString::Printf(TEXT("%.0f / %.0f"), C->Stamina, C->MaxStamina), InvTan);
	Row(NWText::Get(TEXT("ui_encumbrance")), FString::Printf(TEXT("%.1f / %.0f"), Inv->TotalWeight(), Inv->MaxWeight), InvTan);
}

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
	const int32 Rarity = D->Type == TEXT("weapon") ? FMath::Min<int32>(Item.Rarity, 4) : FMath::Clamp(D->Tier - 1, 0, 4);

	// Seções do corpo: gear score, atributos, descrição, peso.
	struct FRow { FString Text; FLinearColor Color; float Size; int32 Style; float Gap; };
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
	if (!D->Description.IsEmpty())
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
		DrawText(Canvas, R.Text, X + 15.f * S, Y + CY * S, R.Size, R.Color, R.Style);
		CY += 27.f + R.Gap;
	}
	DrawTexture(Canvas, FString(Dir) + TEXT("tooltip_bgframenew.dds"), Box(0.f, 0.f, W, H), FLinearColor(0.35f, 0.33f, 0.14f));
	return FBox2f(FVector2f(X, Y0), FVector2f(X + PW, Y0 + PH));
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
	const FString WeightText = FString::Printf(TEXT("%.1f / %.0f"), Weight, Inv->MaxWeight);
	const float WX = BagArea.Max.X - TextWidth(WeightText, 17.f) - 6.f * S;
	const float WY = BagArea.Min.Y - 42.f * S;
	DrawTexture(Canvas, TEXT("lyshineui/images/inventory/iconweight.dds"),
		FBox2f(FVector2f(WX - 18.f * S, WY + 2.f * S), FVector2f(WX - 6.f * S, WY + 18.f * S)));
	DrawText(Canvas, WeightText, WX, WY, 17.f, Weight > Inv->MaxWeight ? FLinearColor(FColor(0xFF, 0x71, 0x71)) : InvTan);
	// Barra fina sob o cabeçalho.
	const FBox2f Bar(FVector2f(BagArea.Min.X, BagArea.Min.Y - 14.f * S), FVector2f(BagArea.Max.X, BagArea.Min.Y - 10.f * S));
	FCanvasTileItem BarBg(FVector2D(Bar.Min), FVector2D(Bar.GetSize()), FLinearColor(1.f, 1.f, 1.f, 0.08f));
	BarBg.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(BarBg);
	FCanvasTileItem BarFill(FVector2D(Bar.Min), FVector2D(Bar.GetSize().X * FMath::Clamp(Weight / Inv->MaxWeight, 0.f, 1.f), Bar.GetSize().Y), InvGold.CopyWithNewOpacity(0.8f));
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
	DrawStatus(Canvas);
	if (Hover)
	{
		if (const FNWItem* Item = Inv->At(Hover->Container, Hover->Slot))
		{
			const FBox2f R = DrawTooltip(Canvas, *Item, Mouse, Hover->Container != ENWItemContainer::Bag);
			// Comparação do jogo: item da bolsa que vai num espaço ocupado mostra o equipado ao lado.
			const FNWItemDef* D = FNWItemCatalog::Get().Find(Item->ItemId);
			const int32 EquipSlot = D && Hover->Container == ENWItemContainer::Bag ? NWEquip::SlotForSet(D->Set) : INDEX_NONE;
			if (const FNWItem* Equipped = EquipSlot != INDEX_NONE ? Inv->At(ENWItemContainer::Equip, EquipSlot) : nullptr; Equipped && R.bIsValid)
			{
				const float Gap = 14.f * UIScale, W = R.GetSize().X;
				const float CX = R.Min.X - Gap - W >= 0.f ? R.Min.X - Gap - W : R.Max.X + Gap;
				DrawTooltip(Canvas, *Equipped, FVector2D(Mouse.X, R.Min.Y + 40.f * UIScale), true, CX);
			}
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
	for (int32 a = 0; a < 5; ++a)
	{
		if (!bRight && Me->Progression && PointRects[a].bIsValid && PointRects[a].IsInside(FVector2f(Mouse)))
		{
			Me->Progression->ServerSpendPoint(ENWAttribute(a));
			return;
		}
	}
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

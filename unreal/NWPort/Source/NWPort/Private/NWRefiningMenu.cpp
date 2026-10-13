#include "NWRefiningMenu.h"

#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "NWCharacter.h"
#include "NWCrafting.h"
#include "NWInteractable.h"
#include "NWInventoryComponent.h"
#include "NWItems.h"
#include "NWPlayerController.h"
#include "NWProgressionComponent.h"
#include "NWText.h"

namespace
{
	// Cores do jogo (_common/uistyle.lua; docs/hud/refino.md §2).
	const FLinearColor TanWarm(FColor(0xEB, 0xE3, 0xD6));
	const FLinearColor GrayNew(FColor(0xB2, 0xAE, 0xA1));
	const FLinearColor Gray50(FColor(0x80, 0x80, 0x80));
	const FLinearColor Gray70(FColor(0xAA, 0xAA, 0xAA));
	const FLinearColor TanCol(FColor(0x9E, 0x94, 0x7E));
	const FLinearColor RedMedium(FColor(0xF2, 0x5D, 0x5D));
	const FLinearColor Green(FColor(0x30, 0xE5, 0xAB));
	const FLinearColor Yellow(FColor(0xFF, 0xDB, 0x7A));
	const FLinearColor CategoryBg(FColor(0x4A, 0x5D, 0x70));
	const TCHAR* Img = TEXT("lyshineui/images/");
	constexpr float RowStep = 46.f;          // UiDynamicScrollBox: protótipo 449×46
	constexpr int32 VisibleRows = 17;
	constexpr double RollCount = 1.0;        // craftroll.lua: duration
	constexpr double RollDelay = 0.5;        // ProgressionContainer: atraso 0,5 s
	constexpr double RollEnd = 2.2;          // fim da contagem + Base/Bonus (0,2 + 0,5)

	const TCHAR* Roman(int32 T)
	{
		static const TCHAR* R[] = {TEXT(""), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V")};
		return R[FMath::Clamp(T, 0, 5)];
	}

	FString Tex(const TCHAR* Rel) { return FString(Img) + Rel; }

	void SetText(FNWUIElement* El, const FString& T, TOptional<FLinearColor> Color = {})
	{
		if (El)
		{
			El->TextOverride = T;
			if (Color.IsSet())
			{
				El->Tint = Color.GetValue();
			}
		}
	}
	void SetShown(FNWUIElement* El, bool bOn) { if (El) { El->Visible = bOn; } }
	// Troca a textura sem mexer no cache: o FNWUILayout recarrega quando TextureOverride muda (CachedFor).
	void SetTexture(FNWUIElement* El, const FString& T) { if (El) { El->TextureOverride = T; } }
	// Opacidade animada pelo Lua (UiFaderBus.SetFadeValue): o canvas guarda o valor inicial em Fade.
	void SetFade(FNWUIElement* El, float F) { if (El) { El->Fade = F; } }

	// UiTransformBus SetLocalPosition + tamanho (ScriptedEntityTweener x/y/w/h) para elemento de âncora pontual.
	void SetLocalRect(FNWUIElement* El, float X, float Y, float W, float H)
	{
		if (El)
		{
			El->Offsets = FVector4f(X - W * El->Pivot.X, Y - H * El->Pivot.Y, X + W * (1.f - El->Pivot.X), Y + H * (1.f - El->Pivot.Y));
		}
	}
	// Ícone de estação (tradeskillscommon.lua, icon da estação).
	const TCHAR* StationIcon(const FString& Type)
	{
		return Type == TEXT("carpentry") ? TEXT("woodshop") : Type == TEXT("weaving") ? TEXT("loom") : Type == TEXT("masonry") ? TEXT("mason")
			: Type == TEXT("tanning") ? TEXT("tanning") : TEXT("smelter");
	}

	FString Loc(const TCHAR* Key) { return NWText::Get(Key); }
	FString NameOf(const TMap<FString, FString>& M, const FString& Fallback)
	{
		const FString* N = M.Find(NWText::Lang());
		return N ? *N : Fallback;
	}
}

bool FNWRefiningMenu::Load()
{
	auto Init = [](FNWUILayout& L, const TCHAR* File)
	{
		if (!L.Load(NWText::DataPath(File)))
		{
			return false;
		}
		L.ResolveTexture = [](const FString& P) { return FNWUILayout::GameTexture(P); };
		L.bExactText = true;
		for (int32 i = 0; i < 3; ++i)
		{
			L.Fonts[i] = FNWUILayout::GameFont(i);
		}
		return true;
	};
	return Init(Ui, TEXT("Data/UI/refining_layout.json")) && Init(Head, TEXT("Data/UI/screen_header_layout.json"))
		&& Init(Help, TEXT("Data/UI/helpbar_layout.json"));
}

void FNWRefiningMenu::Open(ANWCharacter* InCharacter, ANWStation* InStation)
{
	if (!bLoaded)
	{
		bLoaded = Load();
	}
	if (!bLoaded)
	{
		return;
	}
	Character = InCharacter;
	Station = InStation;
	Recipes = InStation ? FNWCrafting::Get().ForStation(InStation->Type, InStation->Tier) : TArray<const FNWRecipe*>();
	// Ordenar por nível dentro da família e do grupo (Sort by Level, o padrão da tela).
	Recipes.StableSort([](const FNWRecipe& A, const FNWRecipe& B)
	{
		return A.Category != B.Category ? A.Category < B.Category : A.Group != B.Group ? A.Group < B.Group : A.Level < B.Level;
	});
	Collapsed.Reset();
	BuildRows();
	Selected = Rows.IndexOfByPredicate([](const FRow& R) { return R.Recipe != INDEX_NONE; });
	Scroll = 0;
	Count = 1;
	State = EState::Select;
}

void FNWRefiningMenu::Select(FName RecipeId, int32 InCount)
{
	const int32 Found = Rows.IndexOfByPredicate([&](const FRow& R) { return R.Recipe != INDEX_NONE && Recipes[R.Recipe]->Id == RecipeId; });
	if (Found != INDEX_NONE)
	{
		Selected = Found;
		Scroll = FMath::Max(0, Found - VisibleRows + 1);
	}
	Count = FMath::Max(1, InCount);
}

void FNWRefiningMenu::Dump(const FString& Filter) const
{
	Ui.Dump(Filter);
	Head.Dump(Filter);
	Help.Dump(Filter);
}

void FNWRefiningMenu::BuildRows()
{
	Rows.Reset();
	Rows.Add({0, Loc(TEXT("crafting_tree_learned")), TEXT("learned")});
	if (Collapsed.Contains(TEXT("learned")))
	{
		return;
	}
	FString Cat, Group;
	for (int32 i = 0; i < Recipes.Num(); ++i)
	{
		const FNWRecipe& R = *Recipes[i];
		if (R.Category != Cat)
		{
			Cat = R.Category;
			Group.Reset();
			Rows.Add({1, NameOf(R.CategoryName, R.Category), TEXT("c:") + Cat});
		}
		if (Collapsed.Contains(TEXT("c:") + Cat))
		{
			continue;
		}
		if (R.Group != Group)
		{
			Group = R.Group;
			Rows.Add({2, NameOf(R.GroupName, R.Group), TEXT("g:") + Cat + Group});
		}
		if (Collapsed.Contains(TEXT("g:") + Cat + Group))
		{
			continue;
		}
		Rows.Add({3, FString(), FString(), i});
	}
}

int32 FNWRefiningMenu::MaxCount() const
{
	const ANWCharacter* Me = Character.Get();
	const FRow* Row = Rows.IsValidIndex(Selected) ? &Rows[Selected] : nullptr;
	if (!Me || !Row || Row->Recipe == INDEX_NONE)
	{
		return 0;
	}
	return FNWCrafting::Get().MaxCount(Me->Inventory, *Recipes[Row->Recipe]);
}

bool FNWRefiningMenu::CanCraft(const FNWRecipe& R) const
{
	const ANWCharacter* Me = Character.Get();
	return Me && FNWCrafting::Get().MaxCount(Me->Inventory, R) > 0
		&& (!Me->Progression || Me->Progression->MissingTradeLevel(R.Skill, R.Level) == 0);
}

void FNWRefiningMenu::Fill(UCanvas* Canvas, const FNWRecipe& R)
{
	ANWCharacter* Me = Character.Get();
	const FNWCrafting& C = FNWCrafting::Get();
	const UNWInventoryComponent* Inv = Me->Inventory;
	const UNWProgressionComponent* Prog = Me->Progression;
	const FNWItemDef* D = FNWItemCatalog::Get().Find(R.Out);
	const int32 Skill = NWTradeskill::Index(R.Skill);
	const bool bLevel = !Prog || Prog->MissingTradeLevel(R.Skill, R.Level) == 0;
	const int32 Max = C.MaxCount(Inv, R);
	Count = FMath::Clamp(Count, 1, FMath::Max(1, Max));

	// Painel central: item resultado (RP/Header), sem raridade nos recursos (ItemName #cccccc).
	FString Name = D ? D->Name : R.Out.ToString();
	if (R.Qty > 1)
	{
		Name += FString::Printf(TEXT(" x%d"), R.Qty);
	}
	SetText(E(TEXT("CraftingRecipePanel/Header/ItemName")), Name, FLinearColor(FColor(0xCC, 0xCC, 0xCC)));
	SetText(E(TEXT("Header/ItemName/Description")), D ? D->Description : FString());
	SetText(E(TEXT("CraftingRecipePanel/Header/ItemTier")), D && D->Tier > 0 ? FString::Printf(TEXT("%s %s"), *Loc(TEXT("cr_tier")), Roman(D->Tier)) : FString());
	SetTexture(E(TEXT("CraftingRecipePanel/Header/ItemRarityBg")), Tex(TEXT("crafting/crafting_itemraritybglarge0.dds")));
	SetTexture(E(TEXT("Header/ItemRarityBg/ItemIcon")), D ? D->Icon : FString());

	// Ingredientes: 7 linhas fixas (ingredientrowv4), "tenho/preciso", vermelho quando falta.
	for (int32 i = 0; i < 7; ++i)
	{
		const FString P = FString::Printf(TEXT("Ingredients/IngredientRow%d"), i + 1);
		const bool bOn = R.Ingredients.IsValidIndex(i);
		SetShown(Ui.FindOne(TEXT("refining"), P), bOn);
		if (!bOn)
		{
			continue;
		}
		const FNWRecipe::FIngredient& I = R.Ingredients[i];
		const FName Shown = C.ChosenFor(Inv, I);
		const FNWItemDef* ID = FNWItemCatalog::Get().Find(Shown);
		const int32 Need = I.Qty * Count, Have = C.Have(Inv, I);
		for (const TCHAR* Off : {TEXT("/IngredientSelectorBg"), TEXT("/ItemFocus"), TEXT("/CategoryIcon"), TEXT("/TypeName"),
			TEXT("/ItemLayout/ItemTier"), TEXT("/ItemLayout/ItemSortValueText"), TEXT("/ItemLayout/QuantityBg"),
			TEXT("/ItemLayout/ItemQuantity"), TEXT("/ItemLayout/ItemAnimatedIndicator"), TEXT("/ItemLayout/ItemDimmer"),
			TEXT("/ItemLayout/ItemHighlight"), TEXT("/Tooltip"), TEXT("/ItemLayout/ItemFrame"), TEXT("/ItemLayout/ItemRarityBg")})
		{
			SetShown(Ui.FindOne(TEXT("refining"), P + Off), false);
		}
		if (FNWUIElement* Ic = Ui.FindOne(TEXT("refining"), P + TEXT("/ItemLayout/ItemIcon")))
		{
			Ic->ImageType = 4;   // ícone quadrado no ItemLayout 118×56: proporção mantida (o jogo redimensiona o layout)
			SetTexture(Ic, ID ? ID->Icon : FString());
		}
		SetText(Ui.FindOne(TEXT("refining"), P + TEXT("/Quantity")), FString::Printf(TEXT("%d/%d"), Have, Need),
			Have >= Need ? FLinearColor::White : RedMedium);
		// Nome no estilo do canvas (FONT_STYLE_NIMBUS_MEDIUM_GRAY_50); vermelho só quando falta.
		FNWUIElement* IName = Ui.FindOne(TEXT("refining"), P + TEXT("/Quantity/IngredientName"));
		SetText(IName, ID ? ID->Name : Shown.ToString());
		if (IName)
		{
			IName->Tint = Have >= Need ? TOptional<FLinearColor>() : TOptional<FLinearColor>(RedMedium);
		}
	}

	// Painel direito: bloco de refino (nível do ofício e chance de item extra).
	SetText(E(TEXT("Refining/Skill/LevelBg/Level")), FString::Printf(TEXT("%d"), Prog ? Prog->TradeLevel(Skill) : 0));
	SetText(E(TEXT("Refining/Skill/Description")), Loc(*NWTradeskill::TextKey(Skill)));
	TArray<FName> Chosen;
	for (const FNWRecipe::FIngredient& I : R.Ingredients)
	{
		Chosen.Add(C.ChosenFor(Inv, I));
	}
	SetText(E(TEXT("Refining/Bonus/LevelBg/BonusAmt")),
		FString::Printf(TEXT("%.0f%%"), C.BonusChance(R, Prog ? Prog->TradeRollBonus(Skill) : 0, Chosen) * 100.f));

	// Requisitos (craftingstatspanel.lua UpdateRequirements): verde com check, vermelho com x; ícone da estação e do
	// ofício ao lado do texto; linhas em y = 81 + 28·n e o bloco (e a quantidade) sobe 28 por linha mostrada.
	auto Req = [this](const TCHAR* Text, const TCHAR* Icon, bool bOk, const FString& Desc)
	{
		SetText(E(Text), Desc, bOk ? Green : RedMedium);
		SetTexture(E(Icon), Tex(bOk ? TEXT("icons/misc/icon_requirement_check.dds") : TEXT("icons/misc/icon_requirement_x.dds")));
	};
	const bool bResources = Max > 0;
	Req(TEXT("ResourcesRequirement/HasResourcesText"), TEXT("ResourcesRequirement/HasResourcesIcon"), bResources,
		Loc(TEXT("ui_has_resources")));
	ANWStation* St = Station.Get();
	// "@<estação>_station" (tradeskillscommon.lua requireSubText) + "Tier n" do StationType da receita.
	Req(TEXT("StationRequirement/Description"), TEXT("StationRequirementIcon"), St->Tier >= R.Tier,
		FString::Printf(TEXT("%s %s %d"), *Loc(*(R.Station + TEXT("_station"))), *Loc(TEXT("ui_tier")), R.Tier));
	SetTexture(E(TEXT("StationRequirement/Icon")), Tex(*FString::Printf(TEXT("icons/stations/%s.dds"), StationIcon(St->Type))));
	SetFade(E(TEXT("StationRequirement/Icon")), 1.f);
	Req(TEXT("SkillRequirement/Description"), TEXT("SkillRequirementIcon"), bLevel,
		FString::Printf(TEXT("%s %s"), *Loc(*NWTradeskill::TextKey(Skill)),
			*Loc(TEXT("cr_levelrequired")).Replace(TEXT("{requiredSkillLevel}"), *FString::FromInt(R.Level))));
	SetTexture(E(TEXT("SkillRequirement/Icon")), NWTradeskill::Icon(Skill));
	SetFade(E(TEXT("SkillRequirement/Icon")), 1.f);
	const float Lines = 2.f * 28.f;   // estação + habilidade
	if (FNWUIElement* Box = E(TEXT("OffsetContainer/CraftingRequirements")))
	{
		Box->Translate.Y = -Lines;   // -123 - L11 (canvas: -123)
	}
	if (FNWUIElement* Q = E(TEXT("OffsetContainer/Quantity")))
	{
		Q->Translate.Y = -Lines;     // -261 - L11 (canvas: -261)
	}

	// Taxa e XP do ofício.
	SetText(E(TEXT("OffsetContainer/TaxRate")), FString::Printf(TEXT("%s: %d"), *Loc(TEXT("ui_crafting_fee")), R.Fee * Count),
		bResources ? TanCol : Gray50);
	SetText(E(TEXT("OffsetContainer/SkillExpReward")), FString::Printf(TEXT("+%d %s"), R.Xp * Count, *Loc(*NWTradeskill::TextKey(Skill))), Yellow);

	// Quantidade (craftquantitywidget): mínimo 1, máximo pelos materiais; número acima da barra.
	const bool bSlider = Max > 1;
	for (const TCHAR* Off : {TEXT("OffsetContainer/Quantity/PlusButton"), TEXT("OffsetContainer/Quantity/MinusButton"),
		TEXT("OffsetContainer/Quantity/Minimum"), TEXT("OffsetContainer/Quantity/Maximum"), TEXT("SliderWithTextInput/Slider")})
	{
		SetShown(E(Off), bSlider);
	}
	for (const TCHAR* Off : {TEXT("SliderWithTextInput/ExtraCurrentValueText"), TEXT("SliderWithTextInput/MaxValueText"),
		TEXT("CurrentValueTextInput/CrownIconLeft"), TEXT("Slider/SliderText")})
	{
		SetShown(E(Off), false);
	}
	SetText(E(TEXT("OffsetContainer/Quantity/Minimum")), TEXT("1"));
	SetText(E(TEXT("OffsetContainer/Quantity/Maximum")), FString::Printf(TEXT("%d"), FMath::Max(1, Max)));
	SetText(E(TEXT("CurrentValueTextInput/CurrentValueText")), FString::Printf(TEXT("%d"), Count));
	if (FNWUIElement* H = E(TEXT("Slider/SliderHandleHolder")))
	{
		H->Translate.X = Max > 1 ? 280.f * float(Count - 1) / float(Max - 1) : 0.f;   // trilho de 280 (SliderTrack)
	}
	// craftquantitywidget.lua: botões sem texto, com ícone (SetIconPath) centrado; SLIDER_STYLE_2 (sliderwithtextinput.lua):
	// caixa do valor 250×40 acima do centro da barra, Caslon 36 centralizado.
	for (const TCHAR* B : {TEXT("Quantity/PlusButton"), TEXT("Quantity/MinusButton")})
	{
		const FString P(B);
		SetText(Ui.FindOne(TEXT("refining"), P + TEXT("/buttonText")), FString());
		if (FNWUIElement* Ic = Ui.FindOne(TEXT("refining"), P + TEXT("/buttonText/buttonIconHolder")))
		{
			Ic->Visible = bSlider;
			Ic->Fade = 1.f;
			Ic->TextureOverride = Tex(P.EndsWith(TEXT("PlusButton")) ? TEXT("icons/crafting/icon_crafting_plus.png") : TEXT("icons/crafting/icon_crafting_minus.png"));
			Ic->Translate.X = 27.f + 1.f;   // âncora −44…−10 do texto vazio; SetIconPositionX(1)
		}
		SetShown(Ui.FindOne(TEXT("refining"), P + TEXT("/buttonText/buttonIconHolder/buttonIconValue")), false);
	}
	SetLocalRect(E(TEXT("SliderWithTextInput/CurrentValueTextInput")), 0.f, -50.f, 250.f, 40.f);
	if (FNWUIElement* V = E(TEXT("CurrentValueTextInput/CurrentValueText")))
	{
		V->Offsets = FVector4f(0.f, 0.f, 0.f, -4.f);
		V->FontFile = TEXT("lyshineui/fonts/caslon.font");
		V->FontSize = 36.f;
		V->HAlign = 1;
	}

	// Botão CONFECCIONAR: herói azul (flipbook blue_loop, 15 quadros) quando pode; apagado quando não.
	const bool bCan = bResources && bLevel;
	SetText(E(TEXT("OffsetContainer/CraftButton/buttonText")), Loc(TEXT("ui_craft")).ToUpper(), bCan ? TanWarm : Gray50);
	SetTexture(E(TEXT("OffsetContainer/CraftButton/frame")), Tex(bCan ? TEXT("slices/button/heroframe2023.dds") : TEXT("slices/button/heroframedisabled2023.dds")));
	SetShown(E(TEXT("OffsetContainer/CraftButton/heroImageSequenceDisabled")), !bCan);
	SetShown(E(TEXT("OffsetContainer/CraftButton/heroImageSequence")), bCan);
	SetShown(E(TEXT("OffsetContainer/CraftButton/buttonFocus")), false);
	SetShown(E(TEXT("OffsetContainer/CraftButton/frameFocus")), false);
	if (bCan)
	{
		const int32 Frame = int32(Now() * 15.0) % 15;
		SetTexture(E(TEXT("OffsetContainer/CraftButton/heroImageSequence")), Tex(*FString::Printf(TEXT("slices/button/blue_loop/blue_%02d.dds"), Frame)));
	}
	SetFade(E(TEXT("OffsetContainer/CraftButton/heroImageSequence")), 1.f);
	SetFade(E(TEXT("OffsetContainer/CraftButton/heroImageSequenceDisabled")), 1.f);
}

void FNWRefiningMenu::DrawRows(UCanvas* Canvas, const FVector2D& Mouse)
{
	FNWUIElement* Proto = E(TEXT("Mask/Content/CraftingTreeViewItem"));
	FNWUIElement* Mask = E(TEXT("CraftingTreeView/ScrollBox/Mask"));
	if (!Proto || !Mask)
	{
		return;
	}
	const float UIScale = Canvas->ClipY / 1080.f;
	const FBox2f ProtoRect = Ui.RectOf(Proto), MaskRect = Ui.RectOf(Mask);
	const float BaseY = (MaskRect.Min.Y - ProtoRect.Min.Y) / UIScale;
	const FBox2f ProtoLocal(ProtoRect.Min / UIScale, ProtoRect.Max / UIScale);
	auto At = [this](const TCHAR* Off) { return Ui.FindOne(TEXT("refining"), FString(TEXT("Mask/Content/CraftingTreeViewItem")) + Off); };
	// Posição x de cada peça por profundidade (craftingtreeviewitem.lua), relativa ao início do item. A base vem do Draw
	// deste quadro, em que o Translate dessas peças foi zerado (Draw).
	TMap<FNWUIElement*, float> Base;
	for (const TCHAR* Off : {TEXT("/StateIcon"), TEXT("/EntryText"), TEXT("/RarityFrame"), TEXT("/TypeIcon")})
	{
		if (FNWUIElement* El = At(Off))
		{
			Base.Add(El, Ui.RectOf(El).Min.X / UIScale - ProtoLocal.Min.X);
		}
	}
	auto PlaceX = [&](const TCHAR* Off, float X)
	{
		if (FNWUIElement* El = At(Off))
		{
			El->Translate.X = X - Base.FindRef(El);
		}
	};
	RowRects.Reset();
	Scroll = FMath::Clamp(Scroll, 0, FMath::Max(0, Rows.Num() - VisibleRows));
	for (int32 i = Scroll; i < Rows.Num() && i < Scroll + VisibleRows; ++i)
	{
		const FRow& Row = Rows[i];
		const bool bHeader = Row.Depth < 3;
		const FNWRecipe* R = bHeader ? nullptr : Recipes[Row.Recipe];
		const FNWItemDef* D = R ? FNWItemCatalog::Get().Find(R->Out) : nullptr;
		// Fundo: cabeçalho com categoryBgAdd (#4a5d70, 0,3); receita com itemBg (#e7e7e7, 0,07); seleção: faixa azul.
		SetShown(At(TEXT("/ListItemBg/categoryBgAdd")), bHeader);
		if (FNWUIElement* Cb = At(TEXT("/ListItemBg/categoryBgAdd")))
		{
			Cb->Tint = CategoryBg;
			Cb->Fade = 0.3f;
		}
		if (FNWUIElement* Bg = At(TEXT("/ListItemBg/itemBg")))
		{
			Bg->Tint = FLinearColor(FColor(0xE7, 0xE7, 0xE7));
			Bg->Fade = bHeader ? 0.f : 0.07f;
		}
		// EntryText.w (craftingtreeviewitem.lua): 360 nos cabeçalhos, 245 na receita aprendida; nome longo encolhe.
		if (FNWUIElement* Et = At(TEXT("/EntryText")))
		{
			Et->Offsets.Z = Et->Offsets.X + (bHeader ? 360.f : 245.f);
			Et->ShrinkToFit = 1;
			Et->MinShrink = 0.7f;
		}
		const bool bSel = i == Selected;
		const FVector2f Offset(0.f, BaseY + (i - Scroll) * RowStep);
		const FBox2f Guess(ProtoRect.Min + FVector2f(0.f, Offset.Y * UIScale), ProtoRect.Max + FVector2f(0.f, Offset.Y * UIScale));
		const bool bHover = Guess.IsInside(FVector2f(Mouse)) && MaskRect.IsInside(FVector2f(Mouse));
		SetShown(At(TEXT("/ListItemBg/SelectedSequenceMask")), bSel);
		SetShown(At(TEXT("/ListItemBg/SelectedSequenceMask/SelectedSequence")), false);
		SetShown(At(TEXT("/ListItemBg/FocusMask")), false);
		SetShown(At(TEXT("/ListItemBg/focusGlow")), bSel);
		SetShown(At(TEXT("/ListItemBg/HoverGlow")), false);
		SetShown(At(TEXT("/ListItemBg/FocusAdd")), bHover && !bSel);
		SetFade(At(TEXT("/ListItemBg/FocusAdd")), 1.f);
		SetFade(At(TEXT("/ListItemBg/SelectedSequenceMask/LastFrame")), 1.f);   // último quadro do flipbook de seleção
		SetFade(At(TEXT("/ListItemBg/focusGlow")), 1.f);
		SetShown(At(TEXT("/TypeIcon/ObjectiveIcon")), false);
		SetShown(At(TEXT("/TypeIcon/Pulse")), false);
		SetShown(At(TEXT("/RarityFrame/FirstCraftBar")), false);
		SetShown(At(TEXT("/RarityFrame/RarityGlow")), false);
		SetShown(At(TEXT("/StateIcon")), bHeader);
		SetShown(At(TEXT("/RarityFrame")), !bHeader);
		SetShown(At(TEXT("/TypeIcon")), !bHeader);
		SetShown(At(TEXT("/GearScoreText")), !bHeader);
		FLinearColor Color = bHeader ? (bHover ? TanWarm : GrayNew) : TanWarm;
		if (bHeader)
		{
			SetTexture(At(TEXT("/StateIcon")), Tex(Collapsed.Contains(Row.Key) ? TEXT("icons/misc/dropdownarrowwhiteright.dds") : TEXT("icons/misc/dropdownarrowwhite.dds")));
			SetText(At(TEXT("/EntryText")), Row.Text, Color);
			// craftingtreeviewitem.lua; +6 no texto: a seta do port é desenhada sem a folga do sprite do jogo.
			const float X[3][2] = {{15.f, 36.f}, {36.f, 57.f}, {59.f, 80.f}};
			PlaceX(TEXT("/StateIcon"), X[Row.Depth][0]);
			PlaceX(TEXT("/EntryText"), X[Row.Depth][1]);
		}
		else
		{
			// "[n] nome": n = quantos do resultado o personagem já tem (QueryInventoryCache), sem colchete quando 0.
			const ANWCharacter* Me = Character.Get();
			const int32 Owned = Me && Me->Inventory ? Me->Inventory->CountOf(R->Out) : 0;
			const FString Name = D ? D->Name : R->Out.ToString();
			Color = CanCraft(*R) ? TanWarm : RedMedium;
			SetText(At(TEXT("/EntryText")), Owned > 0 ? FString::Printf(TEXT("[%d] %s"), Owned, *Name) : Name, Color);
			SetText(At(TEXT("/GearScoreText")), FString::Printf(TEXT("%d"), R->Level), Color);
			if (FNWUIElement* Ti = At(TEXT("/GearScoreText/TradeskillIcon")))
			{
				Ti->Tint = Color;
				Ti->Translate.X = -6.f;   // folga entre o ícone e o nível
			}
			SetTexture(At(TEXT("/GearScoreText/TradeskillIcon")), NWTradeskill::Icon(NWTradeskill::Index(R->Skill)));
			SetTexture(At(TEXT("/RarityFrame")), Tex(TEXT("slices/itemlayout/itembgsquare0.dds")));
			SetTexture(At(TEXT("/TypeIcon")), D ? D->Icon : FString());
			PlaceX(TEXT("/RarityFrame"), 74.f);
			PlaceX(TEXT("/TypeIcon"), 76.f);
			PlaceX(TEXT("/EntryText"), 118.f);
		}
		const FBox2f Drawn = Ui.DrawSubtree(Canvas, TEXT("refining"), Proto, Offset);
		RowRects.Add({Drawn, i});
	}
}

void FNWRefiningMenu::DrawRoll(const FNWRecipe& R)
{
	// craftroll.lua, tipo "Quantity": a contagem sobe de 0 ao total em 1 s depois de 0,5 s; Base e Bônus no fim.
	const ANWCharacter* Me = Character.Get();
	const double T = Now() - RollStart;
	const int32 Received = Me && Me->Inventory ? FMath::Max(0, Me->Inventory->CountOf(R.Out) - HaveBefore) : 0;
	const int32 Total = FMath::Max(Received, RollBase);
	const float K = float(FMath::Clamp((T - RollDelay) / RollCount, 0.0, 1.0));
	const FNWItemDef* D = FNWItemCatalog::Get().Find(R.Out);
	SetText(E(TEXT("RarityOffset/Header/HeaderOffset/ItemName")), D ? D->Name : R.Out.ToString(), TanCol);
	if (FNWUIElement* Icon = E(TEXT("RarityOffset/Header/HeaderOffset/ItemImage")))
	{
		Icon->ImageType = 4;   // ícone do item no lugar do exemplo do canvas, com a proporção mantida
		SetTexture(Icon, D ? D->Icon : FString());
	}
	SetText(E(TEXT("Stats/Quantity/RefiningAmount")), FString::Printf(TEXT("%d"), FMath::FloorToInt(Total * K)));
	const bool bEnd = T > RollDelay + RollCount + 0.2;
	SetShown(E(TEXT("Stats/Quantity/BaseValue")), bEnd);
	SetShown(E(TEXT("Stats/Quantity/BonusValue")), bEnd);
	SetText(E(TEXT("Stats/Quantity/BaseValue")), FString::Printf(TEXT("%d"), RollBase));
	SetText(E(TEXT("Stats/Quantity/BonusValue")), FString::Printf(TEXT("%d"), FMath::Max(0, Received - RollBase)));
	if (FNWUIElement* Bg = E(TEXT("CraftRoll/Background")))
	{
		Bg->AlphaMul = float(FMath::Clamp((T - 0.25) / 0.3, 0.0, 1.0));
	}
	if (FNWUIElement* Box = E(TEXT("CraftRoll/CraftRollContainer")))
	{
		Box->AlphaMul = float(FMath::Clamp((T - 0.25) / 0.3, 0.0, 1.0));
	}
	if (State == EState::Roll && T > RollEnd)
	{
		State = EState::Done;
	}
}

void FNWRefiningMenu::DrawHelp(UCanvas* Canvas)
{
	// Rodapé (helpbar): ENTER Chat à esquerda; à direita, da direita para a esquerda, ESC Voltar e (máximo > 1) ESPAÇO
	// Quantidade máxima. A caixa da tecla acompanha o texto dela; o rótulo vem 12 depois da caixa.
	FNWUIElement* Proto = Help.FindOne(TEXT("helpbar"), TEXT("ActionsDisplays/ActionDisplay"));
	FNWUIElement* Hint = Help.FindOne(TEXT("helpbar"), TEXT("ActionsDisplays/ActionDisplay/Hint"));
	FNWUIElement* Key = Help.FindOne(TEXT("helpbar"), TEXT("ActionDisplay/Hint/HintText"));
	FNWUIElement* Label = Help.FindOne(TEXT("helpbar"), TEXT("ActionsDisplays/ActionDisplay/Text"));
	if (!Proto || !Hint || !Key || !Label)
	{
		return;
	}
	for (int32 n = 2; n <= 12; ++n)
	{
		SetShown(Help.FindOne(TEXT("helpbar"), *FString::Printf(TEXT("ActionsDisplays/ActionDisplay%d"), n)), false);
	}
	for (FNWUIElement* El : Help.Find(TEXT("helpbar"), TEXT("GlobalActionDisplays")))
	{
		El->Visible = false;
	}
	Proto->Visible = false;
	Help.Draw(Canvas);
	const float UIScale = Canvas->ClipY / 1080.f;
	const float ProtoRight = Help.RectOf(Proto).Max.X / UIScale;
	const float ProtoLeft = ProtoRight - 200.f;   // ActionDisplay: -200..0 à direita
	auto Measure = [&](const FString& K, const FString& Text, float& HintW)
	{
		SetText(Key, K);
		SetText(Label, Text);
		HintW = FMath::Max(32.f, Help.TextWidth(Canvas, *Key, UIScale) + 16.f);
		return HintW + 12.f + Help.TextWidth(Canvas, *Label, UIScale);
	};
	auto DrawAction = [&](const FString& K, const FString& Text, float Left)
	{
		float HintW = 32.f;
		Measure(K, Text, HintW);
		Hint->Offsets.Z = HintW;
		Label->Offsets.X = HintW + 12.f;
		Label->Offsets.Z = HintW + 12.f + 300.f;
		Help.DrawSubtree(Canvas, TEXT("helpbar"), Proto, FVector2f(Left - ProtoLeft, 0.f));
	};
	DrawAction(TEXT("ENTER"), Loc(TEXT("ui_chat")), 40.f);
	TArray<TPair<FString, FString>> Right = {{TEXT("ESC"), Loc(TEXT("ui_back"))}};
	if (MaxCount() > 1 && State == EState::Select)
	{
		Right.Add({TEXT("SPACE"), Loc(TEXT("crafting_set_max"))});
	}
	float X = ProtoRight - 40.f;   // mesma margem do ENTER à esquerda
	for (const TPair<FString, FString>& A : Right)
	{
		float HintW = 32.f;
		X -= Measure(A.Key, A.Value, HintW);
		DrawAction(A.Key, A.Value, X);
		X -= 50.f;   // DEFAULT_ACTION_SPACING
	}
}

void FNWRefiningMenu::Draw(UCanvas* Canvas, const FVector2D& Mouse)
{
	ANWCharacter* Me = Character.Get();
	ANWStation* St = Station.Get();
	if (!Me || !St || !Canvas)
	{
		Close();
		return;
	}
	const FRow* Row = Rows.IsValidIndex(Selected) ? &Rows[Selected] : nullptr;
	const FNWRecipe* R = Row && Row->Recipe != INDEX_NONE ? Recipes[Row->Recipe] : nullptr;
	const bool bRoll = State != EState::Select;
	SetShown(E(TEXT("CraftingContainer/CraftingPanel")), !bRoll);
	SetShown(E(TEXT("CraftingContainer/CraftRoll")), bRoll);
	SetShown(E(TEXT("CraftingContainer/CraftDone")), State == EState::Done);
	if (FNWUIElement* Bg = E(TEXT("_root/CraftingScreen/BackgroundStep1")))
	{
		Bg->AlphaMul = bRoll ? float(FMath::Clamp((Now() - RollStart) / 0.35, 0.0, 1.0)) : 0.f;
	}
	if (FNWUIElement* Proto = E(TEXT("Mask/Content/CraftingTreeViewItem")))
	{
		Proto->Visible = false;   // item-modelo: desenhado por linha em DrawRows
	}
	for (const TCHAR* Off : {TEXT("CraftingTreeViewItem/StateIcon"), TEXT("CraftingTreeViewItem/EntryText"),
		TEXT("CraftingTreeViewItem/RarityFrame"), TEXT("CraftingTreeViewItem/TypeIcon")})
	{
		if (FNWUIElement* El = E(Off))
		{
			El->Translate.X = 0.f;   // DrawRows mede a posição base neste Draw
		}
	}
	if (R && !bRoll)
	{
		Fill(Canvas, *R);
	}
	else if (R)
	{
		DrawRoll(*R);
	}
	Ui.Draw(Canvas);
	if (!bRoll)
	{
		DrawRows(Canvas, Mouse);
	}
	// Barra de título: nome da estação e moedas.
	SetText(Head.FindOne(TEXT("screen_header"), TEXT("HeaderHolder/HeaderText")), St->Interact->DisplayName.ToString());
	SetText(Head.FindOne(TEXT("screen_header"), TEXT("CurrencyDisplay/CurrencyAmount")),
		FText::AsNumber(Me->Inventory ? Me->Inventory->Coins : 0).ToString());
	Head.Draw(Canvas);
	DrawHelp(Canvas);
}

bool FNWRefiningMenu::Click(const FVector2D& Mouse)
{
	if (!IsOpen())
	{
		return false;
	}
	ANWCharacter* Me = Character.Get();
	const FVector2f M(Mouse);
	if (State == EState::Done)
	{
		State = EState::Select;   // clique em qualquer lugar volta à estação (CraftCompleteReturnToStation)
		return true;
	}
	if (State == EState::Roll)
	{
		return true;
	}
	if (Head.RectOf(Head.FindOne(TEXT("screen_header"), TEXT("HeaderHolder/BackButton"))).ExpandBy(10.f).IsInside(M))
	{
		if (ANWPlayerController* PC = Me ? Cast<ANWPlayerController>(Me->GetController()) : nullptr)
		{
			PC->CloseMenus();
		}
		return true;
	}
	for (const TPair<FBox2f, int32>& Row : RowRects)
	{
		if (Row.Key.IsInside(M) && Rows.IsValidIndex(Row.Value))
		{
			const FRow& Clicked = Rows[Row.Value];
			if (Clicked.Recipe != INDEX_NONE)
			{
				Selected = Row.Value;
				Count = 1;
			}
			else
			{
				// Cabeçalho: abre/fecha (StateIcon); mantém a receita escolhida se ela continuar na lista.
				const int32 Keep = Rows.IsValidIndex(Selected) ? Rows[Selected].Recipe : INDEX_NONE;
				if (!Collapsed.Remove(Clicked.Key))
				{
					Collapsed.Add(Clicked.Key);
				}
				BuildRows();
				Selected = Rows.IndexOfByPredicate([Keep](const FRow& X) { return X.Recipe == Keep && Keep != INDEX_NONE; });
			}
			return true;
		}
	}
	const FRow* Sel = Rows.IsValidIndex(Selected) ? &Rows[Selected] : nullptr;
	if (!Sel || Sel->Recipe == INDEX_NONE)
	{
		return true;
	}
	const int32 Max = FMath::Max(1, MaxCount());
	if (Ui.RectOf(E(TEXT("Quantity/MinusButton"))).IsInside(M))
	{
		Count = FMath::Max(1, Count - 1);
	}
	else if (Ui.RectOf(E(TEXT("Quantity/PlusButton"))).IsInside(M))
	{
		Count = FMath::Min(Max, Count + 1);
	}
	else if (Ui.RectOf(E(TEXT("OffsetContainer/Quantity/Maximum"))).IsInside(M))
	{
		Count = Max;
	}
	else if (Ui.RectOf(E(TEXT("SliderWithTextInput/Slider"))).IsInside(M) && Max > 1)
	{
		const FBox2f Track = Ui.RectOf(E(TEXT("Slider/SliderTrack")));
		const float K = FMath::Clamp((M.X - Track.Min.X) / FMath::Max(1.f, Track.GetSize().X), 0.f, 1.f);
		Count = 1 + FMath::RoundToInt(K * (Max - 1));
	}
	else if (Ui.RectOf(E(TEXT("OffsetContainer/CraftButton"))).IsInside(M))
	{
		ClickCraft();
	}
	return true;
}

void FNWRefiningMenu::ClickCraft()
{
	ANWCharacter* Me = Character.Get();
	const FRow* Sel = Rows.IsValidIndex(Selected) ? &Rows[Selected] : nullptr;
	if (!Sel || Sel->Recipe == INDEX_NONE || !Me || !Me->Interactor || !Station.IsValid() || !CanCraft(*Recipes[Sel->Recipe]))
	{
		return;
	}
	const FNWRecipe& R = *Recipes[Sel->Recipe];
	HaveBefore = Me->Inventory ? Me->Inventory->CountOf(R.Out) : 0;
	RollBase = R.Qty * Count;
	Me->Interactor->ServerRefine(Station->Type, R.Id, Count);
	State = EState::Roll;
	RollStart = Now();
	Count = 1;
}

void FNWRefiningMenu::Wheel(float Delta)
{
	if (State == EState::Select)
	{
		Scroll = FMath::Clamp(Scroll - int32(Delta) * 3, 0, FMath::Max(0, Rows.Num() - VisibleRows));
	}
}

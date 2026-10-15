#include "NWRefiningMenu.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "NWCharacter.h"
#include "NWCity.h"
#include "NWCrafting.h"
#include "NWInteractable.h"
#include "NWInventoryComponent.h"
#include "NWItems.h"
#include "NWItemRules.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "NWPlayerController.h"
#include "NWProgressionComponent.h"
#include "NWText.h"

namespace
{
	// Game colors (_common/uistyle.lua; docs/game/ui/refining.md §2).
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
	constexpr float RowStep = 46.f;          // UiDynamicScrollBox: prototype 449×46
	constexpr int32 VisibleRows = 16;       // 16 whole rows fit down to the bottom edge of the list (the 17th was cut off)
	constexpr double RollCount = 1.0;        // craftroll.lua: duration
	constexpr double RollDelay = 0.5;        // ProgressionContainer: 0.5 s delay
	constexpr double RollEnd = 2.2;          // end of the count + Base/Bonus (0.2 + 0.5)
	// heroImageSequence/heroImageSequenceDisabled: UiFlipbookAnimation, 30 frames at 30 fps, looping (craftingscreenv4).
	constexpr int32 LoopFrames = 30;
	constexpr double LoopFps = 30.0;
	// button.lua repeatParamsDefault (repeatEnabled in craftquantitywidget.lua): waits 0.5 s, one step every 0.1 s;
	// after 1.8 s the step doubles every 1.5 s, up to 16.
	constexpr double RepeatInitialWait = 0.5;
	constexpr double RepeatPressInterval = 0.1;
	constexpr double RepeatAccelAfter = 1.8;
	constexpr double RepeatQuantityInterval = 1.5;
	constexpr int32 RepeatFactor = 2;
	constexpr int32 RepeatMaxStep = 16;

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
	// Swaps the texture without touching the cache: FNWUILayout reloads when TextureOverride changes (CachedFor).
	void SetTexture(FNWUIElement* El, const FString& T) { if (El) { El->TextureOverride = T; } }
	// Opacity animated by the Lua (UiFaderBus.SetFadeValue): the canvas keeps the initial value in Fade.
	void SetFade(FNWUIElement* El, float F) { if (El) { El->Fade = F; } }

	// UiTransformBus SetLocalPosition + size (ScriptedEntityTweener x/y/w/h) for a point-anchored element.
	void SetLocalRect(FNWUIElement* El, float X, float Y, float W, float H)
	{
		if (El)
		{
			El->Offsets = FVector4f(X - W * El->Pivot.X, Y - H * El->Pivot.Y, X + W * (1.f - El->Pivot.X), Y + H * (1.f - El->Pivot.Y));
		}
	}
	// Station icon (tradeskillscommon.lua, the station's icon).
	const TCHAR* StationIcon(const FString& Type)
	{
		return Type == TEXT("carpentry") ? TEXT("woodshop") : Type == TEXT("weaving") ? TEXT("loom") : Type == TEXT("masonry") ? TEXT("mason")
			: Type == TEXT("tanning") ? TEXT("tanning") : TEXT("smelter");
	}

	FString Loc(const TCHAR* Key) { return NWText::Get(Key); }
	// Territory name of the station's city (javelindata_territorydefinitions: @Everfall, @Windsward), from the
	// `territorio` key of the city in tools/governanca.json.
	FString TerritoryName(const FString& City)
	{
		// Offline capture: station without a city shows Everfall.
		const FString* Key = NWCity::Rules().CityTerritory.Find(City.IsEmpty() ? FString(TEXT("queda_eterna")) : City);
		return Key ? NWText::Get(*Key) : City;
	}
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
	if (!Init(Ui, TEXT("Data/UI/refining_layout.json")) || !Init(Head, TEXT("Data/UI/screen_header_layout.json"))
		|| !Init(Help, TEXT("Data/UI/helpbar_layout.json")))
	{
		return false;
	}
	// The button frames load all at once on open: loading one per frame in the middle of the loop stutters the screen.
	for (int32 i = 0; i < LoopFrames; ++i)
	{
		FNWUILayout::GameTexture(Tex(*FString::Printf(TEXT("slices/button/blue_loop/blue_%02d.dds"), i)));
		FNWUILayout::GameTexture(Tex(*FString::Printf(TEXT("slices/button/gray_loop/gray_%02d.dds"), i)));
	}
	return true;
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
	// Family and group in the game's order (craftingtreeview.lua: sortingIndex from craftingsortinfo.lua); within the
	// group, by level (Sort by Level, the screen's default).
	Recipes.StableSort([](const FNWRecipe& A, const FNWRecipe& B)
	{
		if (A.CategoryOrder != B.CategoryOrder) { return A.CategoryOrder < B.CategoryOrder; }
		if (A.Category != B.Category) { return A.Category < B.Category; }
		if (A.GroupOrder != B.GroupOrder) { return A.GroupOrder < B.GroupOrder; }
		return A.Group != B.Group ? A.Group < B.Group : A.Level < B.Level;
	});
	Collapsed.Reset();
	BuildRows();
	Selected = Rows.IndexOfByPredicate([](const FRow& R) { return R.Recipe != INDEX_NONE; });
	Scroll = 0;
	Count = 1;
	Drag = EDrag::None;
	State = EState::Select;
	Sound(TEXT("ui_crafting_intro"));
	bReroll = false;   // each table opens on the recipes (the "Improve items" tab only exists on crafting tables)
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

	// Center panel: result item (RP/Header), no rarity on resources (ItemName #cccccc).
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

	// Ingredients: 7 fixed rows (ingredientrowv4), "have/need", red when missing.
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
			Ic->ImageType = 4;   // square icon in the 118×56 ItemLayout: aspect ratio kept (the game resizes the layout)
			SetTexture(Ic, ID ? ID->Icon : FString());
		}
		SetText(Ui.FindOne(TEXT("refining"), P + TEXT("/Quantity")), FString::Printf(TEXT("%d/%d"), Have, Need),
			Have >= Need ? FLinearColor::White : RedMedium);
		// Name in the canvas style (FONT_STYLE_NIMBUS_MEDIUM_GRAY_50); red only when missing.
		FNWUIElement* IName = Ui.FindOne(TEXT("refining"), P + TEXT("/Quantity/IngredientName"));
		SetText(IName, ID ? ID->Name : Shown.ToString());
		if (IName)
		{
			IName->Tint = Have >= Need ? TOptional<FLinearColor>() : TOptional<FLinearColor>(RedMedium);
		}
	}

	// Right panel: refining block (trade skill level and extra item chance).
	SetText(E(TEXT("Refining/Skill/LevelBg/Level")), FString::Printf(TEXT("%d"), Prog ? Prog->TradeLevel(Skill) : 0));
	SetText(E(TEXT("Refining/Skill/Description")), Loc(*NWTradeskill::TextKey(Skill)));
	TArray<FName> Chosen;
	for (const FNWRecipe::FIngredient& I : R.Ingredients)
	{
		Chosen.Add(C.ChosenFor(Inv, I));
	}
	SetText(E(TEXT("Refining/Bonus/LevelBg/BonusAmt")),
		FString::Printf(TEXT("%.0f%%"), C.BonusChance(R, Prog ? Prog->TradeRollBonus(Skill) : 0, Chosen) * 100.f));

	// Requirements (craftingstatspanel.lua UpdateRequirements): green with a check, red with an x; station and trade skill
	// icon next to the text; rows at y = 81 + 28·n and the block (and the quantity) moves up 28 per row shown.
	auto Req = [this](const TCHAR* Text, const TCHAR* Icon, bool bOk, const FString& Desc)
	{
		SetText(E(Text), Desc, bOk ? Green : RedMedium);
		SetTexture(E(Icon), Tex(bOk ? TEXT("icons/misc/icon_requirement_check.dds") : TEXT("icons/misc/icon_requirement_x.dds")));
	};
	const bool bResources = Max > 0;
	Req(TEXT("ResourcesRequirement/HasResourcesText"), TEXT("ResourcesRequirement/HasResourcesIcon"), bResources,
		Loc(TEXT("ui_has_resources")));
	ANWStation* St = Station.Get();
	// "@<station>_station" (tradeskillscommon.lua requireSubText) + "Tier n" from the recipe's StationType.
	Req(TEXT("StationRequirement/Description"), TEXT("StationRequirementIcon"), St->Tier >= R.Tier,
		FString::Printf(TEXT("%s %s %d"), *Loc(*(R.Station + TEXT("_station"))), *Loc(TEXT("ui_tier")), R.Tier));
	SetTexture(E(TEXT("StationRequirement/Icon")), Tex(*FString::Printf(TEXT("icons/stations/%s.dds"), StationIcon(St->Type))));
	SetFade(E(TEXT("StationRequirement/Icon")), 1.f);
	Req(TEXT("SkillRequirement/Description"), TEXT("SkillRequirementIcon"), bLevel,
		FString::Printf(TEXT("%s %s"), *Loc(*NWTradeskill::TextKey(Skill)),
			*Loc(TEXT("cr_levelrequired")).Replace(TEXT("{requiredSkillLevel}"), *FString::FromInt(R.Level))));
	SetTexture(E(TEXT("SkillRequirement/Icon")), NWTradeskill::Icon(Skill));
	SetFade(E(TEXT("SkillRequirement/Icon")), 1.f);
	const float Lines = 2.f * 28.f;   // station + skill
	if (FNWUIElement* Box = E(TEXT("OffsetContainer/CraftingRequirements")))
	{
		Box->Translate.Y = -Lines;   // -123 - L11 (canvas: -123)
	}
	if (FNWUIElement* Q = E(TEXT("OffsetContainer/Quantity")))
	{
		Q->Translate.Y = -Lines;     // -261 - L11 (canvas: -261)
	}

	// Trade skill fee and XP.
	// craftingstatspanel.lua: "@ui_craft_fee" with the territory and GetLocalizedCurrency (cents), gray (COLOR_GRAY_50).
	// The standing/company discount (GetCraftingFeeOnCurrentTerritory) is native and has no source: base fee × quantity.
	SetText(E(TEXT("OffsetContainer/TaxRate")), Loc(TEXT("ui_craft_fee"))
		.Replace(TEXT("{territoryName}"), *TerritoryName(St ? St->City : FString()))
		.Replace(TEXT("{fee}"), *UNWInventoryComponent::FormatCoins(int64(R.Fee) * Count)), Gray50);
	// craftingstatspanel.lua: "@ui_craft_bonus" (+{amount} {bonus} {bonus2}); amount = reward × GetIngredientCount × quantity
	// (crafting.xp_ingrediente_contagem: sum of the Qty), bonus = trade skill name. No faction buffs or boosts in the port.
	SetText(E(TEXT("OffsetContainer/SkillExpReward")), Loc(TEXT("ui_craft_bonus"))
		.Replace(TEXT("{amount}"), *FString::FromInt(R.XpFor(Count, Prog && !Prog->HasCrafted(R.Id))))
		.Replace(TEXT("{bonus}"), *Loc(*NWTradeskill::TextKey(Skill)))
		.Replace(TEXT(" {bonus2}"), TEXT("")), Yellow);

	// Quantity (craftquantitywidget): minimum 1, maximum by the materials; number above the bar. With time: one at a time.
	const bool bSlider = Max > 1 && !R.bGear;
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
	SetShown(E(TEXT("SliderWithTextInput/CurrentValueTextInput")), !R.bGear);
	if (FNWUIElement* H = E(TEXT("Slider/SliderHandleHolder")))
	{
		H->Translate.X = Max > 1 ? 280.f * float(Count - 1) / float(Max - 1) : 0.f;   // 280 track (SliderTrack)
	}
	// craftquantitywidget.lua: buttons without text, with a centered icon (SetIconPath); SLIDER_STYLE_2 (sliderwithtextinput.lua):
	// 250×40 value box above the center of the bar, Caslon 36 centered.
	for (const TCHAR* B : {TEXT("Quantity/PlusButton"), TEXT("Quantity/MinusButton")})
	{
		const FString P(B);
		SetText(Ui.FindOne(TEXT("refining"), P + TEXT("/buttonText")), FString());
		if (FNWUIElement* Ic = Ui.FindOne(TEXT("refining"), P + TEXT("/buttonText/buttonIconHolder")))
		{
			Ic->Visible = bSlider;
			Ic->Fade = 1.f;
			Ic->TextureOverride = Tex(P.EndsWith(TEXT("PlusButton")) ? TEXT("icons/crafting/icon_crafting_plus.png") : TEXT("icons/crafting/icon_crafting_minus.png"));
			Ic->Translate.X = 27.f + 1.f;   // anchor −44…−10 of the empty text; SetIconPositionX(1)
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

	// CONFECCIONAR (CRAFT) button: blue hero (flipbook blue_loop, 15 frames) when possible; dimmed when not. Weapon, shield
	// and tool: "Fabricar" (Craft).
	const bool bCan = bResources && bLevel;
	const FString ButtonText = R.bGear ? NWText::Get(TEXT("nw.craft.start")) : Loc(TEXT("ui_craft"));
	SetText(E(TEXT("OffsetContainer/CraftButton/buttonText")), FText::FromString(ButtonText).ToUpper().ToString(), bCan ? TanWarm : Gray50);
	SetTexture(E(TEXT("OffsetContainer/CraftButton/frame")), Tex(bCan ? TEXT("slices/button/heroframe2023.dds") : TEXT("slices/button/heroframedisabled2023.dds")));
	SetShown(E(TEXT("OffsetContainer/CraftButton/heroImageSequenceDisabled")), !bCan);
	SetShown(E(TEXT("OffsetContainer/CraftButton/heroImageSequence")), bCan);
	SetShown(E(TEXT("OffsetContainer/CraftButton/buttonFocus")), false);
	SetShown(E(TEXT("OffsetContainer/CraftButton/frameFocus")), false);
	const int32 Frame = int32(Now() * LoopFps) % LoopFrames;
	SetTexture(E(TEXT("OffsetContainer/CraftButton/heroImageSequence")), Tex(*FString::Printf(TEXT("slices/button/blue_loop/blue_%02d.dds"), Frame)));
	SetTexture(E(TEXT("OffsetContainer/CraftButton/heroImageSequenceDisabled")), Tex(*FString::Printf(TEXT("slices/button/gray_loop/gray_%02d.dds"), Frame)));
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
	// X position of each piece by depth (craftingtreeviewitem.lua), relative to the start of the item. The base comes from
	// this frame's Draw, in which the Translate of these pieces was zeroed (Draw).
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
		// Background: header with categoryBgAdd (#4a5d70, 0.3); recipe with itemBg (#e7e7e7, 0.07); selection: blue strip.
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
		// EntryText.w (craftingtreeviewitem.lua): 360 on headers, 245 on a learned recipe; a long name shrinks.
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
		SetFade(At(TEXT("/ListItemBg/SelectedSequenceMask/LastFrame")), 1.f);   // last frame of the selection flipbook
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
			// craftingtreeviewitem.lua; +6 on the text: the port's arrow is drawn without the padding of the game's sprite.
			const float X[3][2] = {{15.f, 36.f}, {36.f, 57.f}, {59.f, 80.f}};
			PlaceX(TEXT("/StateIcon"), X[Row.Depth][0]);
			PlaceX(TEXT("/EntryText"), X[Row.Depth][1]);
		}
		else
		{
			// "[n] name": n = how many of the result the character already has (QueryInventoryCache), no bracket when 0.
			const ANWCharacter* Me = Character.Get();
			const int32 Owned = Me && Me->Inventory ? Me->Inventory->CountOf(R->Out) : 0;
			const FString Name = D ? D->Name : R->Out.ToString();
			Color = CanCraft(*R) ? TanWarm : RedMedium;
			SetText(At(TEXT("/EntryText")), Owned > 0 ? FString::Printf(TEXT("[%d] %s"), Owned, *Name) : Name, Color);
			SetText(At(TEXT("/GearScoreText")), FString::Printf(TEXT("%d"), R->Level), Color);
			if (FNWUIElement* Ti = At(TEXT("/GearScoreText/TradeskillIcon")))
			{
				Ti->Tint = Color;
				Ti->Translate.X = -6.f;   // gap between the icon and the level
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
	// Gear roll panels stay off in refining.
	for (const TCHAR* P : {TEXT("RarityOffset/QualityInfo"), TEXT("RarityOffset/Stats/Weapons"), TEXT("RarityOffset/Stats/PerkContainer")})
	{
		SetShown(E(P), bGearRoll);
	}
	SetShown(E(TEXT("RarityOffset/Stats/Quantity")), !bGearRoll);
	if (FNWUIElement* Off = E(TEXT("RarityOffset/Header/HeaderOffset")))
	{
		Off->Translate.Y = bGearRoll ? -14.f : 0.f;   // SetRollData: HeaderOffset y = −14 on gear
	}
	if (FNWUIElement* Ro = E(TEXT("CraftRollContainer/RarityOffset")))
	{
		Ro->Translate.Y = 0.f;
		SetShown(Ro, true);
	}
	if (bGearRoll)
	{
		DrawGearRoll(R);
		return;
	}
	// craftroll.lua, "Quantity" type: the count rises from 0 to the total in 1 s, after 0.5 s; Base and Bonus at the end.
	const ANWCharacter* Me = Character.Get();
	const double T = Now() - RollStart;
	const int32 Received = Me && Me->Inventory ? FMath::Max(0, Me->Inventory->CountOf(R.Out) - HaveBefore) : 0;
	int32 Total = FMath::Max(Received, RollBase);
	const float K = float(FMath::Clamp((T - RollDelay) / RollCount, 0.0, 1.0));
	const FNWItemDef* D = FNWItemCatalog::Get().Find(R.Out);
	SetText(E(TEXT("RarityOffset/Header/HeaderOffset/ItemName")), D ? D->Name : R.Out.ToString(), TanCol);
	if (FNWUIElement* Icon = E(TEXT("RarityOffset/Header/HeaderOffset/ItemImage")))
	{
		Icon->ImageType = 4;   // item icon in place of the canvas example, with the aspect ratio kept
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
	// RarityOffset finishes rising in 1.4 s: Crafting_Done (docs/game/ui/refining.md §6).
	SoundAt(T, 1.4, 0, TEXT("ui_crafting_complete"));
	if (State == EState::Roll && T > RollEnd)
	{
		State = EState::Done;
	}
}

double FNWRefiningMenu::GearRollLength() const
{
	// Last perk/gem row (0.55 + 0.15 per row) + 0.3 for the entry, or the 1 s count.
	const FNWItem* It = RollResult();
	const int32 Lines = It ? It->Perks.Num() + 1 : 0;
	return FMath::Max(1.0, 0.55 + 0.15 * Lines + 0.3);
}

void FNWRefiningMenu::DrawGearRoll(const FNWRecipe& R)
{
	const FNWItem* It = RollResult();
	const double Now0 = Now();
	auto Fade = [](double T, double Delay, double Dur) { return float(FMath::Clamp((T - Delay) / FMath::Max(Dur, 0.001), 0.0, 1.0)); };
	// Background, window and runes come in as in refining (0.25 + 0.3).
	const double TB = Now0 - RollStart;
	for (const TCHAR* P : {TEXT("CraftRoll/Background"), TEXT("CraftRoll/CraftRollContainer")})
	{
		if (FNWUIElement* El = E(P))
		{
			El->AlphaMul = Fade(TB, 0.25, 0.3);
		}
	}
	FNWUIElement* Ro = E(TEXT("CraftRollContainer/RarityOffset"));
	SetShown(E(TEXT("CraftRollContainer/ProgressionContainer")), false);   // no trade skill XP bar on this screen
	if (!It)
	{
		// Until the item arrives, only the background and the runes (v4: SetSlot). The server rejected (notice on screen):
		// returns in 10 s (v4: OnCraftingFailed).
		SetShown(Ro, false);
		if (TB > 10.0)
		{
			State = EState::Select;
		}
		return;
	}
	if (ResultAt <= 0.0)
	{
		ResultAt = Now0;
		const FNWItem& Before = Character->Interactor->CraftResult.Before;
		// Craft: the count starts 41 to 60 below (SetGearScore). Reroll GS: from the old GS to the new one (up or down).
		GsFrom = RollKind == 255 ? FMath::Max(0, It->GearScore - (FMath::RandRange(1, 20) + 40))
			: RollKind == uint8(ENWImprove::GearScore) ? Before.GearScore : It->GearScore;
	}
	const double T = Now0 - ResultAt;
	const FNWItemDef* D = FNWItemCatalog::Get().Find(It->ItemId);
	const FNWItemRules& Rules = FNWItemRules::Get();
	const FString Class = D ? FNWItemRules::EquipClass(*D) : FString();
	const bool bSlot = Class == TEXT("arma") || Class == TEXT("escudo") || Class == TEXT("armadura");
	const bool bTool = Class == TEXT("ferramenta");
	const int32 Rarity = FMath::Clamp<int32>(It->Rarity, 0, 4);
	const FLinearColor RarityCol = NWRarityColor(uint8(Rarity));

	// Header: icon, name in the rarity color and the quality below the name.
	if (FNWUIElement* H = E(TEXT("RarityOffset/Header")))
	{
		H->AlphaMul = Fade(T, 0.0, 0.2);
	}
	SetText(E(TEXT("RarityOffset/Header/HeaderOffset/ItemName")), D ? D->Name : It->ItemId.ToString(), RarityCol);
	if (FNWUIElement* Icon = E(TEXT("RarityOffset/Header/HeaderOffset/ItemImage")))
	{
		Icon->ImageType = 4;
		SetTexture(Icon, D ? D->Icon : FString());
		Icon->AlphaMul = Fade(T, 0.0, 0.6);
	}
	if (FNWUIElement* Q = E(TEXT("HeaderOffset/ItemName/CraftedFromName")))
	{
		SetShown(Q, It->Quality >= 0.f);
		SetText(Q, NWText::Format(TEXT("nw.item.quality"), {FString::Printf(TEXT("%.1f"), It->Quality)}), Yellow);
		Q->AlphaMul = Fade(T, 0.6, 0.3);
	}
	if (FNWUIElement* Bg = E(TEXT("RarityBgMask/RarityBg")))
	{
		SetTexture(Bg, FString::Printf(TEXT("lyshineui/images/crafting/crafting_itemraritybglarge%d.dds"), Rarity));
		Bg->AlphaMul = Fade(T, 0.0, 0.1);
	}

	// QualityInfo: GS counting in 1 s (linear) and the rarity.
	if (FNWUIElement* Qi = E(TEXT("RarityOffset/QualityInfo")))
	{
		Qi->AlphaMul = Fade(T, 0.0, 0.2);
	}
	const float K = Fade(T, 0.0, 1.0);
	SetText(E(TEXT("GearScoreLabel/GearScore")), FString::FromInt(FMath::FloorToInt(GsFrom + (It->GearScore - GsFrom) * K)));
	for (int32 n = 1; n <= 4; ++n)
	{
		SetShown(E(*FString::Printf(TEXT("RarityBG/RarityEffect%d"), n)), false);   // flipbooks outside the converted layout
	}
	SetText(E(TEXT("RarityLabel/RarityText")), Loc(*FString::Printf(TEXT("raritylevel%d_displayname"), Rarity)), RarityCol);
	if (FNWUIElement* Lc = E(TEXT("RarityBG/RarityLabelContainer")))
	{
		Lc->AlphaMul = Rarity > 0 ? Fade(T, 0.8, 0.1) : 1.f;   // effect 0.5 + 0.3
	}
	if (FNWUIElement* Sun = E(TEXT("RarityLabelContainer/RisingSun")))
	{
		Sun->AlphaMul = Rarity > 0 ? Fade(T, 0.5, 0.3) : 0.f;
	}

	// Weapon and shield: base damage (does not change with GS in the port). Tool: gathering speed.
	FNWUIElement* W = E(TEXT("RarityOffset/Stats/Weapons"));
	SetShown(W, Class == TEXT("arma") || Class == TEXT("escudo") || bTool);
	if (W)
	{
		W->AlphaMul = Fade(T, 0.0, 0.4);
	}
	SetText(E(TEXT("Weapons/PrimaryStatLabel")), Loc(bTool ? TEXT("ui_tooltip_gatherspeed") : TEXT("ui_tooltip_basedamage")));
	SetText(E(TEXT("PrimaryStatLabel/PrimaryBaseValue")), !D ? FString()
		: bTool ? FString::Printf(TEXT("%.0f%%"), D->Efficiency * 100.f) : FString::Printf(TEXT("%.0f"), D->Damage));
	SetShown(E(TEXT("PrimaryBaseValue/PrimaryValue")), false);

	// Armor (RO/Stats/Armor, docs/game/crafting.md "Crafting gear"): recipe base (quality 0) → item resistance, physical
	// and magical equal in the port (docs/game/items.md "Armor and jewelry"). Jewelry (RO/Stats/Trinket): only the divisor.
	const bool bArmor = Class == TEXT("armadura");
	FNWUIElement* Ar = E(TEXT("RarityOffset/Stats/Armor"));
	SetShown(Ar, bArmor);
	SetShown(E(TEXT("RarityOffset/Stats/Trinket")), Class == TEXT("amuleto") || Class == TEXT("anel") || Class == TEXT("brinco"));
	// The layout brings the panel's children turned off (the Lua turns them on): turns on the ones the port uses; glows stay off.
	for (const TCHAR* P : {TEXT("Armor/AbsorptionTitle"), TEXT("Armor/PhysicalLabel"), TEXT("PhysicalLabel/PhysicalValue"),
		TEXT("PhysicalValue/FinalPhysical"), TEXT("FinalPhysical/Arrow"), TEXT("FinalPhysical/ElementalLabel"),
		TEXT("ElementalLabel/ElementalValue"), TEXT("ElementalValue/FinalElemental"), TEXT("FinalElemental/Arrow"),
		TEXT("Armor/Divider"), TEXT("Trinket/Divider")})
	{
		SetShown(E(P), true);
	}
	if (bArmor && D)
	{
		Ar->AlphaMul = Fade(T, 0.0, 0.4);
		const FString Base = FString::FromInt(FMath::FloorToInt(Rules.Resistance(*D, 0.f)));
		const FString Final = FString::FromInt(FMath::FloorToInt(Rules.Resistance(*D, It->Quality)));
		SetText(E(TEXT("Armor/AbsorptionTitle")), NWText::Get(TEXT("nw.item.resistance")));
		SetText(E(TEXT("FinalPhysical/ElementalLabel")), NWText::Get(TEXT("nw.item.magic_only")));
		SetText(E(TEXT("PhysicalLabel/PhysicalValue")), Base);
		SetText(E(TEXT("ElementalLabel/ElementalValue")), Base);
		for (const TCHAR* P : {TEXT("PhysicalValue/FinalPhysical"), TEXT("ElementalValue/FinalElemental")})
		{
			if (FNWUIElement* F = E(P))
			{
				SetText(F, Final);
				F->AlphaMul = Fade(T, 0.3, 0.3);   // FinalPhysical 0→1 in 0.3
			}
		}
	}

	// Attribute (CraftingAttribute) at 0.4; perks at 0.55 + 0.15·k; gem after. Positions from the table by number of perks
	// (craftroll.lua 4535–5088).
	SetShown(E(TEXT("Stats/NoPerkLabel")), false);
	const int32 NumPerks = It->Perks.Num();
	// Sounds: rarity at 0.5 (Crafting_Rarity_n), each perk at 0.55 + 0.15·k + 0.2 (Crafting_Perk_Show), gem after
	// (Crafting_GemSlot_Show).
	SoundAt(T, 0.5, 1, FString::Printf(TEXT("ui_crafting_rarity_%d"), Rarity));
	for (int32 k = 0; k < NumPerks && k < 8; ++k)
	{
		SoundAt(T, 0.55 + 0.15 * k + 0.2, 2 + k, TEXT("ui_crafting_perk_show"));
	}
	if (bSlot)
	{
		SoundAt(T, 0.55 + 0.15 * NumPerks + 0.2, 10, TEXT("ui_crafting_gemslot_show"));
	}
	if (Ro)
	{
		SetShown(Ro, true);
		Ro->Translate.Y = NumPerks <= 2 ? (bSlot ? 40.f : 70.f) : (bSlot ? 0.f : 46.f);
	}
	auto Row = [&](FNWUIElement* El, double Delay)
	{
		if (El)
		{
			El->AlphaMul = Fade(T, Delay, 0.3);
		}
	};
	FNWUIElement* Attrs = E(TEXT("PerkContainer/AttributesContainer"));
	const bool bAttr = It->Attr != 255 && Rules.Attributes.IsValidIndex(It->Attr);
	SetShown(Attrs, bAttr);
	SetShown(E(TEXT("AttributesContainer/CraftingAttribute")), bAttr);
	SetShown(E(TEXT("AttributesContainer/CraftingAttribute2")), false);
	if (bAttr)
	{
		Row(Attrs, 0.4);
		SetText(E(TEXT("CraftingAttribute/UpArrow/AttributeText")), FString::Printf(TEXT("+%d %s"), Rules.AttrValue(It->GearScore),
			*NWText::Get(TEXT("ui_") + Rules.Attributes[It->Attr].ToString())));
	}
	for (int32 k = 0; k < 5; ++k)
	{
		const FString P = k == 0 ? FString(TEXT("PerkContainer/PerkChance")) : FString::Printf(TEXT("PerkContainer/PerkChance%d"), k + 1);
		FNWUIElement* El = E(*P);
		SetShown(El, k < NumPerks);
		if (k >= NumPerks)
		{
			continue;
		}
		const FName Perk = It->Perks[k];
		const FNWItemRules::FPerk* PD = Rules.Perks.Find(Perk);
		const double Delay = 0.55 + 0.15 * k;
		Row(El, Delay);
		// Name on top and effect below, inside the 75 strip; a long effect (ability perk) in a smaller font.
		const FString Desc = Rules.PerkText(Perk, It->Quality, false);
		if (FNWUIElement* Name = E(*(P + TEXT("/Bg/PerkName"))))
		{
			SetText(Name, Rules.PerkText(Perk, It->Quality, true));
			Name->Translate.Y = -20.f;
		}
		if (FNWUIElement* DescEl = E(*(P + TEXT("/Bg/PerkDesc"))))
		{
			SetText(DescEl, Desc);
			DescEl->Translate.Y = -6.f;
			DescEl->FontSize = Desc.Len() > 60 ? 15.f : 20.f;
		}
		SetTexture(E(*(P + TEXT("/PerkImageBg/Mask/PerkImage"))), PD ? PD->Icon : FString());
		if (FNWUIElement* Cover = E(*(P + TEXT("/PerkImageBg/PerkImageCover"))))
		{
			Cover->AlphaMul = 1.f - Fade(T, Delay + 0.1, 0.3);   // ShowPerk: cover 1→0
		}
	}
	FNWUIElement* Gem = E(TEXT("PerkContainer/GemPerkSlot"));
	SetShown(Gem, bSlot);
	if (bSlot && Gem)
	{
		static const float GemY[] = {112.f, 112.f, 186.f, 260.f, 334.f, 408.f};
		Gem->Translate.Y = GemY[FMath::Clamp(NumPerks, 0, 5)] - 408.f;   // canvas: 408
		Row(Gem, 0.55 + 0.15 * NumPerks);
		// With a gem: its name and icon and the effects (docs/game/items.md "Slot and gems"); empty: the game's socket.
		const FNWItemDef* GD = It->Gem.IsNone() ? nullptr : FNWItemCatalog::Get().Find(It->Gem);
		SetText(E(TEXT("GemPerkSlot/Bg/PerkName")), GD ? GD->Name : NWText::Get(TEXT("nw.item.gem_empty")));
		SetText(E(TEXT("GemPerkSlot/Bg/PerkDesc")), GD ? FString::Join(FNWItemRules::Get().GemLines(It->Gem), TEXT("  ")) : FString());
		SetShown(E(TEXT("GemPerkSlot/PerkImageBg/PerkImage")), GD != nullptr);
		SetTexture(E(TEXT("GemPerkSlot/PerkImageBg/PerkImage")), GD ? GD->Icon : FString());
		if (FNWUIElement* Cover = E(TEXT("GemPerkSlot/PerkImageBg/PerkImageCover")))
		{
			Cover->AlphaMul = 1.f - Fade(T, 0.65 + 0.15 * NumPerks, 0.3);   // ShowPerk(d, true): cover 1→0, the socket stays
		}
	}
	// SetSlot → AdvanceStep at 0.2 s: end state (the click finishes the animations before returning).
	if (State == EState::Roll && T > 0.2)
	{
		State = EState::Done;
	}
}

void FNWRefiningMenu::DrawHelp(UCanvas* Canvas)
{
	// Footer (helpbar): ENTER Chat on the left; on the right, from right to left, ESC Back and (max > 1) SPACE
	// Max quantity. The key box follows its text; the label comes 12 after the box.
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
	const float ProtoLeft = ProtoRight - 200.f;   // ActionDisplay: -200..0 on the right
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
	float X = ProtoRight - 40.f;   // same margin as ENTER on the left
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
	// Gear: the end buttons take the place of "Click anywhere" (clicking outside them still works).
	SetShown(E(TEXT("CraftDoneWindow/ContinueText")), !bGearRoll);
	if (FNWUIElement* Bg = E(TEXT("_root/CraftingScreen/BackgroundStep1")))
	{
		Bg->AlphaMul = bRoll ? float(FMath::Clamp((Now() - RollStart) / 0.35, 0.0, 1.0)) : 0.f;
	}
	if (FNWUIElement* Proto = E(TEXT("Mask/Content/CraftingTreeViewItem")))
	{
		Proto->Visible = false;   // template item: drawn per row in DrawRows
	}
	for (const TCHAR* Off : {TEXT("CraftingTreeViewItem/StateIcon"), TEXT("CraftingTreeViewItem/EntryText"),
		TEXT("CraftingTreeViewItem/RarityFrame"), TEXT("CraftingTreeViewItem/TypeIcon")})
	{
		if (FNWUIElement* El = E(Off))
		{
			El->Translate.X = 0.f;   // DrawRows measures the base position in this Draw
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
	DoneButtons.Reset();
	if (bGearRoll && State == EState::Done)
	{
		DrawDoneButtons(Canvas, Mouse);
	}
	if (!bRoll)
	{
		DrawRows(Canvas, Mouse);
		if (R && R->bGear)
		{
			DrawCraftInfo(Canvas, *R, Mouse);
		}
		DrawTab(Canvas, Mouse);
		if (bReroll)
		{
			DrawReroll(Canvas, Mouse);
		}
	}
	// Title bar: station name and coins.
	SetText(Head.FindOne(TEXT("screen_header"), TEXT("HeaderHolder/HeaderText")), St->Interact->DisplayName.ToString());
	SetText(Head.FindOne(TEXT("screen_header"), TEXT("CurrencyDisplay/CurrencyAmount")),
		UNWInventoryComponent::FormatCoins(Me->Inventory ? Me->Inventory->Coins : 0));
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
	for (const TPair<FBox2f, EDoneButton>& B : DoneButtons)
	{
		if (!B.Key.IsInside(M))
		{
			continue;
		}
		if (B.Value == EDoneButton::Equip)
		{
			const FNWItem* It = ResultInBag();
			const FNWItemDef* D = It ? FNWItemCatalog::Get().Find(It->ItemId) : nullptr;
			const int32 To = D && Me && Me->Inventory ? Me->Inventory->BestEquipSlot(*It) : INDEX_NONE;
			if (To != INDEX_NONE)
			{
				Me->Inventory->ServerMove(ENWItemContainer::Bag, It->Slot, ENWItemContainer::Equip, To);
			}
			State = EState::Select;
		}
		else if (B.Value == EDoneButton::Exit)
		{
			// OnExitPostCrafting: closes the station.
			if (ANWPlayerController* PC = Me ? Cast<ANWPlayerController>(Me->GetController()) : nullptr)
			{
				PC->CloseMenus();
			}
		}
		else
		{
			State = EState::Select;
		}
		return true;
	}
	if (bGearRoll && State != EState::Select)
	{
		// TrySkip: with an animation running, finishes everything; after that returns to the table. Without the item yet, waits.
		if (ResultAt > 0.0 && Now() - ResultAt < GearRollLength())
		{
			ResultAt = Now() - GearRollLength();
			SoundsDone = ~0u;   // skipping finishes the animations without playing everything at once
		}
		else if (ResultAt > 0.0)
		{
			State = EState::Select;
		}
		return true;
	}
	if (State == EState::Done)
	{
		State = EState::Select;   // a click anywhere returns to the station (CraftCompleteReturnToStation)
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
	if (TabRect.bIsValid && TabRect.IsInside(M))
	{
		bReroll = !bReroll;
		return true;
	}
	if (bReroll)
	{
		for (const FImproveButton& B : MatButtons)
		{
			if (B.Rect.IsInside(M))
			{
				ImproveMat.Add(B.Slot, B.Mat);
				Sound(TEXT("ui_crafting_item_select"));
				return true;
			}
		}
		for (const FImproveButton& B : RerollButtons)
		{
			if (B.Rect.IsInside(M) && Me && Me->Interactor && Station.IsValid())
			{
				// Same count as crafting, up to the result the server returns (ClientCraftResult).
				const FNWItem* It = Me->Inventory->Items.FindByPredicate([&B](const FNWItem& I) { return I.Container == ENWItemContainer::Bag && I.Slot == B.Slot; });
				StartResultRoll(It ? It->ItemId : FName(), B.Kind);
				Me->Interactor->ServerImprove(Station->Type, B.Slot, B.Kind, B.Mat);
				return true;
			}
		}
		return true;   // the tab covers the recipe screen
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
				Sound(TEXT("ui_crafting_item_select"));
			}
			else
			{
				// Header: opens/closes (StateIcon); keeps the chosen recipe if it is still in the list.
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
	const bool bMinus = Ui.RectOf(E(TEXT("Quantity/MinusButton"))).IsInside(M);
	if (bMinus || Ui.RectOf(E(TEXT("Quantity/PlusButton"))).IsInside(M))
	{
		// button.lua: the click takes one step; when held, Hold repeats (OnTick).
		Step(bMinus ? -1 : 1);
		Drag = bMinus ? EDrag::Minus : EDrag::Plus;
		HoldLast = Now();
		HoldInitial = HoldQuantity = HoldPress = 0.0;
		HoldStep = 1;
	}
	else if (Ui.RectOf(E(TEXT("OffsetContainer/Quantity/Maximum"))).IsInside(M))
	{
		Count = Max;
	}
	else if (Max > 1 && (Ui.RectOf(E(TEXT("SliderWithTextInput/Slider"))).IsInside(M)
		|| Ui.RectOf(E(TEXT("Slider/SliderHandleHolder"))).ExpandBy(8.f).IsInside(M)))
	{
		// Click on the bar or the marker: jumps to the point and follows the mouse while held (snapToIntegers).
		Drag = EDrag::Slider;
		SliderTo(M.X);
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
	if (!Sel || Sel->Recipe == INDEX_NONE || !Me || !Me->Interactor || !Station.IsValid()
		|| !CanCraft(*Recipes[Sel->Recipe]))
	{
		return;
	}
	const FNWRecipe& R = *Recipes[Sel->Recipe];
	Sound(TEXT("ui_crafting_select"));   // Crafting_Button_Select
	SoundsDone = 0;
	if (R.bGear)
	{
		// Immediately: the item goes to the bag (or only XP in training). With an item, the game's count rises to its GS.
		// The wait starts before the request: with a local server the result comes back inside the call itself.
		const int32 Level = Me->Progression ? Me->Progression->TradeLevel(NWTradeskill::Index(R.Skill)) : 0;
		if (Level >= FNWItemRules::Get().CraftLevel)
		{
			StartResultRoll(R.Out, 255);
		}
		Me->Interactor->ServerCraft(Station->Type, R.Id);
		return;
	}
	bGearRoll = false;
	HaveBefore = Me->Inventory ? Me->Inventory->CountOf(R.Out) : 0;
	RollBase = R.Qty * Count;
	Me->Interactor->ServerRefine(Station->Type, R.Id, Count);
	State = EState::Roll;
	RollStart = Now();
	Count = 1;
}

void FNWRefiningMenu::StartResultRoll(FName ItemId, uint8 Kind)
{
	const ANWCharacter* Me = Character.Get();
	WaitSeq = Me && Me->Interactor ? Me->Interactor->CraftResult.Seq : 0;
	RollKind = Kind;
	RollItem = ItemId;
	ResultAt = 0.0;
	bGearRoll = true;
	State = EState::Roll;
	RollStart = Now();
	SoundsDone = 0;
}

const FNWItem* FNWRefiningMenu::RollResult() const
{
	const ANWCharacter* Me = Character.Get();
	return Me && Me->Interactor && Me->Interactor->CraftResult.Seq > WaitSeq ? &Me->Interactor->CraftResult.After : nullptr;
}

void FNWRefiningMenu::SliderTo(float MouseX)
{
	const int32 Max = MaxCount();
	if (Max <= 1)
	{
		return;
	}
	const FBox2f Track = Ui.RectOf(E(TEXT("Slider/SliderTrack")));
	const float K = FMath::Clamp((MouseX - Track.Min.X) / FMath::Max(1.f, Track.GetSize().X), 0.f, 1.f);
	Count = 1 + FMath::RoundToInt(K * (Max - 1));
}

void FNWRefiningMenu::Step(int32 Delta)
{
	// craftquantitywidget.lua IncrementSlider/DecrementSlider: adds GetQuantityToUpdate, between 1 and the maximum.
	Count = FMath::Clamp(Count + Delta, 1, FMath::Max(1, MaxCount()));
}

void FNWRefiningMenu::Hold(const FVector2D& Mouse, bool bDown)
{
	if (!IsOpen() || State != EState::Select || !bDown)
	{
		Drag = EDrag::None;
		return;
	}
	const double T = Now();
	const double Dt = FMath::Max(0.0, T - HoldLast);
	HoldLast = T;
	if (Drag == EDrag::Slider)
	{
		SliderTo(float(Mouse.X));
		return;
	}
	if (Drag == EDrag::None)
	{
		return;
	}
	// button.lua OnTick.
	HoldInitial += Dt;
	if (HoldInitial <= RepeatInitialWait)
	{
		return;
	}
	HoldQuantity += Dt;
	HoldPress += Dt;
	if (HoldInitial > RepeatAccelAfter && HoldQuantity > RepeatQuantityInterval)
	{
		HoldQuantity -= RepeatQuantityInterval;
		HoldStep = FMath::Min(HoldStep * RepeatFactor, RepeatMaxStep);
	}
	while (HoldPress > RepeatPressInterval)
	{
		HoldPress -= RepeatPressInterval;
		Step(Drag == EDrag::Minus ? -HoldStep : HoldStep);
	}
}

void FNWRefiningMenu::Space()
{
	// uishared.xml, actionmap "crafting": tryskip = space. craftingscreenv4.lua TrySkip: on selection, SetMaxCraft;
	// during the count, skips to the end; on the result, CraftCompleteReturnToStation.
	if (!IsOpen())
	{
		return;
	}
	if (State == EState::Select)
	{
		Count = FMath::Max(1, MaxCount());
	}
	else if (State == EState::Roll)
	{
		RollStart = FMath::Min(RollStart, Now() - (RollDelay + RollCount + 0.2));
	}
	else
	{
		State = EState::Select;
	}
}

void FNWRefiningMenu::Wheel(float Delta)
{
	if (State == EState::Select)
	{
		Scroll = FMath::Clamp(Scroll - int32(Delta) * 3, 0, FMath::Max(0, Rows.Num() - VisibleRows));
	}
}

void FNWRefiningMenu::DrawCraftInfo(UCanvas* Canvas, const FNWRecipe& R, const FVector2D& Mouse)
{
	// Above the button: time, GS and quality range (or the training notice), spectrum and the craft in progress.
	ANWCharacter* Me = Character.Get();
	const FBox2f Button = Ui.RectOf(E(TEXT("OffsetContainer/CraftButton")));
	if (!Me || !Me->Progression || !Button.bIsValid)
	{
		return;
	}
	// In the empty space of the right column, between the potential bonus and the quantity (which is hidden for timed recipes).
	const float S0 = Canvas->ClipY / 1080.f;
	const FVector2f Origin(Button.Min.X, Button.Min.Y - 455.f * S0);
	const FNWItemRules& Rules = FNWItemRules::Get();
	const float S = Canvas->ClipY / 1080.f;
	UFont* Font = FNWUILayout::GameFont(0);
	const FLinearColor Tan(FColor(0xEB, 0xE3, 0xD6)), Gold(FColor(0xED, 0xC6, 0x3E));
	float Y = Origin.Y;
	// Text size as in the faithful screens: font line height measured at 1.
	float W1 = 0.f, H1 = 1.f;
	Canvas->TextSize(Font, TEXT("Ag"), W1, H1, 1.f, 1.f);
	const float Scale = 24.f * S / FMath::Max(1.f, H1);
	auto Line = [&](const FString& Text, const FLinearColor& Color)
	{
		FCanvasTextItem T(FVector2D(Origin.X, Y), FText::FromString(Text), Font, Color);
		T.Scale = FVector2D(Scale, Scale);
		T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f));
		Canvas->DrawItem(T);
		Y += 32.f * S;
	};
	const int32 Skill = NWTradeskill::Index(R.Skill);
	const int32 Level = Me->Progression->TradeLevel(Skill);
	if (Level < Rules.CraftLevel)
	{
		Line(NWText::Format(TEXT("nw.craft.training"), {FString::FromInt(Rules.CraftLevel)}), Gold);
		return;
	}
	const FIntPoint Range = FNWCrafting::Get().GsRange(Me, R);
	Line(NWText::Format(TEXT("nw.craft.gs_range"), {FString::FromInt(Range.X), FString::FromInt(Range.Y)}), Tan);
	Line(NWText::Format(TEXT("nw.craft.quality_min"), {FString::Printf(TEXT("%.1f"), Rules.QualityMin(Level, Me->Progression->TradeMaxLevel(Skill)))}), Tan);
}

namespace
{
	void CraftText(UCanvas* Canvas, const FString& Text, float X, float Y, float Size, const FLinearColor& Color, int32 Style = 0)
	{
		UFont* Font = FNWUILayout::GameFont(Style);
		if (!Font || Text.IsEmpty())
		{
			return;
		}
		float W1 = 0.f, H1 = 1.f;
		Canvas->TextSize(Font, TEXT("Ag"), W1, H1, 1.f, 1.f);
		const float Scale = Size * (Canvas->ClipY / 1080.f) / FMath::Max(1.f, H1);
		FCanvasTextItem T(FVector2D(X, Y), FText::FromString(Text), Font, Color);
		T.Scale = FVector2D(Scale, Scale);
		T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f));
		Canvas->DrawItem(T);
	}

	void CraftButton(UCanvas* Canvas, const FBox2f& R, const FString& Label, bool bHot, bool bEnabled)
	{
		auto Tile = [&](const TCHAR* Path, const FLinearColor& Tint)
		{
			if (UTexture2D* Tex = FNWUILayout::GameTexture(Path))
			{
				FCanvasTileItem T(FVector2D(R.Min), Tex->GetResource(), FVector2D(R.GetSize()), Tint);
				T.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(T);
			}
		};
		Tile(TEXT("lyshineui/images/slices/button/buttonbg.dds"), FLinearColor(FColor(0x2C, 0x25, 0x1B)));
		Tile(TEXT("lyshineui/images/slices/button/primarybuttonframe.dds"), FLinearColor(1.f, 1.f, 1.f, bEnabled ? (bHot ? 1.f : 0.75f) : 0.35f));
		const float S = Canvas->ClipY / 1080.f;
		UFont* Font = FNWUILayout::GameFont(0);
		float W = 0.f, H = 1.f, W1 = 0.f, H1 = 1.f;
		Canvas->TextSize(Font, TEXT("Ag"), W1, H1, 1.f, 1.f);
		const float Scale = 22.f * S / FMath::Max(1.f, H1);
		Canvas->TextSize(Font, *Label, W, H, Scale, Scale);
		CraftText(Canvas, Label, R.GetCenter().X - W * 0.5f, R.GetCenter().Y - H * 0.5f, 22.f,
			bEnabled ? (bHot ? FLinearColor::White : FLinearColor(FColor(0xEB, 0xE3, 0xD6))) : FLinearColor(0.5f, 0.5f, 0.5f));
	}
}

void FNWRefiningMenu::DrawTab(UCanvas* Canvas, const FVector2D& Mouse)
{
	// Only on crafting tables (timed recipes): button that toggles between recipes and "Reroll attribute".
	TabRect = FBox2f(ForceInit);
	if (!Recipes.ContainsByPredicate([](const FNWRecipe* R) { return R->bGear; }))
	{
		return;
	}
	const FBox2f Button = Ui.RectOf(E(TEXT("OffsetContainer/CraftButton")));
	if (!Button.bIsValid)
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.f;
	TabRect = FBox2f(FVector2f(Button.Max.X - 260.f * S, 98.f * S), FVector2f(Button.Max.X, 140.f * S));
	CraftButton(Canvas, TabRect, NWText::Get(bReroll ? TEXT("nw.craft.recipes") : TEXT("nw.craft.improve")), TabRect.IsInside(FVector2f(Mouse)), true);
}

void FNWRefiningMenu::DrawReroll(UCanvas* Canvas, const FVector2D& Mouse)
{
	RerollButtons.Reset();
	MatButtons.Reset();
	ANWCharacter* Me = Character.Get();
	ANWStation* St = Station.Get();
	const FBox2f Button = Ui.RectOf(E(TEXT("OffsetContainer/CraftButton")));
	if (!Me || !Me->Inventory || !St || !Button.bIsValid)
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.f;
	const FLinearColor Tan(FColor(0xEB, 0xE3, 0xD6)), Gray(FColor(0xB2, 0xAE, 0xA1));
	// Covers the recipe panel and the right one.
	const FBox2f Panel(FVector2f(Button.Min.X - 752.f * S, 150.f * S), FVector2f(Button.Max.X + 36.f * S, 1010.f * S));
	// Opaque: the recipe screen below must not show through.
	FCanvasTileItem Bg(FVector2D(Panel.Min), FVector2D(Panel.GetSize()), FLinearColor(0.02f, 0.02f, 0.03f, 1.f));
	Bg.BlendMode = SE_BLEND_Opaque;
	Canvas->DrawItem(Bg);
	FCanvasBoxItem Frame(FVector2D(Panel.Min), FVector2D(Panel.GetSize()));
	Frame.SetColor(FLinearColor(0.42f, 0.38f, 0.28f));
	Canvas->DrawItem(Frame);
	CraftText(Canvas, FText::FromString(NWText::Get(TEXT("nw.craft.improve"))).ToUpper().ToString(), Panel.Min.X + 30.f * S, Panel.Min.Y + 22.f * S, 30.f, Tan, 3);

	const FNWItemRules& Rules = FNWItemRules::Get();
	const FLinearColor Gold(FColor(0xED, 0xC6, 0x3E));
	float Y = Panel.Min.Y + 80.f * S;
	int32 Shown = 0;
	for (const FNWItem& I : Me->Inventory->Items)
	{
		const FNWRecipe* R = I.Container == ENWItemContainer::Bag && I.Quality >= 0.f ? FNWCrafting::Get().RecipeFor(I.ItemId, St->Type) : nullptr;
		const FNWItemDef* D = R ? FNWItemCatalog::Get().Find(I.ItemId) : nullptr;
		if (!D || Y > Panel.Max.Y - 130.f * S)
		{
			continue;
		}
		++Shown;
		if (UTexture2D* Icon = FNWUILayout::GameTexture(D->Icon))
		{
			FCanvasTileItem T(FVector2D(Panel.Min.X + 30.f * S, Y), Icon->GetResource(), FVector2D(64.f * S, 64.f * S), FLinearColor::White);
			T.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(T);
		}
		const FString Attr = Rules.Attributes.IsValidIndex(I.Attr) ? NWText::Get(TEXT("ui_") + Rules.Attributes[I.Attr].ToString()) : FString();
		CraftText(Canvas, D->Name, Panel.Min.X + 108.f * S, Y - 4.f * S, 24.f, Tan, 1);
		CraftText(Canvas, FString::Printf(TEXT("GS %d  ·  +%d %s  ·  %s"), I.GearScore, Rules.AttrValue(I.GearScore), *Attr,
			*NWText::Format(TEXT("nw.item.quality"), {FString::Printf(TEXT("%.1f"), I.Quality)})), Panel.Min.X + 108.f * S, Y + 26.f * S, 21.f, Gold);
		// Materials that pay for the improvements (ingot, plank, the recipe's leather...): a click selects; with no selection,
		// the first one that can reroll the GS.
		const FNWCrafting& Crafting = FNWCrafting::Get();
		const TArray<FName> Mats = Crafting.ImproveMaterials(*R);
		uint8 Mat = 255;
		if (const uint8* Chosen = ImproveMat.Find(I.Slot); Chosen && Mats.IsValidIndex(*Chosen) && !Mats[*Chosen].IsNone())
		{
			Mat = *Chosen;
		}
		for (int32 m = 0; m < Mats.Num() && Mat == 255; ++m)
		{
			if (!Mats[m].IsNone() && Me->Inventory->CountOf(Mats[m]) >= Crafting.ImproveCost(*R, I, ENWImprove::GearScore, uint8(m)).Qty)
			{
				Mat = uint8(m);
			}
		}
		for (int32 m = 0; m < Mats.Num() && Mat == 255; ++m)
		{
			Mat = Mats[m].IsNone() ? Mat : uint8(m);
		}
		if (Mat == 255)
		{
			Y += 130.f * S;
			continue;
		}
		const FLinearColor Red(FColor(0xF2, 0x5D, 0x5D)), Frame0(0.42f, 0.38f, 0.28f);
		float MX = Panel.Min.X + 108.f * S;
		for (int32 m = 0; m < Mats.Num(); ++m)
		{
			const FNWItemDef* MD = Mats[m].IsNone() ? nullptr : FNWItemCatalog::Get().Find(Mats[m]);
			if (!MD)
			{
				continue;
			}
			const FBox2f Chip(FVector2f(MX, Y + 62.f * S), FVector2f(MX + 104.f * S, Y + 102.f * S));
			const bool bSel = m == Mat, bHover = Chip.IsInside(FVector2f(Mouse));
			FCanvasTileItem ChipBg(FVector2D(Chip.Min), FVector2D(Chip.GetSize()), FLinearColor(1.f, 1.f, 1.f, bSel ? 0.08f : bHover ? 0.05f : 0.f));
			ChipBg.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(ChipBg);
			FCanvasBoxItem ChipFrame(FVector2D(Chip.Min), FVector2D(Chip.GetSize()));
			ChipFrame.SetColor(bSel ? Gold : Frame0);
			Canvas->DrawItem(ChipFrame);
			if (UTexture2D* MatIcon = FNWUILayout::GameTexture(MD->Icon))
			{
				FCanvasTileItem T(FVector2D(Chip.Min.X + 4.f * S, Chip.Min.Y + 4.f * S), MatIcon->GetResource(), FVector2D(32.f * S, 32.f * S), FLinearColor::White);
				T.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(T);
			}
			CraftText(Canvas, FString::FromInt(Me->Inventory->CountOf(Mats[m])), Chip.Min.X + 44.f * S, Chip.Min.Y + 7.f * S, 22.f, bSel ? Tan : Gray);
			MatButtons.Add({Chip, I.Slot, 0, uint8(m)});
			MX += 114.f * S;
		}
		const FNWItemDef* MD = FNWItemCatalog::Get().Find(Mats[Mat]);
		const int32 Have = Me->Inventory->CountOf(Mats[Mat]);
		// Three buttons on the right: reroll attribute, reroll GS, unbind; below each one, the material icon and
		// how much it costs (red if missing); on attribute the chance, on GS the limits.
		float BX = Panel.Max.X - 30.f * S;
		for (int32 k = int32(ENWImprove::Num) - 1; k >= 0; --k)
		{
			const ENWImprove Kind = ENWImprove(k);
			const FNWRecipe::FIngredient Cost = Crafting.ImproveCost(*R, I, Kind, Mat);
			const FString Label = Kind == ENWImprove::Attribute ? NWText::Get(TEXT("nw.craft.improve_attr"))
				: Kind == ENWImprove::GearScore ? NWText::Get(TEXT("nw.craft.improve_gs"))
				: NWText::Get(I.Bind == 2 ? TEXT("nw.craft.improve_unbind") : TEXT("nw.item.bind_equip"));
			const bool bOk = Cost.Qty > 0 && Have >= Cost.Qty;
			const FBox2f B(FVector2f(BX - 190.f * S, Y), FVector2f(BX, Y + 44.f * S));
			CraftButton(Canvas, B, Label, B.IsInside(FVector2f(Mouse)), bOk);
			if (Cost.Qty > 0)
			{
				const FString Text = FString::Printf(TEXT("%d / %d"), Cost.Qty, Have);
				UTexture2D* MatIcon = MD ? FNWUILayout::GameTexture(MD->Icon) : nullptr;
				const float CX = B.GetCenter().X - 50.f * S;
				if (MatIcon)
				{
					FCanvasTileItem T(FVector2D(CX, Y + 48.f * S), MatIcon->GetResource(), FVector2D(32.f * S, 32.f * S), FLinearColor::White);
					T.BlendMode = SE_BLEND_Translucent;
					Canvas->DrawItem(T);
				}
				CraftText(Canvas, Text, CX + 38.f * S, Y + 50.f * S, 22.f, Have >= Cost.Qty ? Tan : Red);
				// Odds: attribute = one of the others, equal for all; GS = only the limits.
				TArray<FString> Odds;
				if (Kind == ENWImprove::Attribute)
				{
					Odds.Add(NWText::Format(TEXT("nw.craft.attr_odds"), {FString::FromInt(FMath::RoundToInt(100.f / FMath::Max(1, Rules.Attributes.Num() - 1)))}));
				}
				else if (Kind == ENWImprove::GearScore)
				{
					const FNWCrafting::FGsOdds G = Crafting.GsOdds(Me, *R, I, Mat);
					Odds.Add(FString::Printf(TEXT("GS %d–%d"), G.Range.X, G.Range.Y));
				}
				float OY = Y + 82.f * S;
				for (const FString& Line : Odds)
				{
					// Same scale as CraftText (size = line height), to center below the button.
					float TW = 0.f, TH = 0.f, AW = 0.f, AH = 1.f;
					Canvas->TextSize(FNWUILayout::GameFont(0), *Line, TW, TH, 1.f, 1.f);
					Canvas->TextSize(FNWUILayout::GameFont(0), TEXT("Ag"), AW, AH, 1.f, 1.f);
					CraftText(Canvas, Line, B.GetCenter().X - TW * 0.5f * 20.f * S / FMath::Max(1.f, AH), OY, 20.f, Gray);
					OY += 22.f * S;
				}
			}
			if (bOk)
			{
				RerollButtons.Add({B, I.Slot, uint8(Kind), Mat});
			}
			BX -= 202.f * S;
		}
		Y += 130.f * S;
	}
	if (Shown == 0)
	{
		CraftText(Canvas, NWText::Get(TEXT("nw.craft.reroll_none")), Panel.Min.X + 30.f * S, Y, 24.f, Gray);
	}
}

const FNWItem* FNWRefiningMenu::ResultInBag() const
{
	const FNWItem* R = RollResult();
	const ANWCharacter* Me = Character.Get();
	if (!R || !Me || !Me->Inventory)
	{
		return nullptr;
	}
	// The rolled item is unique (GS, quality, perks and serial): finds its slot in the bag.
	return Me->Inventory->Items.FindByPredicate([R](const FNWItem& I) { return I.Container == ENWItemContainer::Bag && I.SameKind(*R); });
}

void FNWRefiningMenu::DrawDoneButtons(UCanvas* Canvas, const FVector2D& Mouse)
{
	// ActiveButtonsContainer (craftingscreenv4): x 232–602, from y 520, spacing 8 (the UnusedButtonContainer's).
	// Continue is the highlighted button (70 high); Equip and Exit, 45. They appear with the end window (0.7 s, delay 0.55).
	const FNWItem* It = ResultInBag();
	const FNWItemDef* D = It ? FNWItemCatalog::Get().Find(It->ItemId) : nullptr;
	const bool bEquip = D && NWEquip::SlotForSet(D->Set) != INDEX_NONE;
	const bool bTool = bEquip && NWEquip::IsTool(NWEquip::SlotForSet(D->Set));
	const float S = Canvas->ClipY / 1080.f;
	const float X0 = (Canvas->ClipX - 1920.f * S) * 0.5f + 232.f * S, W = 370.f * S;
	float Y = 520.f * S;
	auto Add = [&](EDoneButton Kind, const FString& Label, float H)
	{
		const FBox2f R(FVector2f(X0, Y), FVector2f(X0 + W, Y + H * S));
		CraftButton(Canvas, R, Label, R.IsInside(FVector2f(Mouse)), true);
		DoneButtons.Add({R, Kind});
		Y += (H + 8.f) * S;
	};
	Add(EDoneButton::Continue, Loc(TEXT("cr_continue_crafting")), 70.f);
	if (bEquip)
	{
		Add(EDoneButton::Equip, Loc(bTool ? TEXT("ui_tooltip_setactivetool") : TEXT("inv_equip")), 45.f);
	}
	Add(EDoneButton::Exit, Loc(TEXT("cr_exit_station")), 45.f);
}

void FNWRefiningMenu::Sound(const TCHAR* Event) const
{
	const ANWCharacter* Me = Character.Get();
	UWorld* World = Me ? Me->GetWorld() : nullptr;
	if (!World || FixedNow > 0.0 || !World->IsGameWorld())
	{
		return;   // capture without a game (FixedNow) and outside the game do not play
	}
	static TMap<FString, TArray<TArray<FString>>> Events = []
	{
		TMap<FString, TArray<TArray<FString>>> Out;
		FString Raw;
		TSharedPtr<FJsonObject> Root;
		if (FFileHelper::LoadFileToString(Raw, *(FPaths::ProjectDir() / TEXT("Data/Audio/ui_sounds.json")))
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) && Root.IsValid())
		{
			for (const auto& E : Root->Values)
			{
				TArray<TArray<FString>>& Layers = Out.Add(FString(E.Key));
				for (const TSharedPtr<FJsonValue>& L : E.Value->AsArray())
				{
					TArray<FString>& Names = Layers.AddDefaulted_GetRef();
					for (const TSharedPtr<FJsonValue>& N : L->AsArray())
					{
						Names.Add(N->AsString());
					}
				}
			}
		}
		return Out;
	}();
	const TArray<TArray<FString>>* Layers = Events.Find(Event);
	if (!Layers)
	{
		return;
	}
	for (const TArray<FString>& L : *Layers)
	{
		if (L.Num() == 0)
		{
			continue;
		}
		const FString& N = L[FMath::RandRange(0, L.Num() - 1)];
		if (USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/NW/Audio/UI/%s.%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UGameplayStatics::PlaySound2D(World, S);
		}
	}
}

void FNWRefiningMenu::SoundAt(double T, double At, int32 Bit, const FString& Event)
{
	if (T >= At && !(SoundsDone & (1u << Bit)))
	{
		SoundsDone |= 1u << Bit;
		Sound(*Event);
	}
}

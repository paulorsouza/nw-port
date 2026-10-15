#include "NWMasteryMenu.h"
#include "NWText.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NWCharacter.h"
#include "NWPlayerController.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TextureResource.h"

static const TCHAR* Canvas_ = TEXT("mastery");
static const TCHAR* Keys[3] = {TEXT("Q"), TEXT("R"), TEXT("F")};
static const TCHAR* NodeNames[5] = {TEXT("Node"), TEXT("Node2"), TEXT("Node3"), TEXT("Node4"), TEXT("Node5")};
static const TCHAR* TreeNames[2] = {TEXT("TreeContainer/LeftMasteryTree"), TEXT("TreeContainer/RightMasteryTree")};
// Game UI colors (UIStyle): tan, gold of the descriptions, gray of the notes.
static const FLinearColor Tan(FColor(0xEB, 0xE3, 0xD6));
static const FLinearColor Gold(FColor(0xED, 0xC6, 0x3E));
static const FLinearColor Dim(FColor(0x82, 0x82, 0x82));
static const FString Mastery = TEXT("lyshineui/images/skills/mastery/");

bool FNWMasteryMenu::Load()
{
	if (!Layout.Load(NWText::DataPath(TEXT("Data/UI/mastery_layout.json"))))
	{
		return false;
	}
	Layout.ResolveTexture = [](const FString& P) { return FNWUILayout::GameTexture(P); };
	for (int32 i = 0; i < 3; ++i)
	{
		Layout.Fonts[i] = FNWUILayout::GameFont(i);
	}
	FString Raw;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Raw, *NWText::DataPath(TEXT("Data/UI/abilities.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
	{
		return false;
	}
	for (const auto& Pair : Root->Values)
	{
		const TSharedPtr<FJsonObject> W = Pair.Value->AsObject();
		FWeapon& Weapon = Weapons.AddDefaulted_GetRef();
		Weapon.Key = Pair.Key;
		Weapon.Title = W->GetStringField(TEXT("title"));
		W->TryGetStringField(TEXT("hud_icon"), Weapon.HudIcon);
		for (const TSharedPtr<FJsonValue>& T : W->GetArrayField(TEXT("trees")))
		{
			Weapon.Trees.Add(T->AsString());
		}
		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		if (W->TryGetArrayField(TEXT("nodes"), Nodes))
		{
			for (const TSharedPtr<FJsonValue>& V : *Nodes)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FNode& N = Weapon.Nodes.AddDefaulted_GetRef();
				N.Id = O->GetStringField(TEXT("id"));
				N.Action = FName(O->GetStringField(TEXT("action")));
				N.Kind = O->GetStringField(TEXT("kind"));
				N.Tree = O->GetIntegerField(TEXT("tree"));
				N.Row = O->GetIntegerField(TEXT("row"));
				N.Col = O->GetIntegerField(TEXT("col"));
				N.Icon = O->GetStringField(TEXT("icon"));
				N.Background = O->GetStringField(TEXT("background"));
				N.Name = O->GetStringField(TEXT("name"));
				N.Description = O->GetStringField(TEXT("description"));
				N.Parent = O->GetStringField(TEXT("parent"));
				N.Cooldown = O->GetNumberField(TEXT("cooldown"));
			}
		}
	}
	return Weapons.Num() > 0;
}

void FNWMasteryMenu::Open(ANWCharacter* InCharacter)
{
	if (Weapons.Num() == 0 && !Load())
	{
		UE_LOG(LogTemp, Warning, TEXT("NW: mastery screen without data (Data/UI/mastery_layout.json, abilities.json)"));
		return;
	}
	Character = InCharacter;
	Pending = NAME_None;
	Tab = 0;
	if (InCharacter && InCharacter->WeaponSets.IsValidIndex(InCharacter->ActiveWeapon))
	{
		const FString Active = InCharacter->WeaponSets[InCharacter->ActiveWeapon].Id.ToString().ToLower();
		Tab = FMath::Max(0, Weapons.IndexOfByPredicate([&](const FWeapon& W) { return W.Key == Active; }));
	}
	ApplyWeapon();
}

void FNWMasteryMenu::LoadOwned()
{
	Owned.Reset();
	TArray<FString> Ids;
	GConfig->GetArray(NWIni::Mastery, *Weapons[Tab].Key, Ids, GGameUserSettingsIni);
	Owned.Append(Ids);
}

void FNWMasteryMenu::SaveOwned()
{
	GConfig->SetArray(NWIni::Mastery, *Weapons[Tab].Key, Owned.Array(), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	if (ANWPlayerController* PC = Character.IsValid() ? Character->GetController<ANWPlayerController>() : nullptr)
	{
		PC->ServerSetMastery(Weapons[Tab].Key, Owned.Array());
	}
}

void FNWMasteryMenu::ApplyWeapon()
{
	FWeapon& W = Weapons[Tab];
	LoadOwned();
	for (const TCHAR* Tree : TreeNames)
	{
		for (int32 Row = 1; Row <= 6; ++Row)
		{
			for (const TCHAR* Name : NodeNames)
			{
				if (FNWUIElement* Node = E(FString::Printf(TEXT("%s/Row%d/%s"), Tree, Row, Name)))
				{
					Node->Visible = false;
				}
			}
		}
	}
	// Nodes of each row in the Node, Node2… elements, spread over the datasheet column (5 columns of 108).
	TMap<TPair<int32, int32>, int32> Used;
	TArray<FNode*> Sorted;
	for (FNode& N : W.Nodes)
	{
		Sorted.Add(&N);
	}
	Sorted.Sort([](const FNode& A, const FNode& B) { return A.Tree != B.Tree ? A.Tree < B.Tree : A.Row != B.Row ? A.Row < B.Row : A.Col < B.Col; });
	for (FNode* N : Sorted)
	{
		int32& K = Used.FindOrAdd({N->Tree, N->Row});
		if (K >= 5 || N->Tree > 1 || N->Row > 5)
		{
			continue;
		}
		N->ElementPath = FString::Printf(TEXT("%s/Row%d/%s"), TreeNames[N->Tree], N->Row + 1, NodeNames[K++]);
		N->Root = E(N->ElementPath);
		N->Rune = E(N->ElementPath + TEXT("/Rune"));
		N->Glow = E(N->ElementPath + TEXT("/Glow"));
		N->Bg = E(N->ElementPath + TEXT("/Bg"));
		N->Border = E(N->ElementPath + TEXT("/Bg/Border"));
		N->IconBg = E(N->ElementPath + TEXT("/Bg/IconBg"));
		N->IconImg = E(N->ElementPath + TEXT("/Bg/Icon"));
		N->VLine = E(N->ElementPath + TEXT("/VerticalLine"));
		N->HLine = E(N->ElementPath + TEXT("/VerticalLine/HorizonalLine"));
		N->Ultimate = E(N->ElementPath + TEXT("/UltimateAbilityContainer"));
		N->Lock = E(N->ElementPath + TEXT("/UltimateAbilityContainer/LockImage"));
		N->Ticks = E(N->ElementPath + TEXT("/UltimateAbilityContainer/TickContainer"));
		if (!N->Root)
		{
			continue;
		}
		N->Root->Visible = true;
		// Ultimate moves up a bit so it does not touch the button (script's ultimateArrowOffset).
		N->Root->Translate = FVector2f((N->Col - 2) * 108.f, N->Kind == TEXT("ultimate") ? -40.f : 0.f);
		// Art by type (masterynode.luac): active, passive or "info" (ultimate).
		const FString Type = N->Kind == TEXT("active") ? TEXT("active") : N->Kind == TEXT("ultimate") ? TEXT("info") : TEXT("passive");
		if (N->Rune) N->Rune->TextureOverride = Mastery + TEXT("masterynode") + Type + TEXT("rune.dds");
		if (N->Bg) N->Bg->TextureOverride = Mastery + TEXT("masterynode") + Type + TEXT("bg.dds");
		if (N->Border) N->Border->TextureOverride = Mastery + TEXT("masterynode") + Type + TEXT("border.dds");
		if (N->IconBg)
		{
			N->IconBg->TextureOverride = N->Background;
			N->IconBg->Visible = !N->Background.IsEmpty();
		}
		if (N->IconImg) N->IconImg->TextureOverride = N->Icon;
		if (N->Ultimate) N->Ultimate->Visible = N->Kind == TEXT("ultimate");
		if (N->Ticks) N->Ticks->Visible = false;
		// Arrow to the ability above when it is on the row right above (upgrades).
		const FNode* Parent = W.Nodes.FindByPredicate([&](const FNode& P) { return P.Id == N->Parent; });
		if (N->VLine) N->VLine->Visible = Parent && Parent->Row == N->Row - 1;
		if (N->HLine) N->HLine->Visible = false;
	}
	// Fixed texts the script used to fill in.
	if (FNWUIElement* T = E(TEXT("MasteryTreeWindow/WeaponTitle"))) T->TextOverride = W.Title;
	for (int32 t = 0; t < 2; ++t)
	{
		if (FNWUIElement* T = E(FString::Printf(TEXT("TreeContainer/TreeName%d"), t + 1)))
		{
			T->TextOverride = W.Trees.IsValidIndex(t) ? W.Trees[t] : FString();
		}
	}
	if (FNWUIElement* Icon = E(TEXT("PointsAvailableContainer/WeaponIcon"))) Icon->TextureOverride = W.HudIcon;
	if (FNWUIElement* T = E(TEXT("ProgressBarBg/Level"))) T->TextOverride = NWText::Format(TEXT("nw.mastery.level"), {TEXT("20")});
	if (FNWUIElement* T = E(TEXT("ProgressBarBg/ProgressionPoints"))) T->Visible = false;
	if (FNWUIElement* F = E(TEXT("ProgressBarBg/ProgressFill"))) F->Fill = 1.f;
	if (FNWUIElement* T = E(TEXT("Confirm/buttonText"))) T->TextOverride = NWText::Get(TEXT("nw.mastery.confirm"));
	// Button focus states and animations: the script only turns them on with the mouse over it.
	for (const TCHAR* Part : {TEXT("buttonBgFocus"), TEXT("buttonFocus"), TEXT("buttonFocusHero"), TEXT("frameFocus"),
		TEXT("heroImageSequence"), TEXT("heroImageSequenceDisabled"), TEXT("buttonBottomGlow")})
	{
		if (FNWUIElement* B = E(FString(TEXT("TreeContainer/Confirm/")) + Part)) B->Visible = false;
	}
	if (FNWUIElement* Bg = E(TEXT("TreeContainer/Confirm/buttonBg"))) Bg->Tint = FLinearColor(0.10f, 0.13f, 0.20f);
	if (FNWUIElement* Border = E(TEXT("TreeContainer/Confirm/heroBorder"))) Border->Visible = false;
}

int32 FNWMasteryMenu::SpentInTree(int32 Tree) const
{
	int32 Spent = 0;
	for (const FNode& N : Weapons[Tab].Nodes)
	{
		Spent += (N.Tree == Tree && Owned.Contains(N.Id)) ? 1 : 0;
	}
	return Spent;
}

int32 FNWMasteryMenu::PointsLeft() const
{
	return TotalPoints - Owned.Num();
}

TArray<FName> FNWMasteryMenu::Slots() const
{
	const ANWCharacter* C = Character.Get();
	if (!C)
	{
		return {};
	}
	const int32 Set = C->WeaponSets.IndexOfByPredicate([&](const FNWWeaponSet& S) { return S.Id.ToString().ToLower() == Weapons[Tab].Key; });
	return C->WeaponSets.IsValidIndex(Set) ? C->WeaponSets[Set].Abilities : C->Abilities;
}

void FNWMasteryMenu::Refresh()
{
	FWeapon& W = Weapons[Tab];
	const bool bCanSpend = PointsLeft() > 0;
	for (FNode& N : W.Nodes)
	{
		if (!N.Root)
		{
			continue;
		}
		const bool bActive = N.Kind == TEXT("active");
		const bool bUltimate = N.Kind == TEXT("ultimate");
		const bool bOwned = bActive || Owned.Contains(N.Id);
		const bool bUnlocked = !bUltimate || SpentInTree(N.Tree) >= UltimatePoints;
		const bool bAvailable = bOwned || (bCanSpend && bUnlocked);
		// masterynode.luac states: owned (light), available, unavailable (dark).
		const FLinearColor BgColor = bOwned ? FLinearColor::White : bAvailable ? FLinearColor(0.55f, 0.55f, 0.55f) : FLinearColor(0.3f, 0.3f, 0.3f);
		if (N.Bg) N.Bg->Tint = BgColor;
		if (N.IconImg) N.IconImg->Tint = bOwned ? FLinearColor::White : bAvailable ? FLinearColor(0.75f, 0.75f, 0.75f) : FLinearColor(0.4f, 0.4f, 0.4f);
		if (N.IconBg) N.IconBg->Tint = bOwned ? FLinearColor::White : FLinearColor(0.4f, 0.4f, 0.4f);
		if (N.Rune) N.Rune->AlphaMul = bOwned ? 1.f : 0.35f;
		if (N.Glow)
		{
			const bool bPending = bActive && N.Action == Pending;
			N.Glow->Visible = bPending || (bOwned && !bActive);
			N.Glow->AlphaMul = bPending ? 1.f : 0.45f;
			N.Glow->Tint = Gold;
		}
		if (N.Lock) N.Lock->Visible = bUltimate && !bOwned && !bUnlocked;
	}
	const TArray<FName> Equipped = Slots();
	for (int32 s = 0; s < 3; ++s)
	{
		const FString Slot = FString::Printf(TEXT("LeftCol/AbilitySlot%d"), s + 1);
		const FName Action = Equipped.IsValidIndex(s) ? Equipped[s] : NAME_None;
		const FNode* N = W.Nodes.FindByPredicate([&](const FNode& X) { return !Action.IsNone() && X.Action == Action; });
		if (FNWUIElement* Bg = E(Slot + TEXT("/SlotBg"))) Bg->TextureOverride = N ? N->Background : FString();
		if (FNWUIElement* Icon = E(Slot + TEXT("/SlotIcon"))) Icon->TextureOverride = N ? N->Icon : FString();
		if (FNWUIElement* Key = E(Slot + TEXT("/Hint/HintText"))) Key->TextOverride = Keys[s];
		if (FNWUIElement* Hl = E(Slot + TEXT("/Highlight"))) Hl->Visible = false;
		if (FNWUIElement* Fl = E(Slot + TEXT("/SlotFlashLong"))) Fl->Visible = false;
		if (FNWUIElement* Fs = E(Slot + TEXT("/SlotFlashShort"))) Fs->Visible = !Pending.IsNone();
	}
	if (FNWUIElement* Points = E(TEXT("PointsAvailableContainer/PointsAvailableNumber"))) Points->TextOverride = FString::FromInt(PointsLeft());
	// With points left, the number is highlighted and the weapon drawing goes to the background.
	if (FNWUIElement* Icon = E(TEXT("PointsAvailableContainer/WeaponIcon"))) Icon->AlphaMul = PointsLeft() > 0 ? 0.25f : 1.f;
	if (FNWUIElement* Num = E(TEXT("PointsAvailableContainer/PointsAvailableNumber"))) Num->Visible = PointsLeft() > 0;
	if (FNWUIElement* Lbl = E(TEXT("PointsAvailableContainer/PointsAvailableLabel"))) Lbl->Visible = PointsLeft() > 0;
	// Focused button (game layers) when the mouse is over it.
	if (FNWUIElement* Focus = E(TEXT("TreeContainer/Confirm/frameFocus"))) Focus->Visible = bHoverConfirm;
	if (FNWUIElement* Focus = E(TEXT("TreeContainer/Confirm/buttonBgFocus"))) Focus->Visible = bHoverConfirm;
}

FNWMasteryMenu::FNode* FNWMasteryMenu::NodeAt(const FVector2D& Mouse)
{
	// Only the inner part of the node (background and icon), not the glow around it.
	const int32 Hit = Layout.HitTest(Canvas_, Mouse, [](const FNWUIElement& El)
	{
		return El.Path.Contains(TEXT("/Row")) && (El.Name == TEXT("Bg") || El.Name == TEXT("Icon") || El.Name == TEXT("Border") || El.Name == TEXT("IconBg"));
	});
	const FNWUIElement* El = Layout.Element(Canvas_, Hit);
	if (!El)
	{
		return nullptr;
	}
	return Weapons[Tab].Nodes.FindByPredicate([&](const FNode& N)
	{
		return !N.ElementPath.IsEmpty() && El->Path.Contains(N.ElementPath + TEXT("/"));
	});
}

// Rich text of the descriptions: <hl>..</> gold, <dim>..</> gray; line wrap by width.
struct FRichWord { FString Text; FLinearColor Color; bool bBreak = false; };

static TArray<FRichWord> ParseRich(const FString& Src)
{
	TArray<FRichWord> Out;
	FLinearColor Color = Tan;
	FString Word;
	auto Flush = [&]() { if (!Word.IsEmpty()) { Out.Add({Word, Color}); Word.Reset(); } };
	for (int32 i = 0; i < Src.Len(); ++i)
	{
		if (Src[i] == TEXT('<'))
		{
			const int32 End = Src.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, i);
			if (End != INDEX_NONE)
			{
				const FString Tag = Src.Mid(i + 1, End - i - 1);
				Flush();
				Color = Tag == TEXT("hl") ? Gold : Tag == TEXT("dim") ? Dim : Tan;
				i = End;
				continue;
			}
		}
		if (Src[i] == TEXT('\n'))
		{
			Flush();
			Out.Add({FString(), Color, true});
			continue;
		}
		if (Src[i] == TEXT(' '))
		{
			Word.AppendChar(TEXT(' '));
			Flush();
			continue;
		}
		Word.AppendChar(Src[i]);
	}
	Flush();
	return Out;
}

void FNWMasteryMenu::DrawTooltip(UCanvas* Canvas, const FNode& N, const FVector2D& Mouse)
{
	UFont* Regular = FNWUILayout::GameFont(0);
	UFont* Bold = FNWUILayout::GameFont(2);
	if (!Regular || !Bold)
	{
		return;
	}
	const float S = UIScale;
	const float Width = 440.f * S, Pad = 16.f * S;
	const float BodyScale = 17.f * S / Regular->LegacyFontSize, TitleScale = 24.f * S / Bold->LegacyFontSize;
	FString Sub;
	if (N.Kind == TEXT("active"))
	{
		Sub = NWText::Format(TEXT("nw.mastery.active"), {FString::Printf(TEXT("%.0f"), N.Cooldown)});
	}
	else
	{
		Sub = N.Kind == TEXT("ultimate") ? NWText::Format(TEXT("nw.mastery.ultimate"), {FString::FromInt(UltimatePoints)}) : NWText::Get(TEXT("nw.mastery.passive"));
	}
	// Word layout.
	struct FPlaced { FString Text; FLinearColor Color; float X, Y; };
	TArray<FPlaced> Placed;
	float X = 0.f, Y = 0.f, LineH = 0.f;
	{
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Regular, TEXT("Ag"), W, H, BodyScale, BodyScale);
		LineH = H * 1.15f;
	}
	for (const FRichWord& Word : ParseRich(N.Description))
	{
		if (Word.bBreak)
		{
			X = 0.f;
			Y += LineH;
			continue;
		}
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Regular, Word.Text, W, H, BodyScale, BodyScale);
		if (X > 0.f && X + W > Width - 2.f * Pad)
		{
			X = 0.f;
			Y += LineH;
		}
		Placed.Add({Word.Text, Word.Color, X, Y});
		X += W;
	}
	float TW = 0.f, TH = 0.f;
	Canvas->TextSize(Bold, N.Name, TW, TH, TitleScale, TitleScale);
	const float Header = TH + LineH * 1.3f;
	const float Height = Pad * 2.f + Header + Y + LineH;
	FVector2D Pos = Mouse + FVector2D(28.f * S, 0.f);
	Pos.X = FMath::Min(Pos.X, Canvas->ClipX - Width - 8.f);
	Pos.Y = FMath::Clamp(Pos.Y - Height * 0.3f, 8.f, Canvas->ClipY - Height - 8.f);

	FCanvasTileItem Border(Pos - FVector2D(1.f, 1.f), GWhiteTexture, FVector2D(Width + 2.f, Height + 2.f), FLinearColor(Tan.R, Tan.G, Tan.B, 0.45f));
	Border.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Border);
	FCanvasTileItem Box(Pos, GWhiteTexture, FVector2D(Width, Height), FLinearColor(0.025f, 0.025f, 0.03f, 0.96f));
	Box.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Box);

	FCanvasTextItem Title(Pos + FVector2D(Pad, Pad), FText::FromString(N.Name), Bold, Tan);
	Title.Scale = FVector2D(TitleScale, TitleScale);
	Canvas->DrawItem(Title);
	FCanvasTextItem SubItem(Pos + FVector2D(Pad, Pad + TH), FText::FromString(Sub), Regular, Dim);
	SubItem.Scale = FVector2D(BodyScale, BodyScale);
	Canvas->DrawItem(SubItem);
	for (const FPlaced& P : Placed)
	{
		FCanvasTextItem T(Pos + FVector2D(Pad + P.X, Pad + Header + P.Y), FText::FromString(P.Text), Regular, P.Color);
		T.Scale = FVector2D(BodyScale, BodyScale);
		Canvas->DrawItem(T);
	}
}

void FNWMasteryMenu::Draw(UCanvas* Canvas, const FVector2D& Mouse)
{
	if (!IsOpen())
	{
		return;
	}
	UIScale = Canvas->ClipY / 1080.f;
	{
		const int32 Hit = Layout.HitTest(Canvas_, Mouse, [](const FNWUIElement& El) { return El.Path.Contains(TEXT("TreeContainer/Confirm")); });
		bHoverConfirm = Hit != INDEX_NONE;
	}
	Refresh();
	Layout.Draw(Canvas);

	// Weapon tabs in the header (the game opens one screen per weapon from the mastery list).
	TabRects.Reset();
	UFont* Bold = FNWUILayout::GameFont(2);
	const FBox2f Header = Layout.RectOf(E(TEXT("ScreenHolder/FrameHeader")));
	if (Bold && Header.bIsValid)
	{
		const float S = 20.f * UIScale / Bold->LegacyFontSize;
		float X = Header.Max.X - 40.f * UIScale;
		for (int32 t = Weapons.Num() - 1; t >= 0; --t)
		{
			const FString Label = FString::Printf(TEXT("%d  %s"), t + 1, *Weapons[t].Title);
			float W = 0.f, H = 0.f;
			Canvas->TextSize(Bold, Label, W, H, S, S);
			X -= W;
			const FVector2D P(X, Header.GetCenter().Y - H * 0.5f);
			FCanvasTextItem T(P, FText::FromString(Label), Bold, t == Tab ? Gold : Dim);
			T.Scale = FVector2D(S, S);
			T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f));
			Canvas->DrawItem(T);
			TabRects.Insert(FBox2f(FVector2f(P), FVector2f(P.X + W, P.Y + H)), 0);
			X -= 36.f * UIScale;
		}
	}

	if (const FNode* N = NodeAt(Mouse))
	{
		DrawTooltip(Canvas, *N, Mouse);
	}
}

bool FNWMasteryMenu::Click(const FVector2D& Mouse, bool& bCloseRequested)
{
	bCloseRequested = false;
	if (!IsOpen())
	{
		return false;
	}
	for (int32 t = 0; t < TabRects.Num(); ++t)
	{
		if (TabRects[t].IsInside(FVector2f(Mouse)) && t != Tab)
		{
			Tab = t;
			Pending = NAME_None;
			ApplyWeapon();
			return true;
		}
	}
	const int32 Hit = Layout.HitTest(Canvas_, Mouse, [](const FNWUIElement&) { return true; });
	const FNWUIElement* El = Layout.Element(Canvas_, Hit);
	if (!El)
	{
		return true;
	}
	if (El->Path.Contains(TEXT("ButtonClose")) || El->Path.Contains(TEXT("TreeContainer/Confirm")))
	{
		bCloseRequested = true;
		return true;
	}
	for (int32 s = 0; s < 3; ++s)
	{
		if (El->Path.Contains(FString::Printf(TEXT("LeftCol/AbilitySlot%d"), s + 1)))
		{
			ANWCharacter* C = Character.Get();
			TArray<FName> New = Slots();
			New.SetNum(3);
			if (!Pending.IsNone() && C)
			{
				// Already equipped in another slot: swaps the two.
				const int32 Existing = New.IndexOfByKey(Pending);
				if (Existing != INDEX_NONE)
				{
					New[Existing] = New[s];
				}
				New[s] = Pending;
				const int32 Set = C->WeaponSets.IndexOfByPredicate([&](const FNWWeaponSet& S) { return S.Id.ToString().ToLower() == Weapons[Tab].Key; });
				C->SetWeaponAbilities(Set, New);
				Pending = NAME_None;
			}
			else
			{
				Pending = New[s];
			}
			return true;
		}
	}
	if (FNode* N = NodeAt(Mouse))
	{
		if (N->Kind == TEXT("active"))
		{
			Pending = Pending == N->Action ? NAME_None : N->Action;
		}
		else if (Owned.Contains(N->Id))
		{
			Owned.Remove(N->Id);
			SaveOwned();
		}
		else if (PointsLeft() > 0 && (N->Kind != TEXT("ultimate") || SpentInTree(N->Tree) >= UltimatePoints))
		{
			Owned.Add(N->Id);
			SaveOwned();
		}
	}
	return true;
}

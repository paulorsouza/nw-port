#include "NWLoginMenu.h"
#include "NWText.h"

#include "CanvasItem.h"
#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NWBackend.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWeakWidget.h"

static const FLinearColor LoginTan(FColor(0xEB, 0xE3, 0xD6));
static const FLinearColor LoginDim(FColor(0x9A, 0x96, 0x8E));
static const FLinearColor LoginErrorRed(FColor(0xE8, 0x5A, 0x4A));
static const float FieldWidth = 497.f;
static const float FieldStep = 78.f;

// ---------------------------------------------------------------- text fields (Slate over the Canvas)

class SNWLoginFields : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNWLoginFields) {}
		SLATE_ARGUMENT(FNWLoginMenu*, Menu)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args)
	{
		Menu = Args._Menu;
		ChildSlot[SAssignNew(Panel, SCanvas)];
	}

	void Rebuild()
	{
		Panel->ClearChildren();
		Edits.Reset();
		FSlateFontInfo Font(FNWUILayout::GameFont(0), 18);
		for (int32 i = 0; i < Menu->NumFields(); ++i)
		{
			TSharedRef<SEditableText> Edit = SNew(SEditableText)
				.Text(FText::FromString(Menu->FieldValue(i)))
				.IsPassword(Menu->IsPassword(i))
				.Font(this, &SNWLoginFields::FontFor)
				.ColorAndOpacity(FSlateColor(LoginTan))
				.SelectAllTextWhenFocused(false)
				.OnTextChanged_Lambda([this, i](const FText& T) { Menu->SetFieldValue(i, T.ToString()); })
				.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type How)
				{
					if (How == ETextCommit::OnEnter)
					{
						Menu->Submit();
					}
				});
			Edits.Add(Edit);
			Panel->AddSlot()
				.Position(TAttribute<FVector2D>::CreateLambda([this, i]() { return FVector2D(Menu->FieldRect(i).Min) / Scale(); }))
				.Size(TAttribute<FVector2D>::CreateLambda([this, i]() { return FVector2D(Menu->FieldRect(i).GetSize()) / Scale(); }))
				[
					// In SCanvas the alignment picks the child's point that sits at the position; centering goes in the SBox.
					SNew(SBox).VAlign(VAlign_Center)[Edit]
				];
		}
		// With the account already filled in, the cursor goes to the password.
		FocusField(Menu->NumFields() > 1 && !Menu->FieldValue(0).IsEmpty() ? 1 : 0);
	}

	void FocusField(int32 Index)
	{
		if (Edits.IsValidIndex(Index))
		{
			FSlateApplication::Get().SetKeyboardFocus(Edits[Index], EFocusCause::SetDirectly);
		}
	}

	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Key) override
	{
		// Tab / Shift+Tab between the fields.
		if (Key.GetKey() == EKeys::Tab && Edits.Num() > 0)
		{
			int32 Current = Edits.IndexOfByPredicate([](const TSharedPtr<SEditableText>& E) { return E->HasKeyboardFocus(); });
			Current = (Current + (Key.IsShiftDown() ? Edits.Num() - 1 : 1)) % Edits.Num();
			FocusField(Current);
			return FReply::Handled();
		}
		return SCompoundWidget::OnKeyDown(Geometry, Key);
	}

private:
	float Scale() const
	{
		const float S = GetCachedGeometry().Scale;
		return S > 0.f ? S : 1.f;
	}
	FSlateFontInfo FontFor() const
	{
		// Same size as the game's field text; Slate measures fonts in points (96/72 px).
		return FSlateFontInfo(FNWUILayout::GameFont(0), FMath::Max(8, FMath::RoundToInt(Menu->FontPixels() / Scale() * 1.05f)));
	}

	FNWLoginMenu* Menu = nullptr;
	TSharedPtr<SCanvas> Panel;
	TArray<TSharedPtr<SEditableText>> Edits;
};

// ---------------------------------------------------------------- screen

bool FNWLoginMenu::Load()
{
	const FString File = NWText::DataPath(TEXT("Data/UI/login_layout.json"));
	if (!Layout.Load(File))
	{
		return false;
	}
	Layout.ResolveTexture = [](const FString& P) { return FNWUILayout::GameTexture(P); };
	for (int32 i = 0; i < 3; ++i)
	{
		Layout.Fonts[i] = FNWUILayout::GameFont(i);
	}
	return true;
}

FString FNWLoginMenu::Str(const TCHAR* Key) const
{
	return NWText::Get(Key);
}

FNWUIElement* FNWLoginMenu::Slot(int32 Index, const FString& Suffix)
{
	FString Path = FString::Printf(TEXT("CharacterCreateButton%d"), Index + 1);
	if (!Suffix.IsEmpty())
	{
		Path += TEXT("/") + Suffix;
	}
	return Layout.FindOne(TEXT("landing"), Path);
}

bool FNWLoginMenu::Hit(FNWUIElement* Element, const FVector2D& Mouse) const
{
	return Element && Layout.WasShown(Element) && Layout.RectOf(Element).IsInside(FVector2f(Mouse));
}

void FNWLoginMenu::Activate(APlayerController* PC, UNWBackend* InBackend)
{
	if (!bLoaded)
	{
		bLoaded = Load();
		if (!bLoaded)
		{
			UE_LOG(LogTemp, Warning, TEXT("NW: login screen without data (Data/UI/login_layout.json)"));
			return;
		}
	}
	Owner = PC;
	Backend = InBackend;
	bActive = true;
	if (GEngine && GEngine->GameViewport)
	{
		Overlay = SNew(SNWLoginFields).Menu(this);
		OverlayHost = SNew(SWeakWidget).PossiblyNullContent(Overlay);
		GEngine->GameViewport->AddViewportWidgetContent(OverlayHost.ToSharedRef(), 40);
	}
	if (PC)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
		PC->SetIgnoreLookInput(true);
		PC->SetIgnoreMoveInput(true);
	}
	Message = NWText::Resolve(InBackend->TakeConnectionError());
	if (InBackend->HasSession())
	{
		Show(EScreen::Characters);
		TWeakPtr<int32> Alive = AliveToken;
		InBackend->RefreshCharacters([this, Alive](bool bOk, const FString& Error)
		{
			if (Alive.IsValid())
			{
				Done(bOk, Error);
			}
		});
	}
	else
	{
		Show(EScreen::Login);
	}
}

void FNWLoginMenu::Deactivate()
{
	if (OverlayHost.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(OverlayHost.ToSharedRef());
	}
	Overlay.Reset();
	OverlayHost.Reset();
	bActive = false;
}

void FNWLoginMenu::Show(EScreen InScreen)
{
	Screen = InScreen;
	Fields.Reset();
	auto Add = [this](const FString& Placeholder, bool bPassword = false)
	{
		FField& F = Fields.AddDefaulted_GetRef();
		F.Placeholder = Placeholder;
		F.bPassword = bPassword;
	};
	switch (Screen)
	{
	case EScreen::Login:
		Add(Str(TEXT("nw.login.account")));
		Add(Str(TEXT("nw.login.password")), true);
		break;
	case EScreen::Register:
		Add(Str(TEXT("nw.login.invite")));
		Add(Str(TEXT("nw.login.account")));
		Add(Str(TEXT("nw.login.password")), true);
		break;
	case EScreen::NewCharacter:
		Add(Str(TEXT("ui_charactername")));
		break;
	case EScreen::ConfirmDelete:
		Add(Str(TEXT("ui_confirm_delete_character_placeholder")));
		break;
	case EScreen::Characters:
		break;
	}
	if (Screen == EScreen::Login && Backend.IsValid() && !Backend->GetAccountName().IsEmpty())
	{
		Fields[0].Value = Backend->GetAccountName();
	}
	if (Overlay.IsValid())
	{
		Overlay->Rebuild();
	}
}

void FNWLoginMenu::SetFieldValue(int32 Index, const FString& Value)
{
	if (Fields.IsValidIndex(Index))
	{
		Fields[Index].Value = Value;
	}
}

void FNWLoginMenu::Done(bool bOk, const FString& Error)
{
	UNWBackend* B = Backend.Get();
	if (!B)
	{
		return;
	}
	if (!B->HasSession())
	{
		if (Screen != EScreen::Login && Screen != EScreen::Register)
		{
			Show(EScreen::Login);
		}
		Message = Error;
		return;
	}
	if (!bOk)
	{
		Message = Error;
		return;
	}
	if (HasPopup())
	{
		Show(EScreen::Characters);
	}
	Message.Reset();
	const int32 Num = B->GetCharacters().Num();
	if (!FMath::IsWithin(Selected, 0, Num))
	{
		Selected = Num > 0 ? 0 : -1;
	}
}

void FNWLoginMenu::Submit()
{
	UNWBackend* B = Backend.Get();
	if (!B || B->IsBusy() || bConnecting)
	{
		return;
	}
	TWeakPtr<int32> Alive = AliveToken;
	auto Then = [this, Alive](bool bOk, const FString& Error)
	{
		if (Alive.IsValid())
		{
			Done(bOk, Error);
		}
	};
	Message.Reset();
	switch (Screen)
	{
	case EScreen::Login:
		B->Login(Fields[0].Value, Fields[1].Value, Then);
		break;
	case EScreen::Register:
		B->Register(Fields[0].Value, Fields[1].Value, Fields[2].Value, Then);
		break;
	case EScreen::NewCharacter:
		B->CreateCharacter(Fields[0].Value, [this, Alive, Then](bool bOk, const FString& Error)
		{
			if (Alive.IsValid() && bOk)
			{
				Selected = Backend.IsValid() ? Backend->GetCharacters().Num() - 1 : -1;
			}
			Then(bOk, Error);
		});
		break;
	case EScreen::ConfirmDelete:
	{
		const TArray<FNWCharacterSummary>& Chars = B->GetCharacters();
		if (!Chars.IsValidIndex(Selected))
		{
			Show(EScreen::Characters);
			break;
		}
		if (!Fields[0].Value.TrimStartAndEnd().Equals(Chars[Selected].Name, ESearchCase::IgnoreCase))
		{
			Message = Str(TEXT("nw.login.name_mismatch"));
			break;
		}
		B->DeleteCharacter(Chars[Selected].Id, Then);
		Selected = 0;
		break;
	}
	case EScreen::Characters:
		if (B->GetCharacters().IsValidIndex(Selected))
		{
			bConnecting = true;
			B->Play(B->GetCharacters()[Selected].Id, [this, Alive](bool bOk, const FString& Error)
			{
				if (Alive.IsValid() && !bOk)
				{
					bConnecting = false;
					Message = Error;
				}
			});
		}
		break;
	}
}

void FNWLoginMenu::Cancel()
{
	switch (Screen)
	{
	case EScreen::Login:
		Show(EScreen::Register);
		break;
	case EScreen::Register:
		Show(EScreen::Login);
		break;
	default:
		Show(EScreen::Characters);
		break;
	}
	Message.Reset();
}

void FNWLoginMenu::SetButton(const FString& Button, const FString& Text, bool bHover, bool bEnabled)
{
	if (FNWUIElement* Root = E(Button))
	{
		Root->AlphaMul = bEnabled ? 1.f : 0.45f;
	}
	if (FNWUIElement* T = E(Button + TEXT("/buttonText")))
	{
		T->TextOverride = Text;
		T->FontSize = Button.EndsWith(TEXT("PlayButton")) ? 30.f : 24.f;
	}
	// As in Weapon Mastery: dark background and no "hero" border; the glow comes from the focus.
	if (FNWUIElement* Bg = E(Button + TEXT("/buttonBg")))
	{
		Bg->Tint = FLinearColor(0.10f, 0.13f, 0.20f);
	}
	if (FNWUIElement* Border = E(Button + TEXT("/heroBorder")))
	{
		Border->Visible = false;
	}
	for (const TCHAR* Layer : {TEXT("/buttonBgFocus"), TEXT("/buttonFocus"), TEXT("/frameFocus"), TEXT("/buttonBottomGlow")})
	{
		if (FNWUIElement* L = E(Button + Layer))
		{
			L->Visible = bHover && bEnabled;
		}
	}
}

static FString WeaponName(const FString& Id)
{
	return NWText::Get(Id.Equals(TEXT("Hatchet"), ESearchCase::IgnoreCase) ? TEXT("nw.weapon.hatchet") : TEXT("nw.weapon.sword"));
}

void FNWLoginMenu::Apply(const FVector2D& Mouse)
{
	UNWBackend* B = Backend.Get();
	const bool bSession = B && B->HasSession();
	const bool bBusy = !B || B->IsBusy() || bConnecting;
	const TArray<FNWCharacterSummary> Empty;
	const TArray<FNWCharacterSummary>& Chars = B ? B->GetCharacters() : Empty;

	if (FNWUIElement* Panel = E(TEXT("CharacterSelection/CharacterInfoHolder")))
	{
		Panel->Visible = bSession;
	}
	if (FNWUIElement* Header = E(TEXT("CharcterFrameHeader/FrameHeaderText")))
	{
		Header->Visible = true;
		Header->TextOverride = Str(TEXT("ui_characters"));
	}
	Hover = -1;
	for (int32 i = 0; i < 3; ++i)
	{
		const bool bHas = Chars.IsValidIndex(i);
		if (!HasPopup() && Hit(Slot(i, FString()), Mouse))
		{
			Hover = i;
		}
		if (FNWUIElement* Info = Slot(i, TEXT("CharacterInfoHolder")))
		{
			Info->Visible = bHas;
		}
		if (FNWUIElement* EmptyHolder = Slot(i, TEXT("CharacterEmptyHolder")))
		{
			EmptyHolder->Visible = !bHas;
		}
		if (FNWUIElement* T = Slot(i, TEXT("CharacterEmptyHolder/CharacterEmptyText")))
		{
			T->TextOverride = Str(TEXT("ui_createcharacter"));
		}
		if (bHas)
		{
			const FNWCharacterSummary& C = Chars[i];
			FString Line = WeaponName(C.ActiveWeapon);
			if (C.bHasLastSeen)
			{
				Line += C.LastSeen.ToString(TEXT("  ·  %d/%m"));
			}
			auto SetText = [&](const TCHAR* Suffix, const FString& Text)
			{
				if (FNWUIElement* T = Slot(i, Suffix))
				{
					T->TextOverride = Text;
				}
			};
			SetText(TEXT("CharacterInfoHolder/CharacterName"), C.Name);
			SetText(TEXT("CharacterInfoHolder/CharacterWorld"), Line);
			SetText(TEXT("CharacterInfoHolder/CharacterLevelHolder/CharacterLevel"), TEXT("20"));
		}
		if (FNWUIElement* Delete = Slot(i, TEXT("CharacterInfoHolder/DeleteButton")))
		{
			Delete->Visible = bHas && i == Selected && !HasPopup();
			const FString Prefix = FString::Printf(TEXT("CharacterCreateButton%d/CharacterInfoHolder/DeleteButton"), i + 1);
			SetButton(Prefix, Str(TEXT("nw.login.delete")), Hit(Delete, Mouse), !bBusy);
		}
		// Glows that the Lua animates (fade 0 in the file): on for the selected one and on mouse hover.
		const bool bSel = bHas && i == Selected;
		const bool bHot = Hover == i;
		auto SetFade = [&](const TCHAR* Suffix, float Fade)
		{
			if (FNWUIElement* G = Slot(i, Suffix))
			{
				G->Fade = Fade;
			}
		};
		SetFade(TEXT("ListItemBg/itemBg"), bSel ? 0.18f : bHot ? 0.10f : 0.05f);
		SetFade(TEXT("ListItemBg/FocusAdd"), bSel ? 0.55f : bHot ? 0.30f : 0.f);
		SetFade(TEXT("ListItemBg/focusGlow"), bSel ? 1.f : 0.f);
		SetFade(TEXT("ListItemBg/SelectedSequenceMask/LastFrame"), bSel ? 1.f : 0.f);
		auto SetSize = [&](const TCHAR* Suffix, float Size)
		{
			if (FNWUIElement* T = Slot(i, Suffix))
			{
				T->FontSize = Size;
			}
		};
		SetSize(TEXT("CharacterInfoHolder/CharacterName"), 26.f);
		SetSize(TEXT("CharacterInfoHolder/CharacterWorld"), 19.f);
		SetSize(TEXT("CharacterInfoHolder/CharacterLevelHolder/CharacterLevel"), 28.f);
		SetSize(TEXT("CharacterEmptyHolder/CharacterEmptyText"), 22.f);
	}

	if (FNWUIElement* Play = E(TEXT("CharacterSelection/PlayButton")))
	{
		Play->Visible = bSession;
		SetButton(TEXT("CharacterSelection/PlayButton"), bConnecting ? TEXT("...") : Str(TEXT("ui_play")),
			!HasPopup() && Hit(Play, Mouse), !bBusy && Chars.IsValidIndex(Selected) && !HasPopup());
	}

	// Window (the game's popup).
	if (FNWUIElement* Popup = E(TEXT("TwitchInfoPopup")))
	{
		Popup->Visible = HasPopup();
	}
	FString Title, Accept, Decline;
	switch (Screen)
	{
	case EScreen::Login: Title = Str(TEXT("ui_enter")); Accept = Str(TEXT("ui_enter")); Decline = Str(TEXT("nw.login.create_account")); break;
	case EScreen::Register: Title = Accept = Str(TEXT("nw.login.create_account")); Decline = Str(TEXT("ui_back")); break;
	case EScreen::NewCharacter: Title = Str(TEXT("ui_createcharacter")); Accept = Str(TEXT("ui_confirm")); Decline = Str(TEXT("ui_cancel")); break;
	case EScreen::ConfirmDelete: Title = Str(TEXT("mm_deletechartitle")); Accept = Str(TEXT("ui_confirm")); Decline = Str(TEXT("ui_cancel")); break;
	default: break;
	}
	if (FNWUIElement* Header = E(TEXT("PopupContainer/FrameHeader/FrameHeaderText")))
	{
		Header->Visible = true;
		Header->TextOverride = Title;
	}
	SetButton(TEXT("PopupContainer/AcceptButton"), Accept, Hit(E(TEXT("PopupContainer/AcceptButton")), Mouse), !bBusy);
	SetButton(TEXT("PopupContainer/DeclineButton"), Decline, Hit(E(TEXT("PopupContainer/DeclineButton")), Mouse), !bBusy);
}

void FNWLoginMenu::DrawLabel(UCanvas* Canvas, const FString& Text, const FBox2f& Rect, const FLinearColor& Color, float Size, bool bCenter) const
{
	UFont* Font = FNWUILayout::GameFont(0);
	if (!Font || Text.IsEmpty())
	{
		return;
	}
	const float Scale = Size * UIScale / Font->LegacyFontSize;
	// Simple word wrap at the rectangle width.
	TArray<FString> Words;
	Text.ParseIntoArrayWS(Words);
	TArray<FString> Lines;
	FString Line;
	for (const FString& W : Words)
	{
		const FString Try = Line.IsEmpty() ? W : Line + TEXT(" ") + W;
		float TW = 0.f, TH = 0.f;
		Canvas->TextSize(Font, Try, TW, TH, Scale, Scale);
		if (TW > Rect.GetSize().X && !Line.IsEmpty())
		{
			Lines.Add(Line);
			Line = W;
		}
		else
		{
			Line = Try;
		}
	}
	Lines.Add(Line);
	float Y = Rect.Min.Y;
	for (const FString& L : Lines)
	{
		float TW = 0.f, TH = 0.f;
		Canvas->TextSize(Font, L, TW, TH, Scale, Scale);
		FCanvasTextItem Item(FVector2D(bCenter ? Rect.GetCenter().X - TW * 0.5f : Rect.Min.X, Y), FText::FromString(L), Font, Color);
		Item.Scale = FVector2D(Scale, Scale);
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		Canvas->DrawItem(Item);
		Y += TH * 1.15f;
	}
}

void FNWLoginMenu::DrawFields(UCanvas* Canvas)
{
	FNWUIElement* Popup = E(TEXT("TwitchInfoPopup/PopupContainer"));
	FNWUIElement* Root = Layout.FindOne(TEXT("input"), TEXT("NameInputField"));
	FNWUIElement* Text = Layout.FindOne(TEXT("input"), TEXT("NameInputField/InputText"));
	FNWUIElement* Placeholder = Layout.FindOne(TEXT("input"), TEXT("NameInputField/PlaceholderText"));
	FNWUIElement* Error = Layout.FindOne(TEXT("input"), TEXT("NameInputField/ErrorText"));
	if (!HasPopup() || !Popup || !Root || !Text || !Placeholder)
	{
		return;
	}
	const FBox2f P = Layout.RectOf(Popup);
	const FVector2f TopLeft = P.Min / UIScale;
	const float CenterX = P.GetCenter().X / UIScale;
	float Top = TopLeft.Y + 110.f;
	if (Screen == EScreen::ConfirmDelete)
	{
		const TArray<FNWCharacterSummary>& Chars = Backend->GetCharacters();
		const FString Name = Chars.IsValidIndex(Selected) ? Chars[Selected].Name : FString();
		const FBox2f Box(FVector2f(CenterX - 280.f, Top) * UIScale, FVector2f(CenterX + 280.f, Top + 80.f) * UIScale);
		DrawLabel(Canvas, Str(TEXT("mm_deletecharwarning")).Replace(TEXT("{characterName}"), *Name), Box, LoginTan, 24.f, true);
		Top += 90.f;
	}
	if (Error)
	{
		Error->Visible = false;
	}
	for (int32 i = 0; i < Fields.Num(); ++i)
	{
		Root->Translate = FVector2f(CenterX - FieldWidth * 0.5f, Top + i * FieldStep);
		Text->TextOverride = FString();
		Placeholder->Visible = Fields[i].Value.IsEmpty();
		Placeholder->TextOverride = Fields[i].Placeholder;
		Layout.DrawOne(Canvas, TEXT("input"));
		Fields[i].Rect = Layout.RectOf(Text);
	}
	const float MsgTop = Top + Fields.Num() * FieldStep;
	DrawLabel(Canvas, Message, FBox2f(FVector2f(CenterX - 280.f, MsgTop) * UIScale, FVector2f(CenterX + 280.f, MsgTop + 60.f) * UIScale), LoginErrorRed, 22.f, true);
}

void FNWLoginMenu::Draw(UCanvas* Canvas, const FVector2D& Mouse)
{
	UNWBackend* B = Backend.Get();
	if (!bActive || !B)
	{
		return;
	}
	UIScale = Canvas->ClipY / 1080.f;
	const FString ConnectionError = NWText::Resolve(B->TakeConnectionError());
	if (!ConnectionError.IsEmpty())
	{
		Message = ConnectionError;
		bConnecting = false;
	}
	if (!B->HasSession() && Screen != EScreen::Login && Screen != EScreen::Register)
	{
		Show(EScreen::Login);
	}
	RunTestSteps();
	Apply(Mouse);
	Layout.DrawOne(Canvas, TEXT("landing"));
	DrawFields(Canvas);
	DrawLanguagePicker(Canvas, Mouse);

	if (!HasPopup())
	{
		// Message above Play, and account/sign out below the panel.
		if (FNWUIElement* Play = E(TEXT("CharacterSelection/PlayButton")))
		{
			const FBox2f R = Layout.RectOf(Play);
			DrawLabel(Canvas, Message, FBox2f(FVector2f(R.Min.X - 200.f * UIScale, R.Min.Y - 70.f * UIScale), FVector2f(R.Max.X, R.Min.Y - 8.f * UIScale)), LoginErrorRed, 22.f, false);
		}
		{
			// Account and sign out in the top right corner.
			UFont* Font = FNWUILayout::GameFont(0);
			const float Scale = 20.f * UIScale / Font->LegacyFontSize;
			const FString Account = B->GetAccountName() + TEXT("   ·   ");
			const FString Exit = Str(TEXT("nw.login.logout"));
			float AW = 0.f, EW = 0.f, H = 0.f;
			Canvas->TextSize(Font, Account, AW, H, Scale, Scale);
			Canvas->TextSize(Font, Exit, EW, H, Scale, Scale);
			const FVector2f At(Canvas->ClipX - AW - EW - 48.f * UIScale, 40.f * UIScale);
			LogoutRect = FBox2f(FVector2f(At.X + AW, At.Y), FVector2f(At.X + AW + EW, At.Y + H));
			DrawLabel(Canvas, Account, FBox2f(At, At + FVector2f(AW + 4.f, H)), LoginDim, 20.f, false);
			DrawLabel(Canvas, Exit, LogoutRect, LogoutRect.IsInside(FVector2f(Mouse)) ? LoginTan : LoginDim, 20.f, false);
		}
	}
}

void FNWLoginMenu::ReloadLanguage()
{
	if (!bLoaded)
	{
		return;
	}
	bLoaded = Load();
	if (bActive)
	{
		Show(Screen);
	}
}

void FNWLoginMenu::DrawLanguagePicker(UCanvas* Canvas, const FVector2D& Mouse)
{
	// "PT  EN" in the top right corner; the current language highlighted.
	UFont* Font = FNWUILayout::GameFont(1);
	if (!Font)
	{
		return;
	}
	const float Scale = 20.f * UIScale / Font->LegacyFontSize;
	const TCHAR* Labels[2] = {TEXT("PT"), TEXT("EN")};
	const bool bPt = NWText::Lang() == TEXT("pt-br");
	float X = Canvas->ClipX - 120.f * UIScale;
	const float Y = 90.f * UIScale;
	for (int32 i = 0; i < 2; ++i)
	{
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Font, Labels[i], W, H, Scale, Scale);
		LangRects[i] = FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + H));
		const bool bCurrent = (i == 0) == bPt;
		const bool bHover = LangRects[i].IsInside(FVector2f(Mouse));
		FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Labels[i]), Font, bCurrent ? LoginTan : bHover ? FLinearColor(0.8f, 0.78f, 0.72f) : LoginDim);
		Item.Scale = FVector2D(Scale, Scale);
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		Canvas->DrawItem(Item);
		X += W + 18.f * UIScale;
	}
}

void FNWLoginMenu::Click(const FVector2D& Mouse)
{
	UNWBackend* B = Backend.Get();
	if (!bActive || !B || B->IsBusy() || bConnecting)
	{
		return;
	}
	for (int32 i = 0; i < 2; ++i)
	{
		if (LangRects[i].bIsValid && LangRects[i].IsInside(FVector2f(Mouse)))
		{
			NWText::SetLang(i == 0 ? TEXT("pt") : TEXT("en"));
			ReloadLanguage();
			return;
		}
	}
	if (HasPopup())
	{
		if (Hit(E(TEXT("PopupContainer/AcceptButton")), Mouse))
		{
			Submit();
		}
		else if (Hit(E(TEXT("PopupContainer/DeclineButton")), Mouse))
		{
			Cancel();
		}
		return;
	}
	const int32 Num = B->GetCharacters().Num();
	for (int32 i = 0; i < 3; ++i)
	{
		if (i == Selected && i < Num && Hit(Slot(i, TEXT("CharacterInfoHolder/DeleteButton")), Mouse))
		{
			Show(EScreen::ConfirmDelete);
			return;
		}
	}
	for (int32 i = 0; i < 3; ++i)
	{
		if (Hit(Slot(i, FString()), Mouse))
		{
			if (i < Num)
			{
				Selected = i;
			}
			else
			{
				Show(EScreen::NewCharacter);
			}
			Message.Reset();
			return;
		}
	}
	if (Hit(E(TEXT("CharacterSelection/PlayButton")), Mouse))
	{
		Submit();
		return;
	}
	if (LogoutRect.IsInside(FVector2f(Mouse)))
	{
		B->Logout();
		Show(EScreen::Login);
	}
}

void FNWLoginMenu::RunTestSteps()
{
	if (!bTestParsed)
	{
		bTestParsed = true;
		FString Raw;
		if (FParse::Value(FCommandLine::Get(), TEXT("-nwuisteps="), Raw, false))
		{
			Raw.TrimQuotesInline();
			Raw.ParseIntoArray(TestSteps, TEXT("|"));
			NextTestStep = FPlatformTime::Seconds() + 3.0;
		}
	}
	UNWBackend* B = Backend.Get();
	if (TestSteps.Num() == 0 || FPlatformTime::Seconds() < NextTestStep || !B || B->IsBusy())
	{
		return;
	}
	NextTestStep = FPlatformTime::Seconds() + 1.5;
	const FString Step = TestSteps[0];
	TestSteps.RemoveAt(0);
	TArray<FString> Parts;
	Step.ParseIntoArray(Parts, TEXT(":"));
	UE_LOG(LogTemp, Display, TEXT("NW uisteps: %s"), *Step);
	if (Parts.Num() == 3 && Parts[0] == TEXT("fill"))
	{
		FString Value = Parts[2];
		if (Value.StartsWith(TEXT("$")))
		{
			Value = FPlatformMisc::GetEnvironmentVariable(*Value.Mid(1));
		}
		SetFieldValue(FCString::Atoi(*Parts[1]), Value);
		if (Overlay.IsValid())
		{
			Overlay->Rebuild();
		}
		return;
	}
	if (Parts.Num() == 2 && Parts[0] == TEXT("click"))
	{
		const FString& What = Parts[1];
		FNWUIElement* Target = nullptr;
		if (What == TEXT("accept")) Target = E(TEXT("PopupContainer/AcceptButton"));
		else if (What == TEXT("decline")) Target = E(TEXT("PopupContainer/DeclineButton"));
		else if (What == TEXT("play")) Target = E(TEXT("CharacterSelection/PlayButton"));
		else if (What == TEXT("delete") && Selected >= 0) Target = Slot(Selected, TEXT("CharacterInfoHolder/DeleteButton"));
		else if (What.StartsWith(TEXT("slot"))) Target = Slot(FCString::Atoi(*What.Mid(4)) - 1, FString());
		if (What == TEXT("logout"))
		{
			Click(FVector2D(LogoutRect.GetCenter()));
		}
		else if (Target)
		{
			Click(FVector2D(Layout.RectOf(Target).GetCenter()));
		}
	}
}

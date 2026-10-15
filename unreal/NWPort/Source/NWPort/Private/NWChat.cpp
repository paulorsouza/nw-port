#include "NWChat.h"
#include "NWText.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "NWUILayout.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SWeakWidget.h"

static const FLinearColor ChatBackground(0.043f, 0.043f, 0.043f, 0.8f);
static const FLinearColor ChatFrameGray(0.388f, 0.388f, 0.388f, 1.f);
static const FLinearColor ChatTextColor(0.92f, 0.89f, 0.84f);

static FLinearColor ChannelColor(ENWChatChannel C)
{
	switch (C)
	{
	case ENWChatChannel::Group: return FLinearColor(0.38f, 0.78f, 0.95f);
	case ENWChatChannel::System: return FLinearColor(0.93f, 0.78f, 0.25f);
	default: return FLinearColor(0.85f, 0.85f, 0.85f);
	}
}

static FString ChannelName(ENWChatChannel C)
{
	switch (C)
	{
	case ENWChatChannel::Group: return NWText::Get(TEXT("ui_chat_name_group"));
	case ENWChatChannel::System: return NWText::Get(TEXT("ui_error_systemname"));
	default: return NWText::Get(TEXT("ui_chat_name_local"));
	}
}

// Text field over the Canvas; position in screen pixels, divided by the geometry scale (DPI).
class SNWChatInput : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNWChatInput) {}
		SLATE_ARGUMENT(FNWChat*, Chat)
	SLATE_END_ARGS()

	TSharedPtr<SEditableText> Edit;

	void Construct(const FArguments& Args)
	{
		FNWChat* Chat = Args._Chat;
		ChildSlot
		[
			SNew(SCanvas)
			+ SCanvas::Slot()
			.Position(TAttribute<FVector2D>::CreateLambda([this, Chat]() { return FVector2D(Chat->InputRect().Min) / Scale(); }))
			.Size(TAttribute<FVector2D>::CreateLambda([this, Chat]() { return FVector2D(Chat->InputRect().GetSize()) / Scale(); }))
			[
				SNew(SBox).VAlign(VAlign_Center)
				[
					SAssignNew(Edit, SEditableText)
					.Font(TAttribute<FSlateFontInfo>::CreateLambda([this, Chat]()
					{
						return FSlateFontInfo(FNWUILayout::GameFont(0), FMath::Max(8, FMath::RoundToInt(Chat->FontPixels() / Scale() * 1.05f)));
					}))
					.ColorAndOpacity(FSlateColor(ChatTextColor))
					.HintText(FText::FromString(NWText::Get(TEXT("ui_chat_placeholder"))))
					.OnTextCommitted_Lambda([Chat](const FText& T, ETextCommit::Type How)
					{
						if (How == ETextCommit::OnEnter)
						{
							Chat->Submit(T.ToString());
						}
						else if (How == ETextCommit::OnCleared)
						{
							Chat->Close();
						}
					})
				]
			]
		];
	}

private:
	float Scale() const
	{
		const float S = GetCachedGeometry().Scale;
		return S > 0.f ? S : 1.f;
	}
};

void FNWChat::Add(ENWChatChannel Channel, const FString& Sender, const FString& Text)
{
	// Server messages arrive as "#key|args".
	Lines.Add({Channel, Sender, NWText::Resolve(Text), FPlatformTime::Seconds()});
	if (Lines.Num() > 100)
	{
		Lines.RemoveAt(0, Lines.Num() - 100);
	}
}

void FNWChat::Open(ANWPlayerController* PC)
{
	if (bOpen || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	Owner = PC;
	bOpen = true;
	TSharedRef<SNWChatInput> Input = SNew(SNWChatInput).Chat(this);
	Edit = Input->Edit;
	// Strong reference: with SWeakWidget the field was destroyed on leaving here (chat opened fieldless and hung).
	Host = Input;
	GEngine->GameViewport->AddViewportWidgetContent(Input, 45);
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(Edit);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(Mode);
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	FSlateApplication::Get().SetKeyboardFocus(Edit, EFocusCause::SetDirectly);
}

void FNWChat::Close()
{
	if (!bOpen)
	{
		return;
	}
	bOpen = false;
	if (Host.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Host.ToSharedRef());
	}
	Host.Reset();
	Edit.Reset();
	if (ANWPlayerController* PC = Owner.Get())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->ResetIgnoreMoveInput();
		PC->ResetIgnoreLookInput();
	}
}

void FNWChat::Submit(const FString& Text)
{
	if (ANWPlayerController* PC = Owner.Get(); PC && !Text.TrimStartAndEnd().IsEmpty())
	{
		PC->ServerSendChat(Text);
	}
	Close();
}

void FNWChat::Draw(UCanvas* Canvas)
{
	UFont* Font = FNWUILayout::GameFont(0);
	if (!Font)
	{
		return;
	}
	UIScale = Canvas->ClipY / 1080.f;
	const double Now = FPlatformTime::Seconds();
	// Box: 520 x 300 reference, above the lower-left corner.
	const float S = UIScale;
	const FBox2f Box(FVector2f(30.f, 600.f) * S, FVector2f(560.f, 900.f) * S);
	const float Pad = 10.f * S;
	const float InputH = 34.f * S;
	InputPixels = FBox2f(FVector2f(Box.Min.X + Pad, Box.Max.Y - InputH - Pad * 0.5f), FVector2f(Box.Max.X - Pad, Box.Max.Y - Pad * 0.5f));

	if (bOpen)
	{
		FCanvasTileItem Bg(FVector2D(Box.Min), FVector2D(Box.GetSize()), ChatBackground);
		Bg.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Bg);
		FCanvasBoxItem Frame(FVector2D(Box.Min), FVector2D(Box.GetSize()));
		Frame.SetColor(ChatFrameGray);
		Canvas->DrawItem(Frame);
		FCanvasBoxItem InputFrame(FVector2D(InputPixels.Min), FVector2D(InputPixels.GetSize()));
		InputFrame.SetColor(ChatFrameGray);
		Canvas->DrawItem(InputFrame);
	}

	// Lines from bottom to top, with word wrap.
	const float TextScale = 20.f * S / Font->LegacyFontSize;
	const float MaxW = Box.GetSize().X - Pad * 2.f;
	float Y = (bOpen ? InputPixels.Min.Y : Box.Max.Y) - Pad;
	const int32 MaxLines = bOpen ? 30 : 8;
	int32 Shown = 0;
	for (int32 i = Lines.Num() - 1; i >= 0 && Shown < MaxLines; --i, ++Shown)
	{
		const FLine& L = Lines[i];
		const double Age = Now - L.Time;
		const float Alpha = bOpen ? 1.f : FMath::Clamp(float(16.0 - Age), 0.f, 1.f);
		if (Alpha <= 0.f)
		{
			break;
		}
		const FString Prefix = L.Sender.IsEmpty() ? FString::Printf(TEXT("[%s] "), *ChannelName(L.Channel))
			: FString::Printf(TEXT("[%s] %s: "), *ChannelName(L.Channel), *L.Sender);
		// Wrap: prefix + text in lines of at most MaxW.
		TArray<FString> Words;
		(Prefix + L.Text).ParseIntoArrayWS(Words);
		TArray<FString> Wrapped;
		FString Cur;
		for (const FString& W : Words)
		{
			const FString Try = Cur.IsEmpty() ? W : Cur + TEXT(" ") + W;
			float TW = 0.f, TH = 0.f;
			Canvas->TextSize(Font, Try, TW, TH, TextScale, TextScale);
			if (TW > MaxW && !Cur.IsEmpty())
			{
				Wrapped.Add(Cur);
				Cur = W;
			}
			else
			{
				Cur = Try;
			}
		}
		Wrapped.Add(Cur);
		float LineH = 0.f, Dummy = 0.f;
		Canvas->TextSize(Font, TEXT("Ag"), Dummy, LineH, TextScale, TextScale);
		LineH *= 1.1f;
		Y -= LineH * Wrapped.Num();
		if (Y < Box.Min.Y + Pad)
		{
			break;
		}
		const int32 PrefixLen = Prefix.Len();
		int32 Consumed = 0;
		for (int32 w = 0; w < Wrapped.Num(); ++w)
		{
			const FString& Text = Wrapped[w];
			float X = Box.Min.X + Pad;
			const float LY = Y + LineH * w;
			// The prefix (channel and name) in the channel color; the rest in the game's text tone.
			const int32 PrefixHere = FMath::Clamp(PrefixLen - Consumed, 0, Text.Len());
			auto Put = [&](const FString& Part, const FLinearColor& Color)
			{
				if (Part.IsEmpty())
				{
					return;
				}
				FCanvasTextItem Item(FVector2D(X, LY), FText::FromString(Part), Font, Color.CopyWithNewOpacity(Alpha));
				Item.Scale = FVector2D(TextScale, TextScale);
				Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f * Alpha));
				Canvas->DrawItem(Item);
				float PW = 0.f, PH = 0.f;
				Canvas->TextSize(Font, Part, PW, PH, TextScale, TextScale);
				X += PW;
			};
			Put(Text.Left(PrefixHere), ChannelColor(L.Channel));
			Put(Text.Mid(PrefixHere), L.Channel == ENWChatChannel::System ? ChannelColor(L.Channel) : ChatTextColor);
			Consumed += Text.Len() + 1;
		}
	}
}

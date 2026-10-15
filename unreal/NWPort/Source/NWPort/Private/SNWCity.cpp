#include "SNWCity.h"

#include "Framework/Application/SlateApplication.h"
#include "NWCharacter.h"
#include "NWCrafting.h"
#include "NWInteractable.h"
#include "NWInventoryComponent.h"
#include "NWItems.h"
#include "NWPlayerController.h"
#include "NWSlate.h"
#include "NWText.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

using namespace NWSlate;

namespace
{
	const TCHAR* SpinnerTex = TEXT("lyshineui/images/frontend/image_loading_spinner.dds");
	constexpr double KeepAfterDone = 2.0;
	constexpr double GiveUp = 15.0;

	float Ease(double Since, double Len)
	{
		const float T = FMath::Clamp(float(Since / Len), 0.f, 1.f);
		return 1.f - (1.f - T) * (1.f - T);
	}

	const FNWItemDef* Def(FName Item)
	{
		return FNWItemCatalog::Get().Find(Item);
	}

	FString NameOf(FName Item)
	{
		const FNWItemDef* D = Def(Item);
		return D ? D->Name : Item.ToString();
	}

	TSharedRef<SWidget> Icon(FName Item, float Size)
	{
		const FNWItemDef* D = Def(Item);
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBorder).BorderImage(&St().White).BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.06f))]
				+ SOverlay::Slot()[SNew(SImage).Image(D ? Tex(D->Icon, Size) : nullptr)]
			];
	}

	FString Days(float Seconds)
	{
		const int32 H = FMath::Max(0, FMath::FloorToInt(Seconds / 3600.f));
		return H >= 24 ? FString::Printf(TEXT("%dd %dh"), H / 24, H % 24) : FString::Printf(TEXT("%dh %dm"), H, FMath::Max(0, FMath::FloorToInt(Seconds / 60.f)) % 60);
	}

	int64 ParseCoins(const FString& In)
	{
		FString T = In.TrimStartAndEnd().Replace(TEXT("."), TEXT("")).Replace(TEXT(","), TEXT("."));
		if (T.IsEmpty() || !T.IsNumeric())
		{
			return -1;
		}
		return FMath::Max<int64>(0, int64(FMath::RoundToDouble(FCString::Atod(*T) * 100.0)));
	}

	FString CoinsField(int64 Cents)
	{
		return FString::Printf(TEXT("%lld,%02lld"), Cents / 100, Cents % 100);
	}

	TSharedRef<SWidget> Divider()
	{
		return SNew(SBox).HeightOverride(1.f)[SNew(SBorder).BorderImage(&St().Divider)];
	}

	/** Progress bar (fraction 0..1), green when complete. */
	TSharedRef<SWidget> Bar(float Frac)
	{
		Frac = FMath::Clamp(Frac, 0.f, 1.f);
		const FLinearColor C = Frac >= 1.f ? FLinearColor(FColor(0x6F, 0xC2, 0x6F)) : Gold;
		return SNew(SBox).HeightOverride(6.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(FMath::Max(Frac, 0.001f))[SNew(SBorder).BorderImage(&St().White).BorderBackgroundColor(C)]
				+ SHorizontalBox::Slot().FillWidth(FMath::Max(1.f - Frac, 0.001f))[SNew(SBorder).BorderImage(&St().White).BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.08f))]
			];
	}

	/** "6,0%" (pt-br) or "6.0%". */
	FString Pct(float V)
	{
		FString S = FString::Printf(TEXT("%.1f%%"), V);
		return NWText::Lang() == TEXT("pt-br") ? S.Replace(TEXT("."), TEXT(",")) : S;
	}

	/** Buff text with the integer value ("+10% trade skill XP"). */
	FString BuffLabel(const FString& Buff)
	{
		return NWText::Format(*(TEXT("nw.city.buff_") + Buff), {FString::FromInt(FMath::RoundToInt(NWCity::Rules().BuffValue.FindRef(Buff)))});
	}

}

// ---------------------------------------------------------------- construction

void SNWCity::Construct(const FArguments& Args)
{
	Character = Args._Character;
	Title = Args._Title;
	bGovernance = Args._bGovernance;
	bShot = Args._bShot;
	TabAt = Clock();
	LastRefresh = Clock();
	ANWCharacter* Me = Character.Get();
	OpenSeq = Me && Me->Interactor ? Me->Interactor->CitySeq : 0;
	if (Me && Me->Interactor && !bShot)
	{
		Me->Interactor->CityDone.Reset();
		Me->Interactor->ServerCityOpen();
	}
	if (bShot)
	{
		OpenSeq = -1;
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(&St().Backdrop)
		.Padding(0.f)
		.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		[
			SNew(SBox)
			.MaxDesiredWidth(1560.f)
			.HAlign(HAlign_Fill)
			.Padding(FMargin(48.f, 30.f, 48.f, 36.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Txt(FText::FromString(Title).ToUpper(), 3, 30, Tan)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
						[
							SNew(STextBlock).Font(Font(1, 14)).ColorAndOpacity(Gray)
							.Text_Lambda([this]()
							{
								const FNWCityState& S = State();
								const FString Who = S.bHasOwner ? S.OwnerName : NWText::Get(TEXT("nw.city.no_governor"));
								return FText::FromString(S.Name.IsEmpty() ? FString() : FString::Printf(TEXT("%s  ·  %s %s"), *S.Name,
									*NWText::Get(TEXT("nw.city.governor")), *Who));
							})
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
					[
						SNew(SBox).Visibility_Lambda([this]()
						{
							return Pending.ContainsByPredicate([](const FPending& P) { return !P.bDone; }) ? EVisibility::Visible : EVisibility::Hidden;
						})
						[
							Spinner(22.f)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						bGovernance
						? Treasury(TAttribute<int64>::CreateLambda([this]() { return State().Treasury; }), 2, 20)
						: Coins(TAttribute<int64>::CreateLambda([this]() { return ShownCoins(); }), 2, 20, Gold)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(24.f, 0.f, 0.f, 0.f)
					[
						CloseButton([this]() { Close(); })
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 18.f)[SAssignNew(Panel, SBox)]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SBorder)
						.BorderImage(&St().None)
						.Padding(0.f)
						.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.f, 1.f, 1.f, Loading() ? 0.f : Ease(Clock() - TabAt, 0.18)); })
						[
							SAssignNew(Body, SBox)
						]
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SBox).Visibility_Lambda([this]() { return Loading() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						[
							Spinner(56.f)
						]
					]
				]
			]
		]
	];
	Rebuild();
}

SNWCity::~SNWCity()
{
	if (ANWCharacter* Me = Character.Get(); Me && Me->Interactor && !bShot)
	{
		Me->Interactor->ServerCityClose();
	}
}

TSharedRef<SWidget> SNWCity::Spinner(float Size)
{
	return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
		[
			SNew(SImage)
			.Image(Tex(SpinnerTex, Size))
			.RenderTransformPivot(FVector2D(0.5, 0.5))
			.RenderTransform_Lambda([this]() -> TOptional<FSlateRenderTransform>
			{
				return FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(float(FMath::Fmod(Clock() * 300.0, 360.0)))));
			})
		];
}

TArray<FString> SNWCity::TabNames() const
{
	if (!bGovernance)
	{
		return {NWText::Get(TEXT("nw.city.sell_tab")), NWText::Get(TEXT("nw.city.buy_tab")), NWText::Get(TEXT("nw.city.info_tab"))};
	}
	if (State().bMine)
	{
		return {NWText::Get(TEXT("nw.city.summary")), NWText::Get(TEXT("nw.city.prices")), NWText::Get(TEXT("nw.city.stations")),
			NWText::Get(TEXT("nw.city.defense")), NWText::Get(TEXT("nw.city.government"))};
	}
	return {NWText::Get(TEXT("nw.city.summary")), NWText::Get(TEXT("nw.city.government"))};
}

TSharedRef<SWidget> SNWCity::BuildTabs()
{
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
	const TArray<FString> Names = TabNames();
	for (int32 t = 0; t < Names.Num(); ++t)
	{
		Box->AddSlot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(&St().Flat)
				.OnClicked_Lambda([this, t]()
				{
					if (Tab != t)
					{
						Tab = t;
						TabAt = Clock();
						Selected = NAME_None;
						Count = 1;
						Rebuild();
					}
					return FReply::Handled();
				})
				[
					Txt(Names[t], 2, 17, Tab == t ? Tan : Gray)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 0.f)
			[
				SNew(SBox).HeightOverride(3.f)[SNew(SBorder).BorderImage(&St().White).BorderBackgroundColor(WithA(Gold, Tab == t ? 1.f : 0.f))]
			]
		];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Box]
		+ SVerticalBox::Slot().AutoHeight()[Divider()];
}

void SNWCity::Rebuild()
{
	if (!Body.IsValid())
	{
		return;
	}
	const int32 N = TabNames().Num();
	Tab = FMath::Clamp(Tab, 0, N - 1);
	Panel->SetContent(BuildTabs());
	TSharedRef<SWidget> W = SNullWidget::NullWidget;
	if (!bGovernance)
	{
		W = Tab == 0 ? BuildSellRaw() : Tab == 1 ? BuildBuyList() : BuildInfo();
	}
	else if (State().bMine)
	{
		W = Tab == 0 ? BuildSummary() : Tab == 1 ? BuildPrices() : Tab == 2 ? BuildStations() : Tab == 3 ? BuildDefense() : BuildGovernment();
	}
	else
	{
		W = Tab == 0 ? BuildSummary() : BuildGovernment();
	}
	Body->SetContent(W);
}

// ---------------------------------------------------------------- state and prediction

const FNWCityState& SNWCity::State() const
{
	static const FNWCityState Empty;
	const ANWCharacter* Me = Character.Get();
	return Me && Me->Interactor ? Me->Interactor->CityState : Empty;
}

double SNWCity::Clock() const
{
	return bShot ? 1000.0 : FPlatformTime::Seconds();
}

bool SNWCity::Loading() const
{
	const ANWCharacter* Me = Character.Get();
	return !bShot && Me && Me->Interactor && Me->Interactor->CitySeq == OpenSeq;
}

bool SNWCity::IsBusy(const FString& Key) const
{
	return Pending.ContainsByPredicate([&Key](const FPending& P) { return !P.bDone && P.Busy == Key; });
}

int32 SNWCity::BagCount(FName Item) const
{
	const ANWCharacter* Me = Character.Get();
	int32 N = Me && Me->Inventory ? Me->Inventory->CountOf(Item) : 0;
	for (const FPending& P : Pending)
	{
		if (P.Item == Item && !P.bDone)
		{
			N -= P.Qty;   // sold (+) leaves the bag; bought (−) comes in
		}
	}
	return FMath::Max(0, N);
}

int64 SNWCity::ShownCoins() const
{
	const ANWCharacter* Me = Character.Get();
	int64 C = Me && Me->Inventory ? Me->Inventory->Coins : 0;
	for (const FPending& P : Pending)
	{
		if (!P.bDone)
		{
			C += P.Coins;
		}
	}
	return C;
}

int32 SNWCity::ShownUsed(const FNWCityPrice& P, bool bSellRaw) const
{
	int32 U = P.Used;
	for (const FPending& X : Pending)
	{
		if (!X.bDone && X.Item == P.Item)
		{
			U += bSellRaw ? FMath::Max(0, X.Qty) : FMath::Max(0, -X.Qty);
		}
	}
	return U;
}

int32 SNWCity::Send(const FString& Busy, FName Item, int32 Qty, int64 Coins)
{
	FPending P;
	P.Req = NextReq++;
	P.Item = Item;
	P.Qty = Qty;
	P.Coins = Coins;
	P.Busy = Busy;
	P.At = Clock();
	Pending.Add(P);
	return P.Req;
}

void SNWCity::Consume()
{
	ANWCharacter* Me = Character.Get();
	if (!Me || !Me->Interactor)
	{
		return;
	}
	const double Now = Clock();
	bool bChanged = false;
	for (const TPair<int32, bool>& D : Me->Interactor->CityDone)
	{
		const int32 i = Pending.IndexOfByPredicate([&D](const FPending& P) { return P.Req == D.Key; });
		if (i == INDEX_NONE)
		{
			continue;
		}
		if (!D.Value)
		{
			Flash.Add(Pending[i].Busy, Now);
			Pending.RemoveAt(i);
		}
		else
		{
			Pending[i].bDone = true;
			Pending[i].At = Now;
		}
		bChanged = true;
	}
	Me->Interactor->CityDone.Reset();
	const int32 Before = Pending.Num();
	Pending.RemoveAll([Now](const FPending& P) { return P.bDone ? Now - P.At > KeepAfterDone : Now - P.At > GiveUp; });
	(void)bChanged;
	(void)Before;   // the prediction is read by the lambdas; nothing to rebuild
}

void SNWCity::Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime)
{
	SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
	ANWCharacter* Me = Character.Get();
	if (!Me || !Me->Interactor || !Me->Inventory || bShot)
	{
		return;
	}
	Consume();
	if (Clock() - LastRefresh > 10.0)
	{
		LastRefresh = Clock();
		Me->Interactor->ServerCityRefresh();
	}
	if (SeenSeq != Me->Interactor->CitySeq)
	{
		SeenSeq = Me->Interactor->CitySeq;
		// Rows that finished and already came from the server leave the prediction.
		Pending.RemoveAll([](const FPending& P) { return P.bDone; });
		// Rebuilds only if something in the city changed (the server sends it every 10 s, almost always the same). In the trade
		// tabs the numbers come from lambdas: only rebuilds if the governor changed (swaps the tabs).
		const uint32 Sig = Signature();
		if (Sig != StateSig)
		{
			const bool bOwnerChanged = State().bMine != bWasMine;
			StateSig = Sig;
			bWasMine = State().bMine;
			const bool bEditing = bGovernance && Tab == 1 && (EditBuy.Num() || EditSell.Num() || EditTax >= 0.f);
			if ((bGovernance && !bEditing) || bOwnerChanged || !TradeHolder.IsValid())
			{
				Rebuild();
			}
		}
	}
}

uint32 SNWCity::Signature() const
{
	const FNWCityState& S = State();
	uint32 H = HashCombine(GetTypeHash(S.bValid), GetTypeHash(S.bMine));
	H = HashCombine(H, HashCombine(GetTypeHash(S.Treasury), GetTypeHash(S.Tax)));
	H = HashCombine(H, HashCombine(GetTypeHash(S.OwnerName), GetTypeHash(S.Defense)));
	for (const FNWCityWeapon& X : S.Weapons)
	{
		H = HashCombine(H, HashCombine(GetTypeHash(X.Tier), GetTypeHash(X.WorkTarget)));
	}
	H = HashCombine(H, HashCombine(GetTypeHash(S.WeekRawUnits), GetTypeHash(S.WeekListUnits)));
	H = HashCombine(H, HashCombine(GetTypeHash(S.WeekBuffs), GetTypeHash(S.bLastAttackWon)));
	H = HashCombine(H, HashCombine(GetTypeHash(S.bAuction), GetTypeHash(S.AuctionBid)));
	H = HashCombine(H, HashCombine(GetTypeHash(S.bAuctionMine), GetTypeHash(S.SaleGraceDays > 0.f)));
	for (const FNWCityStation& X : S.Stations)
	{
		H = HashCombine(H, HashCombine(GetTypeHash(X.Level), GetTypeHash(X.Reached)));
		H = HashCombine(H, HashCombine(GetTypeHash(X.Tier), GetTypeHash(X.Work)));
	}
	for (const FNWCityStock& X : S.Stock)
	{
		H = HashCombine(H, HashCombine(GetTypeHash(X.Item), GetTypeHash(X.Quantity)));
	}
	for (const FNWCityBuff& X : S.Buffs)
	{
		H = HashCombine(H, HashCombine(GetTypeHash(X.Station), GetTypeHash(X.Buff)));
	}
	for (const FNWCityPrice& X : S.Buy)
	{
		H = HashCombine(H, GetTypeHash(X.Price));
	}
	for (const FNWCityPrice& X : S.Sell)
	{
		H = HashCombine(H, GetTypeHash(X.Price));
	}
	return H;
}

FReply SNWCity::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Key)
{
	if (Key.GetKey() == EKeys::Escape)
	{
		Close();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(Geometry, Key);
}

void SNWCity::Close()
{
	if (ANWCharacter* Me = Character.Get())
	{
		if (ANWPlayerController* PC = Cast<ANWPlayerController>(Me->GetController()))
		{
			PC->CloseMenus();
		}
	}
}

FString SNWCity::StationName(const FString& Station) const
{
	for (const FNWCrafting::FStation& S : FNWCrafting::Get().Stations())
	{
		if (S.Type == Station)
		{
			if (const FString* N = S.Name.Find(NWText::Lang()))
			{
				// The game name carries the table's scale ("Forge Scale 5"); the scale here is the city's.
				FString Name = *N;
				for (const TCHAR* Word : {TEXT(" Escala "), TEXT(" Tier ")})
				{
					const int32 At = Name.Find(Word, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
					if (At != INDEX_NONE && Name.Mid(At + FCString::Strlen(Word)).IsNumeric())
					{
						Name.LeftInline(At);
					}
				}
				return Name;
			}
		}
	}
	return Station;
}

// ---------------------------------------------------------------- pieces

TSharedRef<SWidget> SNWCity::ActionButton(const FString& Label, const FString& BusyKey, TFunction<bool()> Enabled, TFunction<void()> Do, bool bPrimary)
{
	const int32 Size = bPrimary ? 16 : 14;
	return SNew(SBox).HeightOverride(48.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SButton)
				.ButtonStyle(bPrimary ? &St().Primary : &St().Button)
				.HAlign(HAlign_Center).VAlign(VAlign_Center)
				.IsEnabled_Lambda([this, Enabled, BusyKey]() { return !IsBusy(BusyKey) && (!Enabled || Enabled()); })
				.OnClicked_Lambda([Do]() { Do(); return FReply::Handled(); })
				[
					Txt(Label, 2, Size, Tan)
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f, 0.f)
			[
				SNew(SBox).Visibility_Lambda([this, BusyKey]() { return IsBusy(BusyKey) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					Spinner(24.f)
				]
			]
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.Visibility(EVisibility::HitTestInvisible)
				.BorderImage(&St().White)
				.BorderBackgroundColor_Lambda([this, BusyKey]()
				{
					const double* F = Flash.Find(BusyKey);
					const double Since = F ? Clock() - *F : 100.0;
					return WithA(Red, Since < 0.8 ? 0.35f * (1.f - Ease(Since, 0.8)) : 0.f);
				})
			]
		];
}

// ---------------------------------------------------------------- Town Project Board (players)

const FNWCityPrice* SNWCity::PriceOf(FName Item, bool bSellRaw) const
{
	const TArray<FNWCityPrice>& List = bSellRaw ? State().Buy : State().Sell;
	return List.FindByPredicate([Item](const FNWCityPrice& P) { return P.Item == Item; });
}

int32 SNWCity::MaxCount(FName Item, bool bSellRaw) const
{
	const FNWCityPrice* P = PriceOf(Item, bSellRaw);
	if (!P)
	{
		return 0;
	}
	const int32 Left = FMath::Max(0, P->Limit - ShownUsed(*P, bSellRaw));
	if (bSellRaw)
	{
		return FMath::Min(Left, BagCount(Item));
	}
	return P->Price > 0 ? int32(FMath::Min<int64>(Left, ShownCoins() / P->Price)) : 0;
}

TSharedRef<SWidget> SNWCity::BuildSellRaw()
{
	return BuildTradeList(true);
}

TSharedRef<SWidget> SNWCity::BuildBuyList()
{
	return BuildTradeList(false);
}

TSharedRef<SWidget> SNWCity::BuildTradeList(bool bSellRaw)
{
	// List built once per tab: numbers, color and selection come from lambdas (no flicker, keeps the scroll).
	TSharedRef<SScrollBox> Rows0 = SNew(SScrollBox).Style(&St().ScrollBox).ScrollBarStyle(&St().Scroll);
	TArray<FNWCityPrice> Rows = bSellRaw ? State().Buy : State().Sell;
	if (bSellRaw)
	{
		Rows.StableSort([this](const FNWCityPrice& A, const FNWCityPrice& B) { return BagCount(A.Item) > BagCount(B.Item); });
	}
	for (const FNWCityPrice& P : Rows)
	{
		const FName Item = P.Item;
		TSharedRef<SHorizontalBox> RowLine = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[Icon(Item, 42.f)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Font(1, 15)).Text(FText::FromString(NameOf(Item)))
				.ColorAndOpacity_Lambda([this, Item, bSellRaw]() { return FSlateColor(!bSellRaw || BagCount(Item) > 0 ? Tan : Gray); })
			];
		if (bSellRaw)
		{
			RowLine->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(16.f, 0.f)
			[
				SNew(SBox).WidthOverride(110.f).HAlign(HAlign_Right)
				[
					SNew(STextBlock).Font(Font(0, 14)).ColorAndOpacity(Gray).Text_Lambda([this, Item]() { return FText::AsNumber(BagCount(Item)); })
				]
			];
		}
		RowLine->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(120.f).HAlign(HAlign_Right)
			[
				Coins(TAttribute<int64>::CreateLambda([this, Item, bSellRaw]() { const FNWCityPrice* X = PriceOf(Item, bSellRaw); return X ? X->Price : 0; }), 2, 15, Gold)
			]
		];
		Rows0->AddSlot().Padding(0.f, 1.f)
		[
			SNew(SButton)
			.ButtonStyle(&St().Flat)
			.OnClicked_Lambda([this, Item, bSellRaw]()
			{
				if (Selected != Item)
				{
					Selected = Item;
					Count = 1;
					if (TradeHolder.IsValid())
					{
						TradeHolder->SetContent(BuildTradePanel(bSellRaw));
					}
				}
				return FReply::Handled();
			})
			[
				SNew(SBorder)
				.BorderImage(&St().White)
				.BorderBackgroundColor_Lambda([this, Item]() { return Selected == Item ? WithA(Gold, 0.13f) : FLinearColor(1.f, 1.f, 1.f, 0.025f); })
				.Padding(FMargin(8.f, 6.f))
				[
					RowLine
				]
			]
		];
	}
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 18.f, 0.f)[Rows0]
		+ SHorizontalBox::Slot().AutoWidth()[SAssignNew(TradeHolder, SBox).WidthOverride(430.f)[BuildTradePanel(bSellRaw)]];
}

TSharedRef<SWidget> SNWCity::BuildTradePanel(bool bSellRaw)
{
	if (!PriceOf(Selected, bSellRaw))
	{
		return SNew(SBox).WidthOverride(430.f);
	}
	const FName Item = Selected;
	auto Price = [this, Item, bSellRaw]() { const FNWCityPrice* X = PriceOf(Item, bSellRaw); return X ? X->Price : int64(0); };
	auto Left = [this, Item, bSellRaw]()
	{
		const FNWCityPrice* X = PriceOf(Item, bSellRaw);
		return X ? FMath::Max(0, X->Limit - ShownUsed(*X, bSellRaw)) : 0;
	};
	auto Max = [this, Item, bSellRaw]() { return MaxCount(Item, bSellRaw); };
	auto Row = [](const FString& Label, TSharedRef<SWidget> Right)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(Label, 0, 14, Gray)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Right];
	};
	auto Step = [this, Max](const FString& Label, int32 Delta)
	{
		return SNew(SBox).WidthOverride(Delta == 0 ? 72.f : 48.f).HeightOverride(36.f)
			[
				SNew(SButton)
				.ButtonStyle(&St().Button)
				.ContentPadding(FMargin(0.f))
				.HAlign(HAlign_Center).VAlign(VAlign_Center)
				.OnClicked_Lambda([this, Max, Delta]()
				{
					Count = Delta == 0 ? FMath::Max(1, Max()) : FMath::Clamp(Count + Delta, 1, FMath::Max(1, Max()));
					return FReply::Handled();
				})
				[
					Txt(Label, 2, 14, Tan)
				]
			];
	};
	auto Number = [](TFunction<int64()> Get)
	{
		return SNew(STextBlock).Font(Font(1, 14)).ColorAndOpacity(Tan).Text_Lambda([Get]() { return FText::AsNumber(Get()); });
	};
	const FString Busy = (bSellRaw ? TEXT("sell:") : TEXT("buy:")) + Item.ToString();
	TSharedRef<SVerticalBox> V = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 16.f, 0.f)[Icon(Item, 72.f)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Font(2, 18)).ColorAndOpacity(Tan).AutoWrapText(true).Text(FText::FromString(NameOf(Item)))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f)[Divider()]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[Row(NWText::Get(TEXT("ui_listingunitprice")), Coins(TAttribute<int64>::CreateLambda(Price), 2, 15, Gold))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[Row(NWText::Get(TEXT("nw.city.in_bag")), Number([this, Item]() { return int64(BagCount(Item)); }))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[Row(NWText::Get(TEXT("nw.city.week_left")), Number([Left]() { return int64(Left()); }))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)
		[
			Row(NWText::Get(TEXT("ui_quantity")),
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Step(TEXT("−"), -1)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(80.f).HAlign(HAlign_Center)
					[
						SNew(STextBlock).Font(Font(2, 16)).ColorAndOpacity(Tan)
						.Text_Lambda([this, Max]() { return FText::AsNumber(FMath::Clamp(Count, 1, FMath::Max(1, Max()))); })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()[Step(TEXT("+"), 1)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(6.f, 0.f, 0.f, 0.f)[Step(NWText::Get(TEXT("nw.city.max")), 0)])
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)[Divider()]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(NWText::Get(TEXT("ui_total")), 2, 17, Tan)]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Coins(TAttribute<int64>::CreateLambda([this, Price, Max]() { return Price() * FMath::Clamp(Count, 1, FMath::Max(1, Max())); }), 2, 18, Gold)
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
		[
			ActionButton(NWText::Get(bSellRaw ? TEXT("nw.city.sell") : TEXT("ui_buy")), Busy,
				[Max]() { return Max() > 0; },
				[this, Item, Price, Max, bSellRaw, Busy]()
				{
					ANWCharacter* Me = Character.Get();
					if (!Me || !Me->Interactor || Max() <= 0)
					{
						return;
					}
					const int32 N = FMath::Clamp(Count, 1, Max());
					const int64 Unit = Price();
					const int64 Total = Unit * N;
					const int32 Req = Send(Busy, Item, bSellRaw ? N : -N, bSellRaw ? Total : -Total);
					bSellRaw ? Me->Interactor->ServerCitySellRaw(Req, Item, N, Unit) : Me->Interactor->ServerCityBuyItem(Req, Item, N, Unit);
					Count = 1;
				})
		];
	return SNew(SBox).WidthOverride(430.f)[SNew(SBorder).BorderImage(&St().Panel).Padding(FMargin(22.f, 20.f))[V]];
}

TSharedRef<SWidget> SNWCity::BuildInfo()
{
	const FNWCityState& S = State();
	TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
	V->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[Txt(NWText::Get(TEXT("nw.city.stations")), 2, 17, Tan)];
	TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10.f, 10.f));
	for (const FNWCityStation& St0 : S.Stations)
	{
		TSharedRef<SHorizontalBox> Buffs = SNew(SHorizontalBox);
		for (const FNWCityBuff& B : S.Buffs)
		{
			if (B.Station == St0.Station)
			{
				Buffs->AddSlot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)[Txt(BuffLabel(B.Buff), 0, 13, Gold)];
			}
		}
		Wrap->AddSlot()
		[
			SNew(SBox).WidthOverride(330.f)
			[
				SNew(SBorder).BorderImage(&St().Panel).Padding(FMargin(16.f, 12.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Txt(StationName(St0.Station), 1, 15, Tan)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[Txt(TierText(St0), 2, 14, Gold)]
					+ SVerticalBox::Slot().AutoHeight()[Buffs]
				]
			]
		];
	}
	V->AddSlot().AutoHeight()[Wrap];
	return SNew(SScrollBox).Style(&St().ScrollBox).ScrollBarStyle(&St().Scroll) + SScrollBox::Slot()[V];
}

FString SNWCity::TierText(const FNWCityStation& S) const
{
	FString T = NWText::Format(TEXT("nw.city.tier"), {FString::FromInt(S.Tier)});
	if (S.Level > 0)
	{
		T += TEXT("  ·  ") + NWText::Format(TEXT("nw.city.level"), {FString::FromInt(S.Level)});
	}
	return T;
}

FString SNWCity::LostName(const FString& Id) const
{
	if (NWCity::Rules().Weapons.Contains(Id))
	{
		return NWText::Get(*(TEXT("siege_turret_") + Id));
	}
	return StationName(Id);
}

TSharedRef<SWidget> SNWCity::Basket(const NWCity::FUp& Up, float Share, bool bCoins)
{
	// Construction basket: each raw material with stock / order (red if missing) and the coins from the city's ledger.
	const FNWCityState& S = State();
	TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
	for (const TPair<FName, int64>& M : Up.Materials)
	{
		const int64 Need = int64(FMath::RoundToDouble(M.Value * Share));
		const int64 Have = S.StockOf(M.Key);
		V->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[Icon(M.Key, 26.f)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(NameOf(M.Key), 0, 13, Gray)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Txt(FString::Printf(TEXT("%s / %s"), *FText::AsNumber(Have).ToString(), *FText::AsNumber(Need).ToString()), 1, 13, Have >= Need ? Tan : Red)
			]
		];
	}
	if (bCoins && Up.Coins > 0)
	{
		V->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)[SNew(SBox).HAlign(HAlign_Right)[Treasury(Up.Coins, 1, 13)]];
	}
	return V;
}

bool SNWCity::CanPay(const NWCity::FUp& Up, float Share, bool bCoins) const
{
	const FNWCityState& S = State();
	for (const TPair<FName, int64>& M : Up.Materials)
	{
		if (S.StockOf(M.Key) < int64(FMath::RoundToDouble(M.Value * Share)))
		{
			return false;
		}
	}
	return !bCoins || S.Treasury >= Up.Coins;
}

// ---------------------------------------------------------------- Territory Planning Board (governor)

TSharedRef<SWidget> SNWCity::BuildSummary()
{
	// Governor summary; the weekly target lives only in nw-core (maintainer 2026-10-15).
	const FNWCityState& S = State();
	auto LineRow = [](const FString& Label, TSharedRef<SWidget> Right)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(Label, 0, 15, Gray)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Right];
	};
	TArray<FString> Lost;
	for (const FString& X : S.LastAttackLost)
	{
		Lost.Add(LostName(X));
	}
	const FString Attack = S.NextAttackIn >= 0.f ? NWText::Format(TEXT("nw.city.next_attack"), {Days(S.NextAttackIn)}) : FString(TEXT("—"));
	const FString Last = !S.bHasLastAttack ? FString(TEXT("—"))
		: S.bLastAttackWon ? NWText::Get(TEXT("nw.city.last_attack_won"))
		: NWText::Format(TEXT("nw.city.last_attack_lost"), {FString::Join(Lost, TEXT(", "))});
	TSharedRef<SVerticalBox> Left = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)[LineRow(NWText::Get(TEXT("nw.city.treasury")), Treasury(S.Treasury, 2, 18))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)[LineRow(NWText::Get(TEXT("nw.city.tax")), Txt(Pct(S.Tax * 100.f), 1, 15, Tan))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)[LineRow(NWText::Get(TEXT("nw.city.defense_total")), Txt(FString::FromInt(S.Defense), 1, 15, Tan))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)[LineRow(NWText::Get(TEXT("nw.city.attacks")), Txt(Attack, 1, 15, Tan))]
		+ SVerticalBox::Slot().AutoHeight()[LineRow(NWText::Get(TEXT("nw.city.last_attack")), Txt(Last, 1, 15, S.bHasLastAttack && !S.bLastAttackWon ? Red : Tan))];
	return SNew(SBox).HAlign(HAlign_Left).VAlign(VAlign_Top)
		[
			SNew(SBox).WidthOverride(640.f)[SNew(SBorder).BorderImage(&St().Panel).Padding(FMargin(24.f, 20.f))[Left]]
		];
}

TSharedRef<SWidget> SNWCity::PriceEditor(FName Item, const FNWCityPrice& P, bool bBuy)
{
	TMap<FName, int64>& Edit = bBuy ? EditBuy : EditSell;
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[Icon(Item, 36.f)]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(NameOf(Item), 1, 14, Tan)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f)
		[
			Txt(FString::Printf(TEXT("%s – %s"), *CoinsField(P.Min), *CoinsField(P.Max)), 0, 13, Dim)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(110.f)
			[
				SNew(SEditableTextBox)
				.Style(&St().Field)
				.Font(Font(2, 14))
				.Justification(ETextJustify::Right)
				.SelectAllTextWhenFocused(true)
				.Text(FText::FromString(CoinsField(Edit.Contains(Item) ? Edit[Item] : P.Price)))
				.ForegroundColor_Lambda([this, Item, P, bBuy]()
				{
					const TMap<FName, int64>& E = bBuy ? EditBuy : EditSell;
					const int64 V = E.Contains(Item) ? E[Item] : P.Price;
					return FSlateColor(V < P.Min || V > P.Max ? Red : Tan);
				})
				.OnTextChanged_Lambda([this, Item, bBuy](const FText& T)
				{
					const int64 V = ParseCoins(T.ToString());
					(bBuy ? EditBuy : EditSell).Add(Item, V);
				})
			]
		];
}

TSharedRef<SWidget> SNWCity::BuildPrices()
{
	const FNWCityState& S = State();
	auto Column = [this](const FString& Head, const TArray<FNWCityPrice>& List, bool bBuy)
	{
		TSharedRef<SScrollBox> Box = SNew(SScrollBox).Style(&St().ScrollBox).ScrollBarStyle(&St().Scroll);
		for (const FNWCityPrice& P : List)
		{
			Box->AddSlot().Padding(0.f, 3.f)[PriceEditor(P.Item, P, bBuy)];
		}
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[Txt(Head, 2, 17, Tan)]
			+ SVerticalBox::Slot().FillHeight(1.f)[Box];
	};
	auto Valid = [this]()
	{
		const FNWCityState& St0 = State();
		for (const TPair<FName, int64>& E : EditBuy)
		{
			const FNWCityPrice* P = St0.Buy.FindByPredicate([&E](const FNWCityPrice& X) { return X.Item == E.Key; });
			if (!P || E.Value < P->Min || E.Value > P->Max)
			{
				return false;
			}
		}
		for (const TPair<FName, int64>& E : EditSell)
		{
			const FNWCityPrice* P = St0.Sell.FindByPredicate([&E](const FNWCityPrice& X) { return X.Item == E.Key; });
			if (!P || E.Value < P->Min || E.Value > P->Max)
			{
				return false;
			}
		}
		return EditTax < 0.f || (EditTax >= St0.TaxMin && EditTax <= St0.TaxMax);
	};
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 24.f, 0.f)[Column(NWText::Get(TEXT("nw.city.buy_prices")), S.Buy, true)]
			+ SHorizontalBox::Slot().FillWidth(1.f)[Column(NWText::Get(TEXT("nw.city.sell_prices")), S.Sell, false)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[Txt(NWText::Get(TEXT("nw.city.tax")), 0, 15, Gray)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(90.f)
				[
					SNew(SEditableTextBox)
					.Style(&St().Field)
					.Font(Font(2, 14))
					.Justification(ETextJustify::Right)
					.Text(FText::FromString(FString::Printf(TEXT("%.1f"), (EditTax >= 0.f ? EditTax : S.Tax) * 100.f)))
					.OnTextChanged_Lambda([this](const FText& T)
					{
						const FString V = T.ToString().Replace(TEXT(","), TEXT("."));
						EditTax = V.IsNumeric() ? FCString::Atof(*V) / 100.f : -2.f;
					})
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)
			[
				Txt(FString::Printf(TEXT("%% (%.0f – %.0f)"), S.TaxMin * 100.f, S.TaxMax * 100.f), 0, 13, Dim)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SSpacer)]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(240.f)
				[
					ActionButton(NWText::Get(TEXT("nw.city.save")), TEXT("manage"),
						[this, Valid]() { return Valid() && (EditBuy.Num() || EditSell.Num() || EditTax >= 0.f); },
						[this]()
						{
							ANWCharacter* Me = Character.Get();
							if (!Me || !Me->Interactor)
							{
								return;
							}
							TArray<FNWCityPrice> Buy, Sell;
							for (const TPair<FName, int64>& E : EditBuy)
							{
								FNWCityPrice P;
								P.Item = E.Key;
								P.Price = E.Value;
								Buy.Add(P);
							}
							for (const TPair<FName, int64>& E : EditSell)
							{
								FNWCityPrice P;
								P.Item = E.Key;
								P.Price = E.Value;
								Sell.Add(P);
							}
							Me->Interactor->ServerCityManage(Send(TEXT("manage")), Buy, Sell, EditTax);
							EditBuy.Reset();
							EditSell.Reset();
							EditTax = -1.f;
						})
				]
			]
		];
}

TSharedRef<SWidget> SNWCity::BuildStations()
{
	const FNWCityState& S = State();
	const NWCity::FRules& R = NWCity::Rules();
	TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12.f, 12.f));
	for (const FNWCityStation& Stn : S.Stations)
	{
		const FString Station = Stn.Station;
		TSharedRef<SVerticalBox> Card = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(StationName(Station), 1, 15, Tan)]
				+ SHorizontalBox::Slot().AutoWidth()[Txt(TierText(Stn), 2, 15, Gold)]
			];
		const FString Busy = TEXT("station:") + Station;
		if (!Stn.Work.IsEmpty())
		{
			// Construction in progress: what it turns into and how much is left.
			const FString What = Stn.Work == TEXT("tier") ? NWText::Format(TEXT("nw.city.tier"), {FString::FromInt(Stn.WorkTarget)})
				: NWText::Format(TEXT("nw.city.level"), {FString::FromInt(Stn.WorkTarget)});
			Card->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[Spinner(20.f)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(NWText::Format(TEXT("nw.city.work"), {What}), 1, 14, Tan)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(Days(Stn.WorkEndsIn), 1, 14, Gold)]
			];
		}
		else
		{
			bool bRepair = false;
			if (const NWCity::FUp* Up = NWCity::NextStationUp(Stn, bRepair))
			{
				const float Share = bRepair ? R.RepairFraction : 1.f;
				const FString Label = bRepair ? NWText::Get(TEXT("nw.city.repair"))
					: Stn.Tier < 5 ? NWText::Format(TEXT("nw.city.to_tier"), {FString::FromInt(Up->To)})
					: NWText::Format(TEXT("nw.city.to_level"), {FString::FromInt(Up->To)});
				Card->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 6.f)[Basket(*Up, Share, !bRepair)];
				Card->AddSlot().AutoHeight().Padding(0.f, 4.f)
				[
					ActionButton(Label, Busy, [this, Up, Share, bRepair]() { return CanPay(*Up, Share, !bRepair); },
						[this, Station, Busy]()
						{
							if (ANWCharacter* Me = Character.Get(); Me && Me->Interactor)
							{
								Me->Interactor->ServerCityProject(Send(Busy), TEXT("station"), Station, FString());
							}
						}, false)
				];
			}
		}
		// This table's weekly buffs.
		TSharedRef<SHorizontalBox> Buffs = SNew(SHorizontalBox);
		for (const TPair<FString, int64>& B : R.BuffCost)
		{
			if (B.Key == TEXT("economia"))
			{
				continue;   // no effect in the game yet
			}
			const FString Buff = B.Key;
			const bool bActive = S.HasBuff(Station, Buff);
			const FString BuffBusy = TEXT("buff:") + Station + TEXT(":") + Buff;
			const FString Label = BuffLabel(Buff);
			Buffs->AddSlot().FillWidth(1.f).Padding(0.f, 0.f, 6.f, 0.f)
			[
				bActive
				? StaticCastSharedRef<SWidget>(SNew(SBox).HeightOverride(48.f).VAlign(VAlign_Center).HAlign(HAlign_Center)[Txt(Label, 1, 13, Gold)])
				: ActionButton(Label, BuffBusy, [this, Cost = B.Value]() { return State().Treasury >= Cost; },
					[this, Station, Buff, BuffBusy]()
					{
						if (ANWCharacter* Me = Character.Get(); Me && Me->Interactor)
						{
							Me->Interactor->ServerCityProject(Send(BuffBusy), TEXT("buff"), Station, Buff);
						}
					}, false)
			];
		}
		Card->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)[Buffs];
		Wrap->AddSlot()[SNew(SBox).WidthOverride(440.f)[SNew(SBorder).BorderImage(&St().Panel).Padding(FMargin(16.f, 14.f))[Card]]];
	}
	return SNew(SScrollBox).Style(&St().ScrollBox).ScrollBarStyle(&St().Scroll) + SScrollBox::Slot()[Wrap];
}

TSharedRef<SWidget> SNWCity::BuildDefense()
{
	// The game's fort weapons, each with a scale; the city's defense is the sum.
	const FNWCityState& S = State();
	TArray<FString> Lost;
	for (const FString& X : S.LastAttackLost)
	{
		Lost.Add(LostName(X));
	}
	TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
	V->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(NWText::Get(TEXT("nw.city.defense_total")), 2, 17, Tan)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(FString::FromInt(S.Defense), 2, 22, Gold)]
	];
	V->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
	[
		Txt(S.NextAttackIn >= 0.f ? NWText::Format(TEXT("nw.city.next_attack"), {Days(S.NextAttackIn)}) : FString(TEXT("—")), 1, 14, Gray)
	];
	if (S.bHasLastAttack)
	{
		V->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
		[
			Txt(S.bLastAttackWon ? NWText::Get(TEXT("nw.city.last_attack_won")) : NWText::Format(TEXT("nw.city.last_attack_lost"), {FString::Join(Lost, TEXT(", "))}),
				1, 14, S.bLastAttackWon ? Tan : Red)
		];
	}
	TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12.f, 12.f));
	for (const FNWCityWeapon& W : S.Weapons)
	{
		const FString Weapon = W.Weapon;
		const FString Busy = TEXT("weapon:") + Weapon;
		TSharedRef<SVerticalBox> Card = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(LostName(Weapon), 1, 15, Tan)]
				+ SHorizontalBox::Slot().AutoWidth()[Txt(NWText::Format(TEXT("nw.city.tier"), {FString::FromInt(W.Tier)}), 2, 15, Gold)]
			];
		if (W.WorkTarget > 0)
		{
			Card->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[Spinner(20.f)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					Txt(NWText::Format(TEXT("nw.city.work"), {NWText::Format(TEXT("nw.city.tier"), {FString::FromInt(W.WorkTarget)})}), 1, 14, Tan)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(Days(W.WorkEndsIn), 1, 14, Gold)]
			];
		}
		else if (const NWCity::FUp* Up = NWCity::NextWeaponUp(W))
		{
			Card->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 6.f)[Basket(*Up, 1.f, true)];
			Card->AddSlot().AutoHeight().Padding(0.f, 4.f)
			[
				ActionButton(NWText::Format(TEXT("nw.city.to_tier"), {FString::FromInt(Up->To)}), Busy, [this, Up]() { return CanPay(*Up, 1.f, true); },
					[this, Weapon, Busy]()
					{
						if (ANWCharacter* Me = Character.Get(); Me && Me->Interactor)
						{
							Me->Interactor->ServerCityProject(Send(Busy), TEXT("weapon"), Weapon, FString());
						}
					}, false)
			];
		}
		Wrap->AddSlot()[SNew(SBox).WidthOverride(440.f)[SNew(SBorder).BorderImage(&St().Panel).Padding(FMargin(16.f, 14.f))[Card]]];
	}
	V->AddSlot().AutoHeight()[Wrap];
	return SNew(SScrollBox).Style(&St().ScrollBox).ScrollBarStyle(&St().Scroll) + SScrollBox::Slot()[V];
}

TSharedRef<SWidget> SNWCity::BuildGovernment()
{
	const FNWCityState& S = State();
	TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
	auto Add = [&V](TSharedRef<SWidget> W, float Bottom = 10.f) { V->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, Bottom)[W]; };
	auto Field = [this](int64 Initial, TFunction<void(int64)> Set)
	{
		return SNew(SBox).WidthOverride(220.f).HAlign(HAlign_Left)
			[
				SNew(SEditableTextBox)
				.Style(&St().Field)
				.Font(Font(2, 15))
				.Justification(ETextJustify::Right)
				.SelectAllTextWhenFocused(true)
				.Text(FText::FromString(Initial > 0 ? CoinsField(Initial) : FString()))
				.OnTextChanged_Lambda([Set](const FText& T) { Set(FMath::Max<int64>(0, ParseCoins(T.ToString()))); })
			];
	};
	Add(Txt(S.bHasOwner ? FString::Printf(TEXT("%s %s"), *NWText::Get(TEXT("nw.city.governor")), *S.OwnerName) : NWText::Get(TEXT("nw.city.no_governor")), 2, 18, Tan), 14.f);
	// Player coins: only here, where they pay (the top of the screen shows the city's ledger).
	auto YourCoins = [this]()
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(NWText::Get(TEXT("nw.city.your_coins")), 0, 14, Gray)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Coins(TAttribute<int64>::CreateLambda([this]() { return ShownCoins(); }), 1, 15, Gold)];
	};

	if (!S.bHasOwner)
	{
		// No governor: fixed price, whoever pays takes it.
		const int64 Ask = S.Price;
		Add(SNew(SBox).HAlign(HAlign_Right)[Coins(Ask, 2, 20, ShownCoins() >= Ask ? Gold : Red)]);
		Add(YourCoins(), 14.f);
		Add(ActionButton(NWText::Get(TEXT("nw.city.buy_city")), TEXT("buy_city"), [this, Ask]() { return ShownCoins() >= Ask; },
			[this, Ask]()
			{
				if (ANWCharacter* Me = Character.Get(); Me && Me->Interactor)
				{
					Me->Interactor->ServerCityBuy(Send(TEXT("buy_city"), NAME_None, 0, -Ask), Ask);
				}
			}), 0.f);
	}
	else if (S.bAuction)
	{
		// 24 h auction: highest bid, who placed it, time left; bid with the minimum already filled in.
		Add(Txt(NWText::Get(S.bAuctionForced ? TEXT("nw.city.auction_forced") : TEXT("nw.city.auction")), 2, 16, S.bAuctionForced ? Red : Gold));
		Add(Txt(NWText::Format(TEXT("nw.city.auction_ends"), {Days(S.AuctionEndsIn)}), 1, 14, Gray));
		Add(SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				Txt(S.AuctionBid >= 0 ? NWText::Format(TEXT("nw.city.top_bid"), {S.AuctionBidder}) : NWText::Get(TEXT("nw.city.start_bid")), 0, 15, Gray)
			]
			+ SHorizontalBox::Slot().AutoWidth()[Coins(S.AuctionBid >= 0 ? S.AuctionBid : S.AuctionStart, 2, 18, Gold)], 16.f);
		if (S.bAuctionMine)
		{
			Add(Txt(NWText::Get(TEXT("nw.city.your_bid")), 1, 15, Gold));
		}
		else if (!S.bMine)
		{
			if (EditBid < S.AuctionMinBid)
			{
				EditBid = S.AuctionMinBid;
			}
			Add(YourCoins());
			Add(Field(EditBid, [this](int64 V0) { EditBid = V0; }));
			Add(ActionButton(NWText::Get(TEXT("nw.city.place_bid")), TEXT("bid"),
				[this]() { return EditBid >= State().AuctionMinBid && ShownCoins() >= EditBid; },
				[this]()
				{
					if (ANWCharacter* Me = Character.Get(); Me && Me->Interactor)
					{
						Me->Interactor->ServerCityBid(Send(TEXT("bid"), NAME_None, 0, -EditBid), EditBid);
					}
				}), 0.f);
		}
		else if (!S.bAuctionForced && S.AuctionBid < 0)
		{
			Add(ActionButton(NWText::Get(TEXT("nw.city.cancel_auction")), TEXT("sale"), nullptr,
				[this]()
				{
					if (ANWCharacter* Me = Character.Get(); Me && Me->Interactor)
					{
						Me->Interactor->ServerCitySale(Send(TEXT("sale")), 0);
					}
				}, false), 0.f);
		}
	}
	else if (S.bMine)
	{
		// Governor: auction with whatever starting bid they want, after the grace period.
		if (S.SaleGraceDays > 0.f)
		{
			Add(Txt(NWText::Format(TEXT("nw.city.grace"), {FString::FromInt(FMath::CeilToInt(S.SaleGraceDays))}), 1, 15, Gray), 0.f);
		}
		else
		{
			Add(Txt(NWText::Get(TEXT("nw.city.start_bid")), 0, 15, Gray), 6.f);
			Add(Field(EditSalePrice, [this](int64 V0) { EditSalePrice = V0; }));
			Add(ActionButton(NWText::Get(TEXT("nw.city.start_auction")), TEXT("sale"), [this]() { return EditSalePrice > 0; },
				[this]()
				{
					if (ANWCharacter* Me = Character.Get(); Me && Me->Interactor)
					{
						Me->Interactor->ServerCitySale(Send(TEXT("sale")), EditSalePrice);
					}
				}), 0.f);
		}
	}
	return SNew(SBox).HAlign(HAlign_Left).VAlign(VAlign_Top)
		[
			SNew(SBox).WidthOverride(560.f)[SNew(SBorder).BorderImage(&St().Panel).Padding(FMargin(24.f, 20.f))[V]]
		];
}

// ---------------------------------------------------------------- offline capture

void SNWCity::ShotTab(int32 InTab)
{
	Tab = InTab;
	TabAt = Clock() - 10.0;
	Rebuild();
}

void SNWCity::ShotSelect(FName Item)
{
	Selected = Item;
	Rebuild();
}

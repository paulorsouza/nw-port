#include "SNWMarket.h"

#include "NWCity.h"
#include "NWSlate.h"

#include "Engine/Font.h"
#include "Framework/Application/SlateApplication.h"
#include "NWCharacter.h"
#include "NWInteractable.h"
#include "NWInventoryComponent.h"
#include "NWItemRules.h"
#include "NWPlayerController.h"
#include "NWText.h"
#include "NWUILayout.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/SToolTip.h"

namespace
{
	// ---------------------------------------------------------------- style (the game's colors and fonts)
	using namespace NWSlate;
	constexpr double KeepAfterDone = 2.0;   // bag/coins prediction after the end, until replication arrives
	constexpr double GiveUp = 15.0;         // no response from the server: gives up the prediction
	const TCHAR* SpinnerTex = TEXT("lyshineui/images/frontend/image_loading_spinner.dds");
	const TCHAR* SearchTex = TEXT("lyshineui/images/socialpane/icon_magnifying_glass.dds");
	const FName ColName("Name"), ColTier("Tier"), ColGs("Gs"), ColQuality("Quality"), ColPerks("Perks"), ColSeller("Seller"),
		ColQty("Qty"), ColPrice("Price"), ColLowest("Lowest"), ColExpires("Expires"), ColAction("Action");


	int64 ParseMoney(const FString& In)
	{
		FString T = In.TrimStartAndEnd().Replace(TEXT("."), TEXT("")).Replace(TEXT(","), TEXT("."));
		if (T.IsEmpty() || !T.IsNumeric())
		{
			return 0;
		}
		return FMath::Max<int64>(0, int64(FMath::RoundToDouble(FCString::Atod(*T) * 100.0)));
	}

	FString MoneyField(int64 Cents)
	{
		return Cents <= 0 ? FString() : FString::Printf(TEXT("%lld,%02lld"), Cents / 100, Cents % 100);
	}

	FString Percent(float V)
	{
		FString S = FString::Printf(TEXT("%.1f%%"), V);
		return NWText::Lang() == TEXT("pt-br") ? S.Replace(TEXT("."), TEXT(",")) : S;
	}

	FString Remaining(float Seconds)
	{
		const int32 H = FMath::Max(0, FMath::FloorToInt(Seconds / 3600.f));
		return H >= 24 ? FString::Printf(TEXT("%dd %dh"), H / 24, H % 24)
			: FString::Printf(TEXT("%dh %dm"), H, FMath::Max(0, FMath::FloorToInt(Seconds / 60.f)) % 60);
	}

	float Ease(double Since, double Len)
	{
		const float T = FMath::Clamp(float(Since / Len), 0.f, 1.f);
		return 1.f - (1.f - T) * (1.f - T);
	}

	const FNWItemDef* Def(const FNWItem& I)
	{
		return FNWItemCatalog::Get().Find(I.ItemId);
	}

	FString NameOf(const FNWItem& I)
	{
		const FNWItemDef* D = Def(I);
		return D ? D->Name : I.ItemId.ToString();
	}

	bool InCat(int32 Cat, FName ItemId)
	{
		if (Cat == 0)
		{
			return true;
		}
		const FNWItemDef* D = FNWItemCatalog::Get().Find(ItemId);
		const FString C = D ? FNWItemRules::EquipClass(*D) : FString();
		const FString Id = ItemId.ToString();
		switch (Cat)
		{
		case 1: return C == TEXT("arma") || C == TEXT("escudo");
		case 2: return C == TEXT("armadura");
		case 3: return C == TEXT("amuleto") || C == TEXT("anel") || C == TEXT("brinco");
		case 4: return C == TEXT("ferramenta");
		case 5: return Id.Contains(TEXT("Cut")) || Id.StartsWith(TEXT("Runeglass_"));
		case 6: return FNWItemRules::Get().MarketItems.Contains(ItemId);
		default: return true;
		}
	}

	/** Item icon over the rarity color. */
	TSharedRef<SWidget> ItemIcon(const FNWItem& I, float Size)
	{
		const FNWItemDef* D = Def(I);
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder).BorderImage(&St().White).BorderBackgroundColor(WithA(NWRarityColor(I.Rarity), I.Rarity > 0 ? 0.28f : 0.12f))
				]
				+ SOverlay::Slot()
				[
					SNew(SImage).Image(D ? Tex(D->Icon, Size) : nullptr)
				]
			];
	}

	/** Icons of the item's perks with the text in the tooltip. */
	TSharedRef<SWidget> PerkIcons(const FNWItem& I, float Size)
	{
		const FNWItemRules& IR = FNWItemRules::Get();
		TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
		for (FName P : I.Perks)
		{
			const FNWItemRules::FPerk* PD = IR.Perks.Find(P);
			const FString Tip = IR.PerkText(P, I.Quality, true) + TEXT("\n") + IR.PerkText(P, I.Quality, false);
			Box->AddSlot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(Size).HeightOverride(Size).ToolTipText(FText::FromString(Tip))
				[
					SNew(SImage).Image(PD ? Tex(PD->Icon, Size) : nullptr)
				]
			];
		}
		return Box;
	}

	/** Table row: each cell in a border that fades in smoothly, dims (waiting) and flashes (refused). */
	class SNWMarketRow : public SMultiColumnTableRow<SNWMarket::FRowPtr>
	{
	public:
		SLATE_BEGIN_ARGS(SNWMarketRow) {}
			SLATE_ARGUMENT(SNWMarket::FRowPtr, Row)
			SLATE_ARGUMENT(SNWMarket*, Market)
			SLATE_ARGUMENT(TFunction<TSharedRef<SWidget>(const FName&)>, Cell)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& Owner)
		{
			RowData = Args._Row;
			Market = Args._Market;
			Cell = Args._Cell;
			SMultiColumnTableRow<SNWMarket::FRowPtr>::Construct(FSuperRowType::FArguments().Style(&St().Row).Padding(FMargin(0.f, 1.f)), Owner);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
		{
			SNWMarket::FRowPtr R = RowData;
			SNWMarket* M = Market;
			return SNew(SBorder)
				.BorderImage(&St().White)
				.Padding(FMargin(8.f, 0.f))
				.VAlign(VAlign_Center)
				.BorderBackgroundColor_Lambda([R, M]()
				{
					const double Since = M->Clock() - M->FlashOf(R->Key);
					return WithA(Red, Since < 0.8 ? 0.35f * (1.f - Ease(Since, 0.8)) : 0.f);
				})
				.ColorAndOpacity_Lambda([R, M]()
				{
					const float In = Ease(M->Clock() - R->Born, 0.22);
					const float Wait = R->bCancelling ? 0.4f : R->bPending ? 0.65f : 1.f;
					return FLinearColor(1.f, 1.f, 1.f, In * Wait);
				})
				.RenderTransform_Lambda([R, M]() -> TOptional<FSlateRenderTransform>
				{
					const float In = Ease(M->Clock() - R->Born, 0.22);
					return FSlateRenderTransform(FVector2f((1.f - In) * 18.f, 0.f));
				})
				[
					SNew(SBox).HeightOverride(58.f).VAlign(VAlign_Center)
					[
						Cell(Column)
					]
				];
		}

	private:
		SNWMarket::FRowPtr RowData;
		SNWMarket* Market = nullptr;
		TFunction<TSharedRef<SWidget>(const FName&)> Cell;
	};
}

// ---------------------------------------------------------------- construction

void SNWMarket::Construct(const FArguments& Args)
{
	Character = Args._Character;
	Title = Args._Title;
	bShot = Args._bShot;
	SortCol[0] = ColPrice;
	SortCol[1] = ColName;
	SortCol[2] = ColExpires;
	SortMode[2] = EColumnSortMode::Descending;
	const double Now = Clock();
	PageAt = Now;
	LastRefresh = Now;
	ANWCharacter* Me = Character.Get();
	OpenSeq = Me && Me->Interactor ? Me->Interactor->MarketSeq : 0;
	if (Me && Me->Interactor && !bShot)
	{
		Me->Interactor->MarketDone.Reset();
		Me->Interactor->ServerMarketOpen();
	}
	if (bShot)
	{
		OpenSeq = -1;
	}

	TSharedRef<SWidgetSwitcher> Pages = SNew(SWidgetSwitcher).WidgetIndex_Lambda([this]() { return int32(Page); });
	// Buy: filters | search + table | item panel.
	Pages->AddSlot()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[BuildFilters()]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(18.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[BuildSearch()]
			+ SVerticalBox::Slot().FillHeight(1.f)[BuildList(EPage::Buy)]
		]
		+ SHorizontalBox::Slot().AutoWidth()[BuildPanel(EPage::Buy)]
	];
	// Sell: search + table | item panel.
	Pages->AddSlot()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 18.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[BuildSearch()]
			+ SVerticalBox::Slot().FillHeight(1.f)[BuildList(EPage::Sell)]
		]
		+ SHorizontalBox::Slot().AutoWidth()[BuildPanel(EPage::Sell)]
	];
	Pages->AddSlot()[BuildList(EPage::Orders)];

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(&St().Backdrop)
		.Padding(0.f)
		.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })   // nothing passes through to the game
		[
			SNew(SBox)
			.MaxDesiredWidth(1720.f)
			.HAlign(HAlign_Fill)
			.Padding(FMargin(48.f, 30.f, 48.f, 36.f))
			[
				SNew(SVerticalBox)
				// Title and coins (with a spinner while the server confirms).
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						Txt(FText::FromString(Title).ToUpper(), 3, 30, Tan)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
					[
						SNew(SBox).Visibility_Lambda([this]() { return AnyWaiting() ? EVisibility::Visible : EVisibility::Hidden; })
						[
							Spinner(22.f)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						Coins(TAttribute<int64>::CreateLambda([this]() { return ShownCoins(); }), 2, 20, Gold)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(24.f, 0.f, 0.f, 0.f)
					[
						CloseButton([this]() { Close(); })
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 18.f)[BuildTabs()]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SBorder)
						.BorderImage(&St().None)
						.Padding(0.f)
						.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.f, 1.f, 1.f, Loading() ? 0.f : Ease(Clock() - PageAt, 0.18)); })
						.RenderTransform_Lambda([this]() -> TOptional<FSlateRenderTransform>
						{
							return FSlateRenderTransform(FVector2f(0.f, (1.f - Ease(Clock() - PageAt, 0.18)) * 10.f));
						})
						[
							Pages
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

SNWMarket::~SNWMarket()
{
	if (ANWCharacter* Me = Character.Get(); Me && Me->Interactor && !bShot)
	{
		Me->Interactor->ServerMarketClose();
	}
}

void SNWMarket::Close()
{
	if (ANWCharacter* Me = Character.Get())
	{
		if (ANWPlayerController* PC = Cast<ANWPlayerController>(Me->GetController()))
		{
			PC->CloseMenus();
		}
	}
}

TSharedRef<SToolTip> SNWMarket::ItemTip(const FNWItem& I)
{
	// Same content as the item panel, in a dark box next to the mouse.
	return SNew(SToolTip)
		.BorderImage(&St().None)
		.TextMargin(0.f)
		[
			SNew(SBox).WidthOverride(400.f)
			[
				SNew(SBorder).BorderImage(&St().Tip).Padding(FMargin(18.f, 16.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ItemHeader(I)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f)[SNew(SBox).HeightOverride(1.f)[SNew(SBorder).BorderImage(&St().Divider)]]
					+ SVerticalBox::Slot().AutoHeight()[ItemDetails(I)]
				]
			]
		];
}

TSharedRef<SWidget> SNWMarket::Spinner(float Size)
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

TSharedRef<SWidget> SNWMarket::BuildTabs()
{
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
	for (int32 t = 0; t < 3; ++t)
	{
		const EPage P = EPage(t);
		Box->AddSlot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(&St().Flat)
				.OnClicked_Lambda([this, P]() { SetPage(P); return FReply::Handled(); })
				[
					SNew(STextBlock).Font(Font(2, 17))
					.ColorAndOpacity_Lambda([this, P]() { return FSlateColor(Page == P ? Tan : Gray); })
					.Text_Lambda([this, t]()
					{
						if (t == 0) return FText::FromString(NWText::Get(TEXT("ui_buy")));
						if (t == 1) return FText::FromString(NWText::Get(TEXT("ui_sell")));
						return FText::FromString(FString::Printf(TEXT("%s  %d/%d"), *NWText::Get(TEXT("ui_myorders")), MyOrders(),
							FNWItemRules::Get().MarketMaxOrders));
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 0.f)
			[
				SNew(SBox).HeightOverride(3.f)
				[
					SNew(SBorder).BorderImage(&St().White)
					.BorderBackgroundColor_Lambda([this, P]() { return WithA(Gold, Page == P ? Ease(Clock() - PageAt, 0.2) : 0.f); })
					.RenderTransformPivot(FVector2D(0.5, 0.5))
					.RenderTransform_Lambda([this, P]() -> TOptional<FSlateRenderTransform>
					{
						return FSlateRenderTransform(FScale2D(Page == P ? FMath::Max(0.05f, Ease(Clock() - PageAt, 0.2)) : 0.05f, 1.f));
					})
				]
			]
		];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Box]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(1.f)[SNew(SBorder).BorderImage(&St().Divider)]];
}

TSharedRef<SWidget> SNWMarket::BuildSearch()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(SBox).WidthOverride(22.f).HeightOverride(22.f)[SNew(SImage).Image(Tex(SearchTex, 22.f)).ColorAndOpacity(Gray)]
		]
		+ SHorizontalBox::Slot().MaxWidth(420.f)
		[
			SNew(SEditableTextBox)
			.Style(&St().Field)
			.Font(Font(0, 14))
			.Text_Lambda([this]() { return FText::FromString(Search); })
			.OnTextChanged_Lambda([this](const FText& T) { Search = T.ToString(); bDirty = true; })
		];
}

TSharedRef<SWidget> SNWMarket::BuildFilters()
{
	const FNWItemRules& IR = FNWItemRules::Get();
	auto Section = [](const FString& Label)
	{
		return SNew(SBox).Padding(FMargin(0.f, 16.f, 0.f, 6.f))[Txt(Label, 1, 12, Dim)];
	};
	// Toggle chip: golden outline when on.
	auto Chip = [this](TSharedRef<SWidget> Content, TFunction<bool()> On, TFunction<void()> Toggle)
	{
		return SNew(SButton)
			.ButtonStyle(&St().Chip)
			.OnClicked_Lambda([this, Toggle]() { Toggle(); bDirty = true; return FReply::Handled(); })
			[
				SNew(SBorder)
				.BorderImage(&St().None)
				.ColorAndOpacity_Lambda([On]() { return On() ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.55f); })
				[
					Content
				]
			];
	};
	auto ChipFrame = [](TSharedRef<SWidget> Button, TFunction<bool()> On)
	{
		return SNew(SOverlay)
			+ SOverlay::Slot()[Button]
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.Visibility(EVisibility::HitTestInvisible)
				.BorderImage_Lambda([On]()
				{
					static const FSlateRoundedBoxBrush OnB = Round(FLinearColor::Transparent, Gold, 1.f);
					static const FSlateNoResource Off;
					return On() ? static_cast<const FSlateBrush*>(&OnB) : static_cast<const FSlateBrush*>(&Off);
				})
			];
	};

	TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
	V->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(NWText::Get(TEXT("ui_filters")), 2, 15, Tan)]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).ButtonStyle(&St().Flat)
			.Visibility_Lambda([this]()
			{
				const bool bAny = Cat != ECat::All || Tiers.Num() || Rarities.Num() || MinGs > 0 || Attrs.Num() || PerkFilter.Num() || bAfford;
				return bAny ? EVisibility::Visible : EVisibility::Hidden;
			})
			.OnClicked_Lambda([this]()
			{
				Cat = ECat::All;
				Tiers.Reset();
				Rarities.Reset();
				MinGs = 0;
				Attrs.Reset();
				PerkFilter.Reset();
				bAfford = false;
				bDirty = true;
				return FReply::Handled();
			})
			[
				Txt(NWText::Get(TEXT("ui_clear_all")), 0, 12, Gold)
			]
		]
	];

	// Categories.
	static const TCHAR* CatKeys[int32(ECat::Num)] = {TEXT("ui_allitems"), TEXT("nw.market.cat_weapons"), TEXT("nw.market.cat_armor"),
		TEXT("nw.market.cat_jewelry"), TEXT("nw.market.cat_tools"), TEXT("nw.market.cat_gems"), TEXT("nw.market.cat_materials")};
	V->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f);
	for (int32 c = 0; c < int32(ECat::Num); ++c)
	{
		const ECat Which = ECat(c);
		V->AddSlot().AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(&St().Flat)
			.OnClicked_Lambda([this, Which]() { Cat = Which; bDirty = true; return FReply::Handled(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(SBox).WidthOverride(3.f).HeightOverride(18.f)
					[
						SNew(SBorder).BorderImage(&St().White)
						.BorderBackgroundColor_Lambda([this, Which]() { return WithA(Gold, Cat == Which ? 1.f : 0.f); })
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock).Font(Font(0, 14)).Text(FText::FromString(NWText::Get(CatKeys[c])))
					.ColorAndOpacity_Lambda([this, Which]() { return FSlateColor(Cat == Which ? Tan : Gray); })
				]
			]
		];
	}

	// Tier.
	V->AddSlot().AutoHeight()[Section(NWText::Get(TEXT("trade_column_tier")))];
	TSharedRef<SWrapBox> TierBox = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f, 6.f));
	for (int32 t = 1; t <= 5; ++t)
	{
		static const TCHAR* Roman[] = {TEXT(""), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V")};
		auto On = [this, t]() { return Tiers.Contains(t); };
		TierBox->AddSlot()[ChipFrame(Chip(Txt(Roman[t], 2, 13, Tan), On, [this, t]() { Tiers.Contains(t) ? (void)Tiers.Remove(t) : (void)Tiers.Add(t); }), On)];
	}
	V->AddSlot().AutoHeight()[TierBox];

	// Rarity.
	V->AddSlot().AutoHeight()[Section(NWText::Get(TEXT("trade_column_rarity")))];
	TSharedRef<SWrapBox> RarBox = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f, 6.f));
	for (int32 r = 0; r <= 4; ++r)
	{
		auto On = [this, r]() { return Rarities.Contains(r); };
		RarBox->AddSlot()
		[
			ChipFrame(Chip(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SBox).WidthOverride(9.f).HeightOverride(9.f)[SNew(SBorder).BorderImage(&St().White).BorderBackgroundColor(NWRarityColor(uint8(r)))]
				]
				+ SHorizontalBox::Slot().AutoWidth()[Txt(NWText::Get(*FString::Printf(TEXT("raritylevel%d_displayname"), r)), 0, 13, Tan)],
				On, [this, r]() { Rarities.Contains(r) ? (void)Rarities.Remove(r) : (void)Rarities.Add(r); }), On)
		];
	}
	V->AddSlot().AutoHeight()[RarBox];

	// Minimum GS.
	V->AddSlot().AutoHeight()[Section(NWText::Get(TEXT("ui_min_gear_score")))];
	V->AddSlot().AutoHeight()
	[
		SNew(SBox).WidthOverride(120.f)
		[
			SNew(SEditableTextBox)
			.Style(&St().Field)
			.Font(Font(0, 14))
			.Text_Lambda([this]() { return MinGs > 0 ? FText::AsNumber(MinGs) : FText::GetEmpty(); })
			.OnTextChanged_Lambda([this](const FText& T)
			{
				MinGs = FMath::Max(0, FCString::Atoi(*T.ToString()));
				bDirty = true;
			})
		]
	];

	// Attribute.
	V->AddSlot().AutoHeight()[Section(NWText::Get(TEXT("ui_attributes")))];
	TSharedRef<SWrapBox> AttrBox = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f, 6.f));
	for (int32 a = 0; a < IR.Attributes.Num(); ++a)
	{
		const uint8 A8 = uint8(a);
		auto On = [this, A8]() { return Attrs.Contains(A8); };
		const FString Short = NWText::Get(*(TEXT("ui_") + IR.Attributes[a].ToString())).Left(3).ToUpper();
		AttrBox->AddSlot()[ChipFrame(Chip(Txt(Short, 1, 13, Tan), On, [this, A8]() { Attrs.Contains(A8) ? (void)Attrs.Remove(A8) : (void)Attrs.Add(A8); }), On)];
	}
	V->AddSlot().AutoHeight()[AttrBox];

	// Perks that appear for sale (all chosen ones at the same time).
	V->AddSlot().AutoHeight()[Section(NWText::Get(TEXT("ui_perks")))];
	V->AddSlot().AutoHeight()[SAssignNew(PerkChips, SBox)];

	// Can afford.
	V->AddSlot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
	[
		ChipFrame(Chip(Txt(NWText::Get(TEXT("ui_canafford")), 0, 13, Tan), [this]() { return bAfford; }, [this]() { bAfford = !bAfford; }),
			[this]() { return bAfford; })
	];

	return SNew(SBox).WidthOverride(270.f)
		[
			SNew(SScrollBox).Style(&St().ScrollBox).ScrollBarStyle(&St().Scroll)
			+ SScrollBox::Slot()[V]
		];
}

TSharedRef<SWidget> SNWMarket::BuildPerkChips()
{
	const FNWItemRules& IR = FNWItemRules::Get();
	TSharedRef<SWrapBox> Box = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f, 6.f));
	for (FName P : BookPerks)
	{
		const FNWItemRules::FPerk* PD = IR.Perks.Find(P);
		const FString* Name = PD ? PD->Name.Find(NWText::Lang()) : nullptr;
		auto On = [this, P]() { return PerkFilter.Contains(P); };
		Box->AddSlot()
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SButton)
				.ButtonStyle(&St().Chip)
				.ToolTipText(FText::FromString(Name ? *Name : P.ToString()))
				.OnClicked_Lambda([this, P]() { PerkFilter.Contains(P) ? (void)PerkFilter.Remove(P) : (void)PerkFilter.Add(P); bDirty = true; return FReply::Handled(); })
				[
					SNew(SBorder).BorderImage(&St().None).Padding(0.f)
					.ColorAndOpacity_Lambda([On]() { return On() ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.5f); })
					[
						SNew(SBox).WidthOverride(26.f).HeightOverride(26.f)[SNew(SImage).Image(PD ? Tex(PD->Icon, 26.f) : nullptr)]
					]
				]
			]
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.Visibility(EVisibility::HitTestInvisible)
				.BorderImage_Lambda([On]()
				{
					static const FSlateRoundedBoxBrush OnB = Round(FLinearColor::Transparent, Gold, 1.f);
					static const FSlateNoResource Off;
					return On() ? static_cast<const FSlateBrush*>(&OnB) : static_cast<const FSlateBrush*>(&Off);
				})
			]
		];
	}
	return Box;
}

TSharedRef<SWidget> SNWMarket::BuildList(EPage P)
{
	const int32 i = int32(P);
	auto HeaderText = [](const FString& S, EHorizontalAlignment Align = HAlign_Left)
	{
		return SNew(SBox).HAlign(Align).Padding(FMargin(8.f, 4.f))[Txt(S, 1, 12, Dim)];
	};
	auto OnSort = [this, i](EColumnSortPriority::Type, const FName& Col, EColumnSortMode::Type Mode)
	{
		SortCol[i] = Col;
		SortMode[i] = Mode;
		Sort(EPage(i));
		if (Lists[i].IsValid())
		{
			Lists[i]->RequestListRefresh();
		}
	};
	auto Mode = [this, i](FName Col) { return TAttribute<EColumnSortMode::Type>::CreateLambda([this, i, Col]() { return SortCol[i] == Col ? SortMode[i] : EColumnSortMode::None; }); };
	TSharedRef<SHeaderRow> H = SNew(SHeaderRow).Style(&St().Header);
	auto Add = [&](FName Col, const FString& Label, float Width, EHorizontalAlignment Align, bool bSort = true)
	{
		SHeaderRow::FColumn::FArguments A;
		A.ColumnId(Col).HAlignHeader(Align).HAlignCell(Align).HeaderContent()[HeaderText(Label, Align)];
		if (Width > 0.f)
		{
			A.FixedWidth(Width);
		}
		else
		{
			A.FillWidth(1.f);
		}
		if (bSort)
		{
			A.SortMode(Mode(Col)).OnSort_Lambda(OnSort);
		}
		H->AddColumn(A);
	};
	Add(ColName, NWText::Get(TEXT("trade_column_name")), 0.f, HAlign_Left);
	Add(ColTier, NWText::Get(TEXT("trade_column_tier")), 70.f, HAlign_Center);
	Add(ColGs, TEXT("GS"), 70.f, HAlign_Right);
	Add(ColQuality, NWText::Get(TEXT("ui_quality")), 100.f, HAlign_Right);
	Add(ColPerks, NWText::Get(TEXT("ui_perks")), 120.f, HAlign_Left, false);
	if (P == EPage::Buy)
	{
		Add(ColSeller, NWText::Get(TEXT("nw.market.col_seller")), 150.f, HAlign_Left);
	}
	Add(ColQty, NWText::Get(TEXT("trade_column_quantity")), 100.f, HAlign_Right);
	if (P == EPage::Sell)
	{
		Add(ColLowest, NWText::Get(TEXT("nw.market.col_lowest")), 140.f, HAlign_Right);
	}
	else
	{
		Add(ColPrice, NWText::Get(TEXT("trade_column_price")), 130.f, HAlign_Right);
	}
	if (P == EPage::Orders)
	{
		Add(ColExpires, NWText::Get(TEXT("trade_column_expiresin")), 130.f, HAlign_Right);
		Add(ColAction, FString(), 170.f, HAlign_Center, false);
	}

	static const TCHAR* EmptyKeys[3] = {TEXT("nw.market.empty"), TEXT("nw.market.nothing_to_sell"), TEXT("nw.market.no_orders")};
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SAssignNew(Lists[i], SListView<FRowPtr>)
			.ListItemsSource(&Rows[i])
			.ListViewStyle(&St().List)
			.ScrollBarStyle(&St().Scroll)
			.HeaderRow(H)
			.SelectionMode(P == EPage::Orders ? ESelectionMode::None : ESelectionMode::Single)
			.OnGenerateRow_Lambda([this, P](FRowPtr R, const TSharedRef<STableViewBase>& Owner) { return OnRow(R, Owner, P); })
			.OnSelectionChanged_Lambda([this, i](FRowPtr R, ESelectInfo::Type)
			{
				const FString Key = R.IsValid() ? R->Key : FString();
				if (Key != Selected[i])
				{
					Selected[i] = Key;
					SelectedAt = Clock();
					Count = 1;
					if (i == int32(EPage::Sell) && R.IsValid())
					{
						Price = R->Lowest;   // suggestion: the lowest listed price of the same item
					}
					RefreshPanel(EPage(i));
				}
			})
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 90.f, 0.f, 0.f)
		[
			SNew(SBox)
			.Visibility_Lambda([this, i]() { return Rows[i].Num() == 0 && !Loading() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				Txt(NWText::Get(EmptyKeys[i]), 0, 16, Dim)
			]
		];
}

TSharedRef<ITableRow> SNWMarket::OnRow(FRowPtr R, const TSharedRef<STableViewBase>& Owner, EPage P)
{
	auto Cell = [this, R, P](const FName& Col) -> TSharedRef<SWidget>
	{
		const FNWItem& I = R->Item;
		const FNWItemDef* D = Def(I);
		if (Col == ColName)
		{
			return SNew(SHorizontalBox)
				.ToolTip(ItemTip(I))
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[ItemIcon(I, 46.f)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(Font(1, 15)).Text(FText::FromString(NameOf(I))).ColorAndOpacity(I.Rarity > 0 ? NWRarityColor(I.Rarity) : Tan)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				];
		}
		if (Col == ColTier)
		{
			static const TCHAR* Roman[] = {TEXT("—"), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V")};
			return Txt(D && D->Tier >= 1 && D->Tier <= 5 ? Roman[D->Tier] : TEXT("—"), 1, 14, Gray);
		}
		if (Col == ColGs)
		{
			return Txt(I.GearScore > 0 ? FString::FromInt(I.GearScore) : FString(TEXT("—")), 2, 15, I.GearScore > 0 ? Tan : Dim);
		}
		if (Col == ColQuality)
		{
			return Txt(I.Quality >= 0.f ? Percent(I.Quality) : FString(TEXT("—")), 0, 14, I.Quality >= 0.f ? Gold : Dim);
		}
		if (Col == ColPerks)
		{
			return PerkIcons(I, 24.f);
		}
		if (Col == ColSeller)
		{
			return SNew(STextBlock).Font(Font(0, 14)).ColorAndOpacity(Gray).Text(FText::FromString(R->Order.Seller)).OverflowPolicy(ETextOverflowPolicy::Ellipsis);
		}
		if (Col == ColQty)
		{
			return Txt(FText::AsNumber(I.Quantity), 0, 14, Gray);
		}
		if (Col == ColPrice)
		{
			return Coins(R->Order.UnitPrice, 2, 15, Gold);
		}
		if (Col == ColLowest)
		{
			return R->Lowest > 0 ? Coins(R->Lowest, 0, 14, Gold) : StaticCastSharedRef<SWidget>(Txt(TEXT("—"), 0, 14, Dim));
		}
		if (Col == ColExpires)
		{
			return Txt(Remaining(R->Order.ExpiresIn), 0, 14, Gray);
		}
		if (Col == ColAction)
		{
			return SNew(SOverlay)
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).Visibility_Lambda([R]() { return R->bPending || R->bCancelling ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						Spinner(28.f)
					]
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(&St().Button)
					.Visibility_Lambda([R]() { return R->bPending || R->bCancelling ? EVisibility::Collapsed : EVisibility::Visible; })
					.OnClicked_Lambda([this, R]() { Cancel(R); return FReply::Handled(); })
					[
						Txt(NWText::Get(TEXT("ui_cancel")), 1, 13, Tan)
					]
				];
		}
		return SNullWidget::NullWidget;
	};
	return SNew(SNWMarketRow, Owner).Row(R).Market(this).Cell(Cell);
}

TSharedRef<SWidget> SNWMarket::ItemHeader(const FNWItem& I)
{
	const FNWItemDef* D = Def(I);
	TSharedRef<SVerticalBox> Right = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Font(Font(2, 18)).Text(FText::FromString(NameOf(I))).ColorAndOpacity(I.Rarity > 0 ? NWRarityColor(I.Rarity) : Tan)
			.AutoWrapText(true)
		];
	FString Sub;
	if (I.GearScore > 0)
	{
		Sub = FString::Printf(TEXT("GS %d"), I.GearScore);
	}
	if (D && D->Tier > 0)
	{
		Sub += (Sub.IsEmpty() ? TEXT("") : TEXT("   ")) + FString::Printf(TEXT("%s %d"), *NWText::Get(TEXT("trade_column_tier")), D->Tier);
	}
	Right->AddSlot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)[Txt(Sub, 1, 14, Gold)];
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 16.f, 0.f)[ItemIcon(I, 84.f)]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Right];
}

TSharedRef<SWidget> SNWMarket::ItemDetails(const FNWItem& I)
{
	// Attribute, quality, perks with the quality value, gem and crafter (like the inventory tooltip).
	const FNWItemRules& IR = FNWItemRules::Get();
	TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
	if (I.Attr != 255 && IR.Attributes.IsValidIndex(I.Attr) && I.GearScore > 0)
	{
		V->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			Txt(FString::Printf(TEXT("+%d  %s"), IR.AttrValue(I.GearScore), *NWText::Get(*(TEXT("ui_") + IR.Attributes[I.Attr].ToString()))), 1, 15, Tan)
		];
	}
	if (I.Quality >= 0.f)
	{
		V->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(NWText::Get(TEXT("ui_quality")), 0, 14, Gray)]
			+ SHorizontalBox::Slot().AutoWidth()[Txt(Percent(I.Quality), 2, 15, Gold)]
		];
		// Quality bar.
		V->AddSlot().AutoHeight().Padding(0.f, 4.f, 0.f, 6.f)
		[
			SNew(SBox).HeightOverride(4.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBorder).BorderImage(&St().White).BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.08f))]
				+ SOverlay::Slot().HAlign(HAlign_Left)
				[
					SNew(SBox).WidthOverride(FMath::Clamp(I.Quality, 0.f, 100.f) * 3.8f)[SNew(SBorder).BorderImage(&St().White).BorderBackgroundColor(Gold)]
				]
			]
		];
	}
	for (FName P : I.Perks)
	{
		const FNWItemRules::FPerk* PD = IR.Perks.Find(P);
		V->AddSlot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(SBox).WidthOverride(30.f).HeightOverride(30.f)[SNew(SImage).Image(PD ? Tex(PD->Icon, 30.f) : nullptr)]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Txt(IR.PerkText(P, I.Quality, true), 1, 14, Tan)]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font(0, 13)).ColorAndOpacity(Gray).AutoWrapText(true).Text(FText::FromString(IR.PerkText(P, I.Quality, false)))
				]
			]
		];
	}
	if (const FNWItemDef* G = I.Gem.IsNone() ? nullptr : FNWItemCatalog::Get().Find(I.Gem))
	{
		V->AddSlot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)[SNew(SBox).WidthOverride(30.f).HeightOverride(30.f)[SNew(SImage).Image(Tex(G->Icon, 30.f))]]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Txt(G->Name, 1, 14, Tan)]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font(0, 13)).ColorAndOpacity(Gray).AutoWrapText(true)
					.Text(FText::FromString(FString::Join(IR.GemLines(I.Gem), TEXT("\n"))))
				]
			]
		];
	}
	if (IR.Gems.Contains(I.ItemId))
	{
		V->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(STextBlock).Font(Font(0, 14)).ColorAndOpacity(Tan).AutoWrapText(true).Text(FText::FromString(FString::Join(IR.GemLines(I.ItemId), TEXT("\n"))))
		];
	}
	if (!I.Signer.IsEmpty())
	{
		V->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[Txt(NWText::Format(TEXT("nw.item.signed"), {I.Signer}), 6, 13, Gold)];
	}
	return V;
}

TSharedRef<SWidget> SNWMarket::Stepper(TFunction<int32()> Max)
{
	auto Btn = [this, Max](const FString& Label, int32 Step)
	{
		return SNew(SBox).WidthOverride(40.f).HeightOverride(36.f)
			[
				SNew(SButton)
				.ButtonStyle(&St().Button)
				.ContentPadding(FMargin(0.f))
				.HAlign(HAlign_Center).VAlign(VAlign_Center)
				.IsEnabled_Lambda([this, Max, Step]() { return Step < 0 ? Count > 1 : Count < Max(); })
				.OnClicked_Lambda([this, Max, Step]() { Count = FMath::Clamp(Count + Step, 1, FMath::Max(1, Max())); return FReply::Handled(); })
				[
					Txt(Label, 2, 15, Tan)
				]
			];
	};
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[Btn(TEXT("−"), -1)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(64.f).HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(Font(2, 16)).ColorAndOpacity(Tan).Text_Lambda([this]() { return FText::AsNumber(Count); })
			]
		]
		+ SHorizontalBox::Slot().AutoWidth()[Btn(TEXT("+"), 1)];
}

TSharedRef<SWidget> SNWMarket::BuildPanel(EPage P)
{
	const int32 i = int32(P);
	return SNew(SBox).WidthOverride(430.f)
		[
			SNew(SBorder)
			.BorderImage(&St().None)
			.Padding(0.f)
			.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.f, 1.f, 1.f, Ease(Clock() - SelectedAt, 0.2)); })
			.RenderTransform_Lambda([this]() -> TOptional<FSlateRenderTransform>
			{
				return FSlateRenderTransform(FVector2f((1.f - Ease(Clock() - SelectedAt, 0.2)) * 24.f, 0.f));
			})
			[
				SAssignNew(Panels[i], SBox)
			]
		];
}

void SNWMarket::RefreshPanel(EPage P)
{
	if (P == EPage::Orders || !Panels[int32(P)].IsValid())
	{
		return;
	}
	const int32 i = int32(P);
	FRowPtr R = Find(P, Selected[i]);
	if (!R.IsValid())
	{
		Panels[i]->SetContent(SNullWidget::NullWidget);
		if (P == EPage::Sell)
		{
			PriceBox.Reset();
		}
		return;
	}
	const FNWItem Item = R->Item;
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	Body->AddSlot().AutoHeight()[ItemHeader(Item)];
	Body->AddSlot().AutoHeight().Padding(0.f, 14.f)[SNew(SBox).HeightOverride(1.f)[SNew(SBorder).BorderImage(&St().Divider)]];
	Body->AddSlot().FillHeight(1.f)
	[
		SNew(SScrollBox).Style(&St().ScrollBox).ScrollBarStyle(&St().Scroll)
		+ SScrollBox::Slot()[ItemDetails(Item)]
	];
	auto LabelRow = [](const FString& Label, TSharedRef<SWidget> Right)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(Label, 0, 14, Gray)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Right];
	};
	TSharedRef<SVerticalBox> Act = SNew(SVerticalBox);
	const FString Key = R->Key;
	if (P == EPage::Buy)
	{
		Act->AddSlot().AutoHeight().Padding(0.f, 4.f)[LabelRow(NWText::Get(TEXT("nw.market.col_seller")), Txt(R->Order.Seller, 1, 14, Tan))];
		Act->AddSlot().AutoHeight().Padding(0.f, 4.f)[LabelRow(NWText::Get(TEXT("ui_listingunitprice")), Coins(R->Order.UnitPrice, 2, 15, Gold))];
		if (Item.Quantity > 1)
		{
			Act->AddSlot().AutoHeight().Padding(0.f, 6.f)
			[
				LabelRow(NWText::Get(TEXT("ui_quantity")), Stepper([this, Key]() { FRowPtr X = Find(EPage::Buy, Key); return X.IsValid() ? X->Item.Quantity : 1; }))
			];
		}
		Act->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)[SNew(SBox).HeightOverride(1.f)[SNew(SBorder).BorderImage(&St().Divider)]];
		Act->AddSlot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(NWText::Get(TEXT("ui_total")), 2, 17, Tan)]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Coins(TAttribute<int64>::CreateLambda([this, Key]() { FRowPtr X = Find(EPage::Buy, Key); return X.IsValid() ? X->Order.UnitPrice * Count : 0; }), 2, 18,
					TAttribute<FSlateColor>::CreateLambda([this, Key]()
					{
						FRowPtr X = Find(EPage::Buy, Key);
						return FSlateColor(X.IsValid() && ShownCoins() < X->Order.UnitPrice * Count ? Red : Gold);
					}))
			]
		];
		Act->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SBox).HeightOverride(52.f)
			[
				SNew(SButton)
				.ButtonStyle(&St().Primary)
				.HAlign(HAlign_Center).VAlign(VAlign_Center)
				.IsEnabled_Lambda([this, Key]() { FRowPtr X = Find(EPage::Buy, Key); return X.IsValid() && ShownCoins() >= X->Order.UnitPrice * Count; })
				.OnClicked_Lambda([this, Key]() { Buy(Find(EPage::Buy, Key)); return FReply::Handled(); })
				[
					Txt(NWText::Get(TEXT("ui_buy")), 2, 17, Tan)
				]
			]
		];
	}
	else
	{
		if (Item.Quantity > 1)
		{
			Act->AddSlot().AutoHeight().Padding(0.f, 6.f)
			[
				LabelRow(NWText::Get(TEXT("ui_quantity")), Stepper([this, Key]() { FRowPtr X = Find(EPage::Sell, Key); return X.IsValid() ? X->Item.Quantity : 1; }))
			];
		}
		Act->AddSlot().AutoHeight().Padding(0.f, 6.f)
		[
			LabelRow(NWText::Get(TEXT("ui_listingunitprice")),
				SNew(SBox).WidthOverride(170.f)
				[
					SAssignNew(PriceBox, SEditableTextBox)
					.Style(&St().Field)
					.Font(Font(2, 16))
					.Justification(ETextJustify::Right)
					.SelectAllTextWhenFocused(true)
					.Text(FText::FromString(MoneyField(Price)))
					.OnTextChanged_Lambda([this](const FText& T) { Price = ParseMoney(T.ToString()); })
					.OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type How)
					{
						Price = ParseMoney(T.ToString());
						if (How == ETextCommit::OnEnter)
						{
							PostSell();
						}
					})
				])
		];
		TSharedRef<SHorizontalBox> DaysBox = SNew(SHorizontalBox);
		for (int32 Dd : NWMarket::Durations)
		{
			DaysBox->AddSlot().AutoWidth().Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SButton)
					.ButtonStyle(&St().Chip)
					.OnClicked_Lambda([this, Dd]() { Days = Dd; return FReply::Handled(); })
					[
						SNew(STextBlock).Font(Font(1, 13)).Text(FText::FromString(NWText::Format(TEXT("nw.market.days"), {FString::FromInt(Dd)})))
						.ColorAndOpacity_Lambda([this, Dd]() { return FSlateColor(Days == Dd ? Tan : Dim); })
					]
				]
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.Visibility(EVisibility::HitTestInvisible)
					.BorderImage_Lambda([this, Dd]()
					{
						static const FSlateRoundedBoxBrush OnB = Round(FLinearColor::Transparent, Gold, 1.f);
						static const FSlateNoResource Off;
						return Days == Dd ? static_cast<const FSlateBrush*>(&OnB) : static_cast<const FSlateBrush*>(&Off);
					})
				]
			];
		}
		Act->AddSlot().AutoHeight().Padding(0.f, 6.f)[LabelRow(NWText::Get(TEXT("nw.market.duration")), DaysBox)];
		Act->AddSlot().AutoHeight().Padding(0.f, 4.f)
		[
			LabelRow(NWText::Get(TEXT("nw.market.col_lowest")),
				R->Lowest > 0 ? Coins(R->Lowest, 1, 14, Gold) : StaticCastSharedRef<SWidget>(Txt(TEXT("—"), 1, 14, Dim)))
		];
		Act->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)[SNew(SBox).HeightOverride(1.f)[SNew(SBorder).BorderImage(&St().Divider)]];
		Act->AddSlot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(NWText::Get(TEXT("ui_total")), 2, 17, Tan)]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Coins(TAttribute<int64>::CreateLambda([this]() { return Price * Count; }), 2, 18, Gold)
			]
		];
		Act->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SBox).HeightOverride(52.f)
			[
				SNew(SButton)
				.ButtonStyle(&St().Primary)
				.HAlign(HAlign_Center).VAlign(VAlign_Center)
				.IsEnabled_Lambda([this]()
				{
					const int64 Max = NWCity::Rules().ItemPriceMax;
					return Price > 0 && (Max <= 0 || Price <= Max) && MyOrders() < FNWItemRules::Get().MarketMaxOrders;
				})
				.OnClicked_Lambda([this]() { PostSell(); return FReply::Handled(); })
				[
					Txt(NWText::Get(TEXT("ui_placesellorder")), 2, 17, Tan)
				]
			]
		];
	}
	Body->AddSlot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)[Act];
	Panels[i]->SetContent(
		SNew(SBorder).BorderImage(&St().Panel).Padding(FMargin(22.f, 20.f))
		[
			Body
		]);
	if (P == EPage::Sell && PriceBox.IsValid() && !bShot && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(PriceBox, EFocusCause::SetDirectly);
	}
}

// ---------------------------------------------------------------- state

double SNWMarket::Clock() const
{
	return bShot ? ShotNow : FPlatformTime::Seconds();
}

double SNWMarket::FlashOf(const FString& Key) const
{
	const double* F = Flash.Find(Key);
	return F ? *F : -100.0;
}

bool SNWMarket::Loading() const
{
	const ANWCharacter* Me = Character.Get();
	return !bShot && Me && Me->Interactor && Me->Interactor->MarketSeq == OpenSeq;
}

bool SNWMarket::AnyWaiting() const
{
	return Pending.ContainsByPredicate([](const FPending& P) { return !P.bDone; });
}

SNWMarket::FRowPtr SNWMarket::Find(EPage P, const FString& Key) const
{
	const FRowPtr* R = Rows[int32(P)].FindByPredicate([&Key](const FRowPtr& X) { return X->Key == Key; });
	return R ? *R : FRowPtr();
}

SNWMarket::FRowPtr SNWMarket::Row(const FString& Key, double Now)
{
	FRowPtr& R = Known.FindOrAdd(Key);
	if (!R.IsValid())
	{
		R = MakeShared<FRow>();
		R->Key = Key;
		R->Born = bShot ? Now - 10.0 : Now;
	}
	return R;
}

int64 SNWMarket::ShownCoins() const
{
	const ANWCharacter* Me = Character.Get();
	int64 C = Me && Me->Inventory ? Me->Inventory->Coins : 0;
	for (const FPending& P : Pending)
	{
		if (P.Kind == EKind::Buy && (!P.bDone || C == P.CoinsBefore))
		{
			C -= P.Coins;
		}
	}
	return C;
}

int32 SNWMarket::MyOrders() const
{
	return Rows[int32(EPage::Orders)].Num();
}

FNWItem SNWMarket::ShownBag(const FNWItem& I) const
{
	FNWItem Out = I;
	for (const FPending& P : Pending)
	{
		if (P.Kind == EKind::Post && P.Slot == I.Slot && I.ItemId == P.SlotBefore.ItemId && I.Quantity == P.SlotBefore.Quantity)
		{
			Out.Quantity -= P.Qty;
		}
	}
	return Out;
}

bool SNWMarket::Pass(const FNWMarketRow& O) const
{
	const FNWItem& I = O.Item;
	const FNWItemDef* D = Def(I);
	if (!InCat(int32(Cat), I.ItemId))
	{
		return false;
	}
	if (Tiers.Num() && !(D && Tiers.Contains(D->Tier)))
	{
		return false;
	}
	if (Rarities.Num() && !Rarities.Contains(int32(I.Rarity)))
	{
		return false;
	}
	if (MinGs > 0 && I.GearScore < MinGs)
	{
		return false;
	}
	if (Attrs.Num() && !Attrs.Contains(I.Attr))
	{
		return false;
	}
	for (FName P : PerkFilter)
	{
		if (!I.Perks.Contains(P))
		{
			return false;
		}
	}
	if (bAfford && O.UnitPrice > ShownCoins())
	{
		return false;
	}
	return true;
}

void SNWMarket::Sort(EPage P)
{
	const int32 i = int32(P);
	const FName Col = SortCol[i];
	const bool bDesc = SortMode[i] == EColumnSortMode::Descending;
	Rows[i].StableSort([Col, bDesc](const FRowPtr& A, const FRowPtr& B)
	{
		auto Key = [&Col](const FRowPtr& R) -> double
		{
			const FNWItemDef* D = Def(R->Item);
			if (Col == ColTier) return D ? D->Tier : 0;
			if (Col == ColGs) return R->Item.GearScore;
			if (Col == ColQuality) return R->Item.Quality;
			if (Col == ColQty) return R->Item.Quantity;
			if (Col == ColPrice) return double(R->Order.UnitPrice);
			if (Col == ColLowest) return double(R->Lowest);
			if (Col == ColExpires) return R->Order.ExpiresIn;
			return 0.0;
		};
		if (Col == ColName || Col == ColSeller)
		{
			const FString SA = Col == ColName ? NameOf(A->Item) : A->Order.Seller;
			const FString SB = Col == ColName ? NameOf(B->Item) : B->Order.Seller;
			return bDesc ? SB < SA : SA < SB;
		}
		const double KA = Key(A), KB = Key(B);
		return bDesc ? KB < KA : KA < KB;
	});
}

void SNWMarket::Consume()
{
	ANWCharacter* Me = Character.Get();
	if (!Me || !Me->Interactor)
	{
		return;
	}
	const double Now = Clock();
	for (const TPair<int32, bool>& D : Me->Interactor->MarketDone)
	{
		const int32 i = Pending.IndexOfByPredicate([&D](const FPending& P) { return P.Req == D.Key; });
		if (i == INDEX_NONE)
		{
			continue;
		}
		if (D.Value)
		{
			Pending[i].bDone = true;
			Pending[i].DoneAt = Now;
		}
		else
		{
			// Refused: goes back to what it was and the row flashes.
			const FPending& P = Pending[i];
			Flash.Add(P.Kind == EKind::Post ? FString::Printf(TEXT("bag:%d"), P.Slot) : P.OrderId, Now);
			Pending.RemoveAt(i);
		}
		bDirty = true;
	}
	Me->Interactor->MarketDone.Reset();
	// Prediction no longer needed: a new book arrived (it comes before the end) and the bag/coins already changed, or time ran out.
	const int32 Before = Pending.Num();
	Pending.RemoveAll([Now, Me](const FPending& P)
	{
		if (!P.bDone)
		{
			return Now - P.At > GiveUp;
		}
		if (Now - P.DoneAt > KeepAfterDone)
		{
			return true;
		}
		if (P.Kind == EKind::Post)
		{
			const FNWItem* I = Me->Inventory->At(ENWItemContainer::Bag, P.Slot);
			return !I || I->ItemId != P.SlotBefore.ItemId || I->Quantity != P.SlotBefore.Quantity;
		}
		if (P.Kind == EKind::Buy)
		{
			return Me->Inventory->Coins != P.CoinsBefore;
		}
		return true;
	});
	bDirty |= Pending.Num() != Before;
}

void SNWMarket::Rebuild()
{
	ANWCharacter* Me = Character.Get();
	if (!Me || !Me->Interactor || !Me->Inventory)
	{
		return;
	}
	bDirty = false;
	const double Now = Clock();
	// Book with the prediction: a new order goes in, a purchase removes quantity, a cancellation marks the row.
	TArray<FNWMarketRow> Book = Me->Interactor->MarketRows;
	for (const FPending& P : Pending)
	{
		if (P.bDone)
		{
			continue;
		}
		if (P.Kind == EKind::Post)
		{
			Book.Add(P.Row);
		}
		else if (P.Kind == EKind::Buy)
		{
			if (FNWMarketRow* R = Book.FindByPredicate([&P](const FNWMarketRow& X) { return X.Id == P.OrderId; }))
			{
				R->Item.Quantity -= P.Qty;
			}
		}
	}
	Book.RemoveAll([](const FNWMarketRow& R) { return R.Item.Quantity <= 0; });

	TMap<FName, int64> Low;
	TSet<FName> PerksSeen;
	for (const FNWMarketRow& O : Book)
	{
		if (O.bSell && !O.bMine)
		{
			int64& L = Low.FindOrAdd(O.Item.ItemId);
			L = L == 0 ? O.UnitPrice : FMath::Min(L, O.UnitPrice);
			PerksSeen.Append(O.Item.Perks);
		}
	}
	const FString Q = Search.TrimStartAndEnd();
	auto Match = [&Q](const FNWItem& I)
	{
		if (Q.IsEmpty() || NameOf(I).Contains(Q))
		{
			return true;
		}
		for (FName P : I.Perks)
		{
			if (FNWItemRules::Get().PerkText(P, I.Quality, true).Contains(Q))
			{
				return true;
			}
		}
		return false;
	};

	for (TArray<FRowPtr>& L : Rows)
	{
		L.Reset();
	}
	TSet<FString> Alive;
	for (const FNWMarketRow& O : Book)
	{
		FRowPtr R = Row(O.Id, Now);
		R->Order = O;
		R->Item = O.Item;
		R->bPending = O.Id.StartsWith(TEXT("pending:"));
		R->bCancelling = Pending.ContainsByPredicate([&O](const FPending& P) { return !P.bDone && P.Kind == EKind::Cancel && P.OrderId == O.Id; });
		Alive.Add(O.Id);
		if (O.bMine)
		{
			Rows[int32(EPage::Orders)].Add(R);
		}
		else if (O.bSell && Pass(O) && Match(O.Item))
		{
			Rows[int32(EPage::Buy)].Add(R);
		}
	}
	for (const FNWItem& I : Me->Inventory->Items)
	{
		const FNWItemDef* D = Def(I);
		if (I.Container != ENWItemContainer::Bag || !D || !FNWItemRules::Get().CanTrade(*D, I))
		{
			continue;
		}
		const FNWItem Shown = ShownBag(I);
		if (Shown.Quantity <= 0 || !Match(Shown))
		{
			continue;
		}
		const FString Key = FString::Printf(TEXT("bag:%d"), I.Slot);
		FRowPtr R = Row(Key, Now);
		R->Item = Shown;
		R->Lowest = Low.FindRef(I.ItemId);
		Alive.Add(Key);
		Rows[int32(EPage::Sell)].Add(R);
	}
	for (auto It = Known.CreateIterator(); It; ++It)
	{
		if (!Alive.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
	for (int32 i = 0; i < 3; ++i)
	{
		Sort(EPage(i));
		if (Lists[i].IsValid())
		{
			Lists[i]->RequestListRefresh();
			FRowPtr Sel = Find(EPage(i), Selected[i]);
			if (Sel.IsValid() && !Lists[i]->IsItemSelected(Sel))
			{
				Lists[i]->SetSelection(Sel, ESelectInfo::Direct);
			}
			else if (!Sel.IsValid() && !Selected[i].IsEmpty())
			{
				Selected[i].Reset();
				Lists[i]->ClearSelection();
				RefreshPanel(EPage(i));
			}
		}
	}
	// Perk chips: only when the set changes.
	TArray<FName> Perks = PerksSeen.Array();
	Perks.Sort([](FName A, FName B) { return A.LexicalLess(B); });
	uint32 Sig = 0;
	for (FName P : Perks)
	{
		Sig = HashCombine(Sig, GetTypeHash(P));
	}
	if (Sig != BookPerksSig || !PerkChips.IsValid() || BookPerks.Num() != Perks.Num())
	{
		BookPerksSig = Sig;
		BookPerks = Perks;
		if (PerkChips.IsValid())
		{
			PerkChips->SetContent(BuildPerkChips());
		}
	}
}

void SNWMarket::Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime)
{
	SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
	ANWCharacter* Me = Character.Get();
	if (!Me || !Me->Interactor || !Me->Inventory || bShot)
	{
		return;
	}
	Consume();
	const double Now = Clock();
	// Book refreshed every 10 s with the screen open.
	if (Now - LastRefresh > 10.0)
	{
		LastRefresh = Now;
		Me->Interactor->ServerMarketRefresh();
	}
	if (SeenSeq != Me->Interactor->MarketSeq)
	{
		SeenSeq = Me->Interactor->MarketSeq;
		bDirty = true;
	}
	uint32 Bag = GetTypeHash(Me->Inventory->Coins);
	for (const FNWItem& I : Me->Inventory->Items)
	{
		Bag = HashCombine(Bag, HashCombine(GetTypeHash(I.ItemId), HashCombine(GetTypeHash(I.Quantity), GetTypeHash(I.Slot))));
	}
	if (Bag != SeenBag)
	{
		SeenBag = Bag;
		bDirty = true;
	}
	if (bDirty)
	{
		Rebuild();
	}
}

FReply SNWMarket::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Key)
{
	if (Key.GetKey() == EKeys::Escape)
	{
		Close();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(Geometry, Key);
}

void SNWMarket::SetPage(EPage P)
{
	if (Page == P)
	{
		return;
	}
	Page = P;
	PageAt = Clock();
	Count = 1;
	if (FSlateApplication::IsInitialized() && !bShot)
	{
		FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
	}
}

// ---------------------------------------------------------------- actions (prediction + request to the server)

void SNWMarket::Buy(FRowPtr R)
{
	ANWCharacter* Me = Character.Get();
	if (!R.IsValid() || !Me || !Me->Interactor || R->bPending)
	{
		return;
	}
	const int32 N = FMath::Clamp(Count, 1, R->Item.Quantity);
	const int64 Cost = R->Order.UnitPrice * N;
	if (ShownCoins() < Cost)
	{
		return;
	}
	FPending P;
	P.Req = NextReq++;
	P.Kind = EKind::Buy;
	P.OrderId = R->Order.Id;
	P.Qty = N;
	P.Coins = Cost;
	P.CoinsBefore = Me->Inventory->Coins;
	P.At = Clock();
	Pending.Add(P);
	Count = 1;
	Me->Interactor->ServerMarketBuy(P.Req, P.OrderId, N);
	Rebuild();
}

void SNWMarket::PostSell()
{
	ANWCharacter* Me = Character.Get();
	FRowPtr R = Find(EPage::Sell, Selected[int32(EPage::Sell)]);
	if (!R.IsValid() || !Me || !Me->Interactor || Price <= 0 || MyOrders() >= FNWItemRules::Get().MarketMaxOrders)
	{
		return;
	}
	const int32 Slot = R->Item.Slot;
	const FNWItem* Raw = Me->Inventory->At(ENWItemContainer::Bag, Slot);
	if (!Raw)
	{
		return;
	}
	const int32 N = FMath::Clamp(Count, 1, R->Item.Quantity);
	FPending P;
	P.Req = NextReq++;
	P.Kind = EKind::Post;
	P.Slot = Slot;
	P.SlotBefore = *Raw;
	P.Qty = N;
	P.At = Clock();
	P.Row.Id = FString::Printf(TEXT("pending:%d"), P.Req);
	P.Row.bSell = true;
	P.Row.bMine = true;
	P.Row.Item = *Raw;
	P.Row.Item.Quantity = N;
	P.Row.UnitPrice = Price;
	P.Row.ExpiresIn = Days * 86400.f;
	Pending.Add(P);
	Me->Interactor->ServerMarketPostSell(P.Req, Slot, N, Price, Days);
	Count = 1;
	Rebuild();
}

void SNWMarket::Cancel(FRowPtr R)
{
	ANWCharacter* Me = Character.Get();
	if (!R.IsValid() || !Me || !Me->Interactor || R->bPending || R->bCancelling)
	{
		return;
	}
	FPending P;
	P.Req = NextReq++;
	P.Kind = EKind::Cancel;
	P.OrderId = R->Order.Id;
	P.At = Clock();
	Pending.Add(P);
	Me->Interactor->ServerMarketCancel(P.Req, P.OrderId);
	Rebuild();
}

// ---------------------------------------------------------------- offline capture

void SNWMarket::ShotTab(int32 Tab)
{
	Page = EPage(FMath::Clamp(Tab, 0, 2));
	PageAt = Clock() - 10.0;
}

void SNWMarket::ShotSelect(int32 Index)
{
	const int32 i = int32(Page);
	Rebuild();
	if (Rows[i].IsValidIndex(Index) && Lists[i].IsValid())
	{
		Lists[i]->SetSelection(Rows[i][Index], ESelectInfo::Direct);
		Selected[i] = Rows[i][Index]->Key;
		SelectedAt = Clock() - 10.0;
		if (Page == EPage::Sell)
		{
			Price = Rows[i][Index]->Lowest;
		}
		RefreshPanel(Page);
	}
}

void SNWMarket::ShotSearch(const FString& Query)
{
	Search = Query;
	Rebuild();
}

void SNWMarket::ShotPerkFilter(FName Perk)
{
	PerkFilter.Add(Perk);
	Rebuild();
}

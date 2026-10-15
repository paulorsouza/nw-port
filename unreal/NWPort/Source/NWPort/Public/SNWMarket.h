#pragma once

#include "CoreMinimal.h"
#include "NWItems.h"
#include "NWMarket.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class ANWCharacter;
class SBox;
class SEditableTextBox;
class SToolTip;
class SWidgetSwitcher;

/**
 * Trading Post (area 4): the port's first own screen, in Slate, with the game's colors and fonts
 * (docs/game/city.md §5). Tabs Buy (other players' sell orders, filters and sortable table), Sell (bag items
 * that can go to the market) and My Orders. Buy orders stay off the screen (maintainer 2026-10-14); the server still accepts them.
 *
 * Optimistic response: buying, selling and cancelling change the screen right away (new row with spinner, row disappears,
 * coins drop) and the server confirms later (UNWInteractorComponent::ClientMarketDone). If refused, it goes back to what it was
 * and the row flashes red. The prediction holds until the server's book and bag arrive.
 */
class NWPORT_API SNWMarket : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNWMarket) : _Character(nullptr), _bShot(false) {}
		SLATE_ARGUMENT(ANWCharacter*, Character)
		SLATE_ARGUMENT(FString, Title)
		/** Offline capture (NWUIShotCommandlet): no server, rows already settled. */
		SLATE_ARGUMENT(bool, bShot)
	SLATE_END_ARGS()

	/** Table row: order from the book (Buy, My Orders) or bag item (Sell). */
	struct FRow
	{
		FString Key;            // order id, "pending:<n>" or "bag:<slot>"
		FNWMarketRow Order;
		FNWItem Item;
		int64 Lowest = 0;       // Sell: lowest listed price of the same item
		bool bPending = false;  // order created waiting for the server
		bool bCancelling = false;
		double Born = 0.0;
	};
	using FRowPtr = TSharedPtr<FRow>;

	void Construct(const FArguments& Args);
	virtual ~SNWMarket() override;
	virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Key) override;

	// Offline capture.
	void ShotTab(int32 Tab);
	void ShotSelect(int32 Index);
	void ShotSearch(const FString& Query);
	void ShotPerkFilter(FName Perk);

	double Clock() const;
	double FlashOf(const FString& Key) const;

private:
	enum class EPage : uint8 { Buy, Sell, Orders };
	enum class ECat : uint8 { All, Weapons, Armor, Jewelry, Tools, Gems, Materials, Num };
	enum class EKind : uint8 { Buy, Post, Cancel };
	struct FPending
	{
		int32 Req = 0;
		EKind Kind = EKind::Buy;
		FString OrderId;
		int32 Slot = INDEX_NONE;
		FNWItem SlotBefore;
		FNWMarketRow Row;
		int32 Qty = 0;
		int64 Coins = 0;
		int64 CoinsBefore = 0;
		double At = 0.0;
		bool bDone = false;
		double DoneAt = 0.0;
	};

	TWeakObjectPtr<ANWCharacter> Character;
	FString Title;
	bool bShot = false;
	double ShotNow = 1000.0;
	EPage Page = EPage::Buy;
	double PageAt = 0.0;
	int32 OpenSeq = 0;
	int32 SeenSeq = -1;
	uint32 SeenBag = 0;
	double LastRefresh = 0.0;
	bool bDirty = true;

	// Filters (Buy) and search (Buy and Sell).
	FString Search;
	ECat Cat = ECat::All;
	TSet<int32> Tiers;
	TSet<int32> Rarities;
	int32 MinGs = 0;
	TSet<uint8> Attrs;
	TSet<FName> PerkFilter;
	bool bAfford = false;
	TArray<FName> BookPerks;        // perks that appear in the orders for sale
	uint32 BookPerksSig = 0;

	// Sorting per page.
	FName SortCol[3];
	EColumnSortMode::Type SortMode[3] = {EColumnSortMode::Ascending, EColumnSortMode::Ascending, EColumnSortMode::Ascending};

	// Rows (the same FRow per key across rebuilds: the table keeps the selection and does not re-animate).
	TArray<FRowPtr> Rows[3];
	TMap<FString, FRowPtr> Known;
	TSharedPtr<SListView<FRowPtr>> Lists[3];
	FString Selected[3];
	double SelectedAt = 0.0;

	// Item panel on the right (Buy and Sell).
	TSharedPtr<SBox> Panels[2];
	TSharedPtr<SEditableTextBox> PriceBox;
	TSharedPtr<SBox> PerkChips;
	int32 Count = 1;
	int64 Price = 0;
	int32 Days = 3;

	TArray<FPending> Pending;
	int32 NextReq = 1;
	TMap<FString, double> Flash;

	// Construction.
	TSharedRef<SWidget> BuildTabs();
	TSharedRef<SWidget> BuildFilters();
	TSharedRef<SWidget> BuildList(EPage P);
	TSharedRef<SWidget> BuildSearch();
	TSharedRef<SWidget> BuildPerkChips();
	TSharedRef<SWidget> BuildPanel(EPage P);
	TSharedRef<SWidget> ItemHeader(const FNWItem& Item);
	TSharedRef<SWidget> ItemDetails(const FNWItem& Item);
	TSharedRef<SWidget> Stepper(TFunction<int32()> Max);
	TSharedRef<SWidget> Spinner(float Size);
	TSharedRef<SToolTip> ItemTip(const FNWItem& Item);
	void Close();
	TSharedRef<ITableRow> OnRow(FRowPtr Row, const TSharedRef<STableViewBase>& Owner, EPage P);

	// State.
	void SetPage(EPage P);
	void Consume();
	void Rebuild();
	void RefreshPanel(EPage P);
	bool Loading() const;
	FRowPtr Find(EPage P, const FString& Key) const;
	FRowPtr Row(const FString& Key, double Now);
	bool Pass(const FNWMarketRow& O) const;
	void Sort(EPage P);
	int64 ShownCoins() const;
	int32 MyOrders() const;
	FNWItem ShownBag(const FNWItem& I) const;
	bool AnyWaiting() const;

	// Actions.
	void Buy(FRowPtr R);
	void PostSell();
	void Cancel(FRowPtr R);
};

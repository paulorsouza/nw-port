#pragma once

#include "CoreMinimal.h"
#include "NWCity.h"
#include "Widgets/SCompoundWidget.h"

class ANWCharacter;
class SBox;
class SEditableTextBox;

/**
 * City screen (governance, docs/game/economy.md), in the style of the new screens (NWSlate). Two city boards:
 * - Town Project Board (bGovernance false): sell raw materials to the city, buy from the list, view the city.
 * - Territory Planning Board (bGovernance): weekly summary, prices and tax, tables and buffs, defense and
 *   government (buy or sell the city). Non-governors see the summary and can buy the city.
 * Selling and buying change the screen immediately (bag, coins, limit) and wait for the server with a spinner; if rejected, it reverts.
 */
class NWPORT_API SNWCity : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNWCity) : _Character(nullptr), _bGovernance(false), _bShot(false) {}
		SLATE_ARGUMENT(ANWCharacter*, Character)
		SLATE_ARGUMENT(FString, Title)
		SLATE_ARGUMENT(bool, bGovernance)
		SLATE_ARGUMENT(bool, bShot)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	virtual ~SNWCity() override;
	virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Key) override;

	// Offline capture.
	void ShotTab(int32 Tab);
	void ShotSelect(FName Item);

private:
	struct FPending
	{
		int32 Req = 0;
		FName Item;
		int32 Qty = 0;        // + sold to the city, − bought
		int64 Coins = 0;      // coins that come in (+) or go out (−) of the bag
		FString Busy;         // key of the button/row that shows the spinner
		double At = 0.0;
		bool bDone = false;
	};

	TWeakObjectPtr<ANWCharacter> Character;
	FString Title;
	bool bGovernance = false;
	bool bShot = false;
	int32 Tab = 0;
	double TabAt = 0.0;
	int32 SeenSeq = -1;
	int32 OpenSeq = 0;
	double LastRefresh = 0.0;
	TArray<FPending> Pending;
	int32 NextReq = 1;
	TMap<FString, double> Flash;
	FName Selected;
	int32 Count = 1;
	// Prices being edited (governor): item -> cents; tax in %.
	TMap<FName, int64> EditBuy, EditSell;
	float EditTax = -1.f;
	int64 EditSalePrice = 0;
	int64 EditBid = 0;

	TSharedPtr<SBox> Body;
	TSharedPtr<SBox> Panel;
	TSharedPtr<SBox> TradeHolder;
	uint32 StateSig = 0;
	bool bWasMine = false;

	const FNWCityState& State() const;
	double Clock() const;
	bool Loading() const;
	void Consume();
	void Rebuild();
	TArray<FString> TabNames() const;
	TSharedRef<SWidget> BuildTabs();
	TSharedRef<SWidget> BuildSellRaw();
	TSharedRef<SWidget> BuildBuyList();
	TSharedRef<SWidget> BuildInfo();
	TSharedRef<SWidget> BuildSummary();
	TSharedRef<SWidget> BuildPrices();
	TSharedRef<SWidget> BuildStations();
	TSharedRef<SWidget> BuildDefense();
	TSharedRef<SWidget> BuildGovernment();
	TSharedRef<SWidget> BuildTradePanel(bool bSellRaw);
	TSharedRef<SWidget> BuildTradeList(bool bSellRaw);
	const FNWCityPrice* PriceOf(FName Item, bool bSellRaw) const;
	int32 MaxCount(FName Item, bool bSellRaw) const;
	uint32 Signature() const;
	TSharedRef<SWidget> Spinner(float Size);
	TSharedRef<SWidget> ActionButton(const FString& Label, const FString& BusyKey, TFunction<bool()> Enabled, TFunction<void()> Do, bool bPrimary = true);
	TSharedRef<SWidget> PriceEditor(FName Item, const FNWCityPrice& P, bool bBuy);
	bool IsBusy(const FString& Key) const;
	int32 BagCount(FName Item) const;
	int64 ShownCoins() const;
	int32 ShownUsed(const FNWCityPrice& P, bool bSellRaw) const;
	FString StationName(const FString& Station) const;
	FString TierText(const FNWCityStation& S) const;
	/** Name of what the attack destroyed (table or weapon). */
	FString LostName(const FString& Id) const;
	TSharedRef<SWidget> Basket(const NWCity::FUp& Up, float Share, bool bCoins);
	bool CanPay(const NWCity::FUp& Up, float Share, bool bCoins) const;
	int32 Send(const FString& Busy, FName Item = NAME_None, int32 Qty = 0, int64 Coins = 0);
	void Close();
};

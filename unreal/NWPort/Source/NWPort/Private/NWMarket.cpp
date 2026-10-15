#include "NWMarket.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NWBackend.h"
#include "NWCity.h"
#include "NWCharacter.h"
#include "NWCrafting.h"
#include "NWInteractable.h"
#include "NWInventoryComponent.h"
#include "NWItemRules.h"
#include "NWOps.h"
#include "NWPlayerController.h"
#include "NWText.h"

DEFINE_LOG_CATEGORY_STATIC(LogNWMarket, Log, All);

namespace
{
	UNWBackend* MarketBackend(const UActorComponent* C)
	{
		const UGameInstance* GI = C && C->GetWorld() ? C->GetWorld()->GetGameInstance() : nullptr;
		UNWBackend* B = GI ? GI->GetSubsystem<UNWBackend>() : nullptr;
		return B && B->IsServerEnabled() ? B : nullptr;
	}

	ANWPlayerController* MarketPC(const UActorComponent* C)
	{
		const APawn* P = C ? Cast<APawn>(C->GetOwner()) : nullptr;
		return P ? Cast<ANWPlayerController>(P->GetController()) : nullptr;
	}

	void MarketWarn(const UActorComponent* C, const FString& Key)
	{
		if (ANWPlayerController* PC = MarketPC(C))
		{
			PC->Warning(NWText::Key(Key));
		}
	}

	/** Server order book: read from nw-core at most every CacheSeconds; whoever asks at the same time waits for the same read. */
	struct FBook
	{
		double FetchedAt = -1.0;
		bool bFetching = false;
		TArray<UNWBackend::FMarketOrder> Orders;
		TArray<TFunction<void()>> Waiting;
	};
	FBook Book;
	constexpr double CacheSeconds = 3.0;

	void WithBook(UNWBackend* B, bool bForce, TFunction<void()> Fn)
	{
		const double Now = FPlatformTime::Seconds();
		if (!bForce && Book.FetchedAt >= 0.0 && Now - Book.FetchedAt < CacheSeconds)
		{
			Fn();
			return;
		}
		Book.Waiting.Add(MoveTemp(Fn));
		if (Book.bFetching)
		{
			return;
		}
		Book.bFetching = true;
		B->ListOrders([](bool bOk, const TArray<UNWBackend::FMarketOrder>& Orders)
		{
			Book.bFetching = false;
			if (bOk)
			{
				Book.Orders = Orders;
				Book.FetchedAt = FPlatformTime::Seconds();
			}
			TArray<TFunction<void()>> Now = MoveTemp(Book.Waiting);
			Book.Waiting.Reset();
			for (TFunction<void()>& F : Now)
			{
				F();
			}
		});
	}

	const UNWBackend::FMarketOrder* FindOrder(const FString& Id)
	{
		return Book.Orders.FindByPredicate([&Id](const UNWBackend::FMarketOrder& O) { return O.Id == Id; });
	}

	FString CodeKey(const FString& Code)
	{
		return Code == TEXT("storage_full") ? FString(TEXT("ui_storage_is_full")) : TEXT("nw.market.") + (Code.IsEmpty() ? FString(TEXT("failed")) : Code);
	}
}

bool UNWInteractorComponent::NearTradingPost() const
{
	const AActor* Me = GetOwner();
	for (TActorIterator<ANWStation> It(GetWorld()); It && Me; ++It)
	{
		if (It->Type == TEXT("tradingpost") && FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation()) < 600.f)
		{
			return true;
		}
	}
	return false;
}

void UNWInteractorComponent::MarketSend(bool bForce)
{
	UNWBackend* B = MarketBackend(this);
	ANWPlayerController* PC = MarketPC(this);
	if (!B || !PC || !PC->Record)
	{
		ClientMarketOrders({});
		return;
	}
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	const FString Me = PC->Record->Id;
	WithBook(B, bForce, [Self, Me]()
	{
		if (!Self.IsValid() || !Self->bMarketOpen)
		{
			return;
		}
		TArray<FNWMarketRow> Rows;
		for (const UNWBackend::FMarketOrder& O : Book.Orders)
		{
			FNWMarketRow R;
			R.Id = O.Id;
			R.bSell = O.bSell;
			R.bMine = O.CharacterId == Me;
			R.Seller = O.Seller;
			R.Item = O.Item;
			R.UnitPrice = O.UnitPrice;
			R.ExpiresIn = float(O.ExpiresIn);
			Rows.Add(R);
		}
		Self->ClientMarketOrders(Rows);
	});
}

void UNWInteractorComponent::ClientMarketOrders_Implementation(const TArray<FNWMarketRow>& Rows)
{
	MarketRows = Rows;
	++MarketSeq;
}

void UNWInteractorComponent::ClientMarketDone_Implementation(int32 Req, bool bOk)
{
	MarketDone.Add({Req, bOk});
}

void UNWInteractorComponent::MarketFinish(int32 Req, bool bOk)
{
	// New rows before the end of the request (reliable RPCs from the same actor arrive in order): the screen swaps the
	// preview for the book without flickering.
	UNWBackend* B = MarketBackend(this);
	ANWPlayerController* PC = MarketPC(this);
	if (!B || !PC || !PC->Record || !bMarketOpen)
	{
		ClientMarketDone(Req, bOk);
		return;
	}
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	WithBook(B, true, [Self, Req, bOk]()
	{
		if (Self.IsValid())
		{
			Self->MarketSend(false);
			Self->ClientMarketDone(Req, bOk);
		}
	});
}

void UNWInteractorComponent::MarketClaimCredits()
{
	// Sale credits (sell order completed, buy order cancelled or expired): written operation, adds
	// exactly what there was up to the query.
	UNWBackend* B = MarketBackend(this);
	ANWPlayerController* PC = MarketPC(this);
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!B || !PC || !PC->Record || !Me)
	{
		return;
	}
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	B->PendingCredits(PC->Record->Id, [Self, Me](bool bOk, int64 Amount, const FString& Until)
	{
		if (!bOk || Amount <= 0 || !Self.IsValid())
		{
			return;
		}
		NWOps::Run(Me, TEXT("market_credit"),
			[Me, Amount]()
			{
				Me->Inventory->AddCoins(Amount);
				TWeakObjectPtr<UNWInventoryComponent> Inv(Me->Inventory);
				NWOps::Hold(Me, [Inv, Amount]() { if (Inv.IsValid()) { Inv->ClientLoot({}, Amount); } });
				return true;
			},
			[Self, Until, Amount](const FNWCharacterRecord& After, const FNWOpState&, TFunction<void(NWOps::EResult)> Fin)
			{
				UNWBackend* B2 = Self.IsValid() ? MarketBackend(Self.Get()) : nullptr;
				if (!B2)
				{
					Fin(NWOps::EResult::Failed);
					return;
				}
				B2->ClaimCredits(After, Until, [Fin, Amount](int32 Status, int64 Got)
				{
					if (Status == 200 && Got != Amount)
					{
						UE_LOG(LogNWMarket, Error, TEXT("credits: expected %lld, claimed %lld"), Amount, Got);
					}
					Fin(Status == 200 || Status == 208 ? NWOps::EResult::Ok : NWOps::EResult::Failed);
				});
			});
	});
}

NWOps::FCommit UNWInteractorComponent::MarketCommit(TFunction<void(const FNWCharacterRecord&, TFunction<void(int32, const FString&)>)> Call)
{
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	return [Self, Call](const FNWCharacterRecord& After, const FNWOpState&, TFunction<void(NWOps::EResult)> Fin)
	{
		Call(After, [Self, Fin](int32 Status, const FString& Code)
		{
			if (Status >= 200 && Status < 300)
			{
				Fin(NWOps::EResult::Ok);
				return;
			}
			if (Status == 400 || Status == 409)
			{
				MarketWarn(Self.Get(), CodeKey(Code));   // refused by nw-core: the reason, not the generic warning
				Fin(NWOps::EResult::Rejected);
				return;
			}
			Fin(NWOps::EResult::Failed);
		});
	};
}

void UNWInteractorComponent::ServerMarketOpen_Implementation()
{
	bMarketOpen = NearTradingPost();
	if (bMarketOpen)
	{
		MarketClaimCredits();
		MarketSend(true);
	}
}

void UNWInteractorComponent::ServerMarketClose_Implementation()
{
	bMarketOpen = false;
}

void UNWInteractorComponent::ServerMarketRefresh_Implementation()
{
	if (bMarketOpen && NearTradingPost())
	{
		MarketSend(false);
	}
}

void UNWInteractorComponent::ServerMarketBuy_Implementation(int32 Req, const FString& OrderId, int32 Quantity)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	ANWPlayerController* PC = MarketPC(this);
	const UNWBackend::FMarketOrder* O = FindOrder(OrderId);
	if (!Me || !PC || !PC->Record || !bMarketOpen || !NearTradingPost() || !O || !O->bSell || O->CharacterId == PC->Record->Id
		|| Quantity <= 0 || Quantity > O->Item.Quantity)
	{
		ClientMarketDone(Req, false);
		return;
	}
	const int64 Total = O->UnitPrice * Quantity;   // no fees
	const FString Reason = FString::Printf(TEXT("market_buy:%s:%d"), *O->Item.ItemId.ToString(), Quantity);
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	NWOps::Run(Me, Reason,
		[Self, Me, Total]()
		{
			if (Me->Inventory->Coins < Total)
			{
				MarketWarn(Self.Get(), TEXT("ui_insufficient_funds"));
				return false;
			}
			Me->Inventory->AddCoins(-Total);
			NWOps::Hold(Me, [Self]() { MarketWarn(Self.Get(), TEXT("nw.market.bought")); });
			return true;
		},
		MarketCommit([Self, OrderId, Quantity, Reason](const FNWCharacterRecord& After, TFunction<void(int32, const FString&)> Done)
		{
			UNWBackend* B = Self.IsValid() ? MarketBackend(Self.Get()) : nullptr;
			B ? B->FillOrder(After, OrderId, Quantity, nullptr, Reason, Done) : Done(0, FString());
		}),
		[Self, Req](bool bOk) { if (Self.IsValid()) { Self->MarketFinish(Req, bOk); } });
}

void UNWInteractorComponent::ServerMarketSell_Implementation(int32 Req, const FString& OrderId, int32 BagSlot, int32 Quantity)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	ANWPlayerController* PC = MarketPC(this);
	const UNWBackend::FMarketOrder* O = FindOrder(OrderId);
	if (!Me || !PC || !PC->Record || !bMarketOpen || !NearTradingPost() || !O || O->bSell || O->CharacterId == PC->Record->Id
		|| Quantity <= 0 || Quantity > O->Item.Quantity)
	{
		ClientMarketDone(Req, false);
		return;
	}
	const FName Wanted = O->Item.ItemId;
	const int64 Total = O->UnitPrice * Quantity;
	const FString Reason = FString::Printf(TEXT("market_sell:%s:%d"), *Wanted.ToString(), Quantity);
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	TSharedRef<FNWItem> Delivered = MakeShared<FNWItem>();
	NWOps::Run(Me, Reason,
		[Self, Me, BagSlot, Quantity, Wanted, Total, Delivered]()
		{
			const FNWItem* In = Me->Inventory->At(ENWItemContainer::Bag, BagSlot);
			const FNWItemDef* D = In ? FNWItemCatalog::Get().Find(In->ItemId) : nullptr;
			if (!D || In->ItemId != Wanted || In->Quantity < Quantity || !FNWItemRules::Get().CanTrade(*D, *In))
			{
				MarketWarn(Self.Get(), TEXT("nw.market.not_tradeable"));
				return false;
			}
			*Delivered = Me->Inventory->TakeAt(BagSlot, Quantity);
			Me->Inventory->AddCoins(Total);
			TWeakObjectPtr<UNWInventoryComponent> Inv(Me->Inventory);
			NWOps::Hold(Me, [Inv, Total]() { if (Inv.IsValid()) { Inv->ClientLoot({}, Total); } });
			return true;
		},
		MarketCommit([Self, OrderId, Quantity, Reason, Delivered](const FNWCharacterRecord& After, TFunction<void(int32, const FString&)> Done)
		{
			UNWBackend* B = Self.IsValid() ? MarketBackend(Self.Get()) : nullptr;
			B ? B->FillOrder(After, OrderId, Quantity, &Delivered.Get(), Reason, Done) : Done(0, FString());
		}),
		[Self, Req](bool bOk) { if (Self.IsValid()) { Self->MarketFinish(Req, bOk); } });
}

void UNWInteractorComponent::ServerMarketPostSell_Implementation(int32 Req, int32 BagSlot, int32 Quantity, int64 UnitPrice, int32 Days)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!Me || !bMarketOpen || !NearTradingPost() || Quantity <= 0 || UnitPrice <= 0 || !NWMarket::ValidDays(Days) || !MarketBackend(this))
	{
		ClientMarketDone(Req, false);
		return;
	}
	if (NWCity::Rules().ItemPriceMax > 0 && UnitPrice > NWCity::Rules().ItemPriceMax)
	{
		MarketWarn(this, TEXT("nw.market.price_too_high"));
		ClientMarketDone(Req, false);
		return;
	}
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	TSharedRef<FNWItem> Listed = MakeShared<FNWItem>();
	NWOps::Run(Me, TEXT("market_post_sell"),
		[Self, Me, BagSlot, Quantity, Listed]()
		{
			const FNWItem* In = Me->Inventory->At(ENWItemContainer::Bag, BagSlot);
			const FNWItemDef* D = In ? FNWItemCatalog::Get().Find(In->ItemId) : nullptr;
			if (!D || In->Quantity < Quantity || !FNWItemRules::Get().CanTrade(*D, *In))
			{
				MarketWarn(Self.Get(), TEXT("nw.market.not_tradeable"));
				return false;
			}
			*Listed = Me->Inventory->TakeAt(BagSlot, Quantity);
			return true;
		},
		MarketCommit([Self, Listed, UnitPrice, Days](const FNWCharacterRecord& After, TFunction<void(int32, const FString&)> Done)
		{
			UNWBackend* B = Self.IsValid() ? MarketBackend(Self.Get()) : nullptr;
			B ? B->CreateOrder(After, true, *Listed, UnitPrice, Days * 24, TEXT("market_post_sell"), Done) : Done(0, FString());
		}),
		[Self, Req](bool bOk) { if (Self.IsValid()) { Self->MarketFinish(Req, bOk); } });
}

void UNWInteractorComponent::ServerMarketPostBuy_Implementation(int32 Req, FName ItemId, int32 Quantity, int64 UnitPrice, int32 Days)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	const FNWItemDef* D = FNWItemCatalog::Get().Find(ItemId);
	if (!Me || !D || !bMarketOpen || !NearTradingPost() || Quantity <= 0 || UnitPrice <= 0 || !NWMarket::ValidDays(Days) || !MarketBackend(this)
		|| (NWCity::Rules().ItemPriceMax > 0 && UnitPrice > NWCity::Rules().ItemPriceMax))
	{
		ClientMarketDone(Req, false);
		return;
	}
	// Equipment buy order: any crafted piece of that item (the seller delivers theirs).
	FNWItem Probe;
	Probe.ItemId = ItemId;
	Probe.Quantity = Quantity;
	Probe.Quality = FNWItemRules::EquipClass(*D).IsEmpty() ? -1.f : 0.f;
	if (!FNWItemRules::Get().CanTrade(*D, Probe))
	{
		MarketWarn(this, TEXT("nw.market.not_tradeable"));
		ClientMarketDone(Req, false);
		return;
	}
	const int64 Total = UnitPrice * Quantity;
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	FNWItem Wanted;
	Wanted.ItemId = ItemId;
	Wanted.Quantity = Quantity;
	NWOps::Run(Me, TEXT("market_post_buy"),
		[Self, Me, Total]()
		{
			if (Me->Inventory->Coins < Total)
			{
				MarketWarn(Self.Get(), TEXT("ui_insufficient_funds"));
				return false;
			}
			Me->Inventory->AddCoins(-Total);   // order collateral (comes back as credit if cancelled or expired)
			return true;
		},
		MarketCommit([Self, Wanted, UnitPrice, Days](const FNWCharacterRecord& After, TFunction<void(int32, const FString&)> Done)
		{
			UNWBackend* B = Self.IsValid() ? MarketBackend(Self.Get()) : nullptr;
			B ? B->CreateOrder(After, false, Wanted, UnitPrice, Days * 24, TEXT("market_post_buy"), Done) : Done(0, FString());
		}),
		[Self, Req](bool bOk) { if (Self.IsValid()) { Self->MarketFinish(Req, bOk); } });
}

void UNWInteractorComponent::ServerMarketCancel_Implementation(int32 Req, const FString& OrderId)
{
	UNWBackend* B = MarketBackend(this);
	ANWPlayerController* PC = MarketPC(this);
	const UNWBackend::FMarketOrder* O = FindOrder(OrderId);
	if (!B || !PC || !PC->Record || !bMarketOpen || !O || O->CharacterId != PC->Record->Id)
	{
		ClientMarketDone(Req, false);
		return;
	}
	// nw-core returns the item to Storage (sale) or the collateral as credit (purchase), which is claimed right after.
	const bool bBuy = !O->bSell;
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	B->CancelOrder(PC->Record->Id, OrderId, [Self, Req, bBuy](int32 Status)
	{
		if (!Self.IsValid())
		{
			return;
		}
		if (Status != 204)
		{
			MarketWarn(Self.Get(), TEXT("nw.market.failed"));
		}
		else if (bBuy)
		{
			Self->MarketClaimCredits();
		}
		Self->MarketFinish(Req, Status == 204);
	});
}

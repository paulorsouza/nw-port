#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWItems.h"
#include "NWOps.h"
#include "NWCity.h"
#include "NWMarket.h"
#include "NWInteractable.generated.h"

class ANWCharacter;

/**
 * Something the player uses with the interact key (area 4: NPC, station, storage, Trading Post). The
 * player's UNWInteractorComponent picks the focus and NWHUD draws the game card (name + [E] action).
 * OnInteract runs on the owning client; the system calls the server through its own component when needed.
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWInteractableComponent();
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	static const TArray<TWeakObjectPtr<UNWInteractableComponent>>& All();

	/** Name on the card strip. */
	FText DisplayName;
	/** Action on the key line (text key: from the game, e.g. ui_talk, or tools/strings). */
	FString ActionKey = TEXT("ui_talk");
	/** Maximum distance (cm) from the player to the focus point. */
	float Radius = 250.f;
	/** Height of the focus point (card ring) above the actor origin. */
	float FocusHeight = 0.f;
	TFunction<void(ANWCharacter*)> OnInteract;

	FVector FocusLocation() const;
};

/**
 * Player's: picks the interactable in focus (the closest in front) and handles the key (owning client). On the server,
 * delivers the NPC gifts (FNWNpcRewards) and keeps what was already taken, replicated to the owner for the NPC icon.
 */
UCLASS(ClassGroup = (NW))
class NWPORT_API UNWInteractorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWInteractorComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** true if there was a focus and the action was done (gathering does not get the key). */
	bool InteractPressed();

	TWeakObjectPtr<UNWInteractableComponent> Focus;

	/** NPC gift (variant): the server checks distance, the week's draw and what was already picked up. */
	UFUNCTION(Server, Reliable) void ServerNpcInteract(const FString& Variant);
	/** Town teleporter (docs/game/city.md "Teleporter"): to the other town's map, beside its teleporter. */
	UFUNCTION(Server, Reliable) void ServerFastTravel();
	/** Keys "<kind>:<npc>:<period>" of what the character already picked up (nw-core; only the owner receives them). */
	UPROPERTY(Replicated) TArray<FString> RewardClaims;
	/** Bag on the ground (NWLootBag.h): the server checks owner and distance and moves what fits into the bag. */
	UFUNCTION(Server, Reliable) void ServerLootTake(class ANWLootBag* Bag);
	/** Takes Quantity of the item in slot Slot of the bag on the ground (bag screen: drag, right click, split). */
	UFUNCTION(Server, Reliable) void ServerLootTakeItem(class ANWLootBag* Bag, int32 Slot, int32 Quantity);
	/** Refining (NWCrafting.h): server checks nearby station, level, materials and fee; an error warns the owner. */
	UFUNCTION(Server, Reliable) void ServerRefine(const FString& StationType, FName Recipe, int32 Count);
	/** Shop (NWShop.h): the server checks NPC nearby, price, weekly limit and weight; writes at once (SavePlayer). */
	UFUNCTION(Server, Reliable) void ServerShopBuy(const FString& ShopId, FName Item, int32 Quantity);
	/** Craft weapon, shield or tool (docs/game/crafting.md; FNWCrafting::Craft): instant, item in the bag; training below the
	 *  crafting level. */
	UFUNCTION(Server, Reliable) void ServerCraft(const FString& StationType, FName Recipe);
	/** Upgrade of the bag item at its trade skill station, paying materials (docs/game/items.md): Kind = ENWImprove, Mat =
	 *  chosen material (FNWCrafting::ImproveMaterials). */
	UFUNCTION(Server, Reliable) void ServerImprove(const FString& StationType, int32 BagSlot, uint8 Kind, uint8 Mat);
	/** Result of the last craft (Kind 255) or upgrade (ENWImprove) for the station screen: the item before and after. */
	UFUNCTION(Client, Reliable) void ClientCraftResult(const FNWItem& Before, const FNWItem& After, uint8 Kind);
	struct FCraftResult { FNWItem Before, After; uint8 Kind = 255; int32 Seq = 0; };
	/** Owner only: Seq goes up on every result (the screen waits for a Seq greater than the click's). */
	FCraftResult CraftResult;

	// ---- shared storage (area 4, docs/game/city.md "Storage"): one per character, opened at any pantry,
	// 5 tabs; the server keeps it and writes to nw-core (bag + storage in one transaction).
	/** Open ("shared") or closed (empty), the items (Tab = tab), tab names and capacity; only the owner receives it. */
	UPROPERTY(Replicated) FString StorageLocation;
	UPROPERTY(Replicated) TArray<FNWItem> StorageItems;
	UPROPERTY(Replicated) TArray<FString> StorageTabNames;
	UPROPERTY(Replicated) float StorageCapacity = 0.f;
	float StorageWeight() const;
	/** Capacity from the sources: armazem_por_cidade × known cities (houses and city level when they exist). */
	float StorageCapacityNow() const;
	/** Opens at the pantry of city City (the city becomes known). */
	UFUNCTION(Server, Reliable) void ServerStorageOpen(const FString& City);
	UFUNCTION(Server, Reliable) void ServerStorageClose();
	/** Deposit (Slot = bag slot, into tab Tab) or withdraw (Slot = storage slot), Quantity of the stack. */
	UFUNCTION(Server, Reliable) void ServerStorageMove(bool bDeposit, int32 Slot, int32 Quantity, uint8 Tab);
	/** Several whole stacks at once, written in a single transaction: "Pegar tudo" (catcontainer ContainerTakeAll) and
	 *  move the whole section (catsection TransferAll). Bag slots (store, into tab Tab) or storage slots (withdraw). */
	UFUNCTION(Server, Reliable) void ServerStorageMoveMany(bool bDeposit, const TArray<int32>& Slots, uint8 Tab);
	/** Moves the stack in storage slot Slot to tab Tab (merges with the same item already there). */
	UFUNCTION(Server, Reliable) void ServerStorageSetTab(int32 Slot, uint8 Tab);
	/** Tab name (empty = default; up to 24 letters). */
	UFUNCTION(Server, Reliable) void ServerStorageRenameTab(uint8 Tab, const FString& Name);

	// ---- market (NWMarket.h, NWMarket.cpp): only near the Trading Post; the owner receives the order book rows.
	UFUNCTION(Server, Reliable) void ServerMarketOpen();
	UFUNCTION(Server, Reliable) void ServerMarketClose();
	UFUNCTION(Server, Reliable) void ServerMarketRefresh();
	/** Buys Quantity from a sell order (goes to storage). */
	UFUNCTION(Server, Reliable) void ServerMarketBuy(int32 Req, const FString& OrderId, int32 Quantity);
	/** Sells Quantity of the item in slot BagSlot to a buy order. */
	UFUNCTION(Server, Reliable) void ServerMarketSell(int32 Req, const FString& OrderId, int32 BagSlot, int32 Quantity);
	/** Creates a sell order with Quantity of the item in slot BagSlot. */
	UFUNCTION(Server, Reliable) void ServerMarketPostSell(int32 Req, int32 BagSlot, int32 Quantity, int64 UnitPrice, int32 Days);
	/** Creates a buy order (price × quantity leaves the coins right away). */
	UFUNCTION(Server, Reliable) void ServerMarketPostBuy(int32 Req, FName ItemId, int32 Quantity, int64 UnitPrice, int32 Days);
	UFUNCTION(Server, Reliable) void ServerMarketCancel(int32 Req, const FString& OrderId);
	UFUNCTION(Client, Reliable) void ClientMarketOrders(const TArray<FNWMarketRow>& Rows);
	/** End of client request Req (from the screen; the new book arrives before it, in order). bOk = written. */
	UFUNCTION(Client, Reliable) void ClientMarketDone(int32 Req, bool bOk);
	/** Client: order book received and the number of the last update (the screen redraws when it changes). */
	TArray<FNWMarketRow> MarketRows;
	int32 MarketSeq = 0;
	/** Client: finished requests (Req, written) the screen has not read yet. */
	TArray<TPair<int32, bool>> MarketDone;

	// ---- city (governance; NWCity.h, NWCity.cpp): near the Town Project Board (players) or the Territory Planning
	// Board (governor). Same scheme as the market: Req from the screen, state and end of the request.
	UFUNCTION(Server, Reliable) void ServerCityOpen();
	UFUNCTION(Server, Reliable) void ServerCityClose();
	UFUNCTION(Server, Reliable) void ServerCityRefresh();
	/** Sells Quantity of raw material from the bag to the city at the seen price (UnitPrice). */
	UFUNCTION(Server, Reliable) void ServerCitySellRaw(int32 Req, FName Item, int32 Quantity, int64 UnitPrice);
	/** Buys Quantity of an item from the city list. */
	UFUNCTION(Server, Reliable) void ServerCityBuyItem(int32 Req, FName Item, int32 Quantity, int64 UnitPrice);
	/** Buys the city at the seen price (starting or the one asked by the owner). */
	UFUNCTION(Server, Reliable) void ServerCityBuy(int32 Req, int64 Price);
	/** Governor: opens the 24 h auction with starting bid Price (> 0) or cancels (0, only without a bid). */
	UFUNCTION(Server, Reliable) void ServerCitySale(int32 Req, int64 Price);
	/** Bid in the city auction (leaves the coins; comes back as credit if someone outbids). */
	UFUNCTION(Server, Reliable) void ServerCityBid(int32 Req, int64 Amount);
	/** Governor: prices (only the list items) and tax (< 0 = keep). */
	UFUNCTION(Server, Reliable) void ServerCityManage(int32 Req, const TArray<FNWCityPrice>& Buy, const TArray<FNWCityPrice>& Sell, float Tax);
	/** Governor: "station" (upgrade or repair), "buff" or "weapon". */
	UFUNCTION(Server, Reliable) void ServerCityProject(int32 Req, const FString& Kind, const FString& Station, const FString& Buff);
	UFUNCTION(Client, Reliable) void ClientCityState(const FNWCityState& State);
	UFUNCTION(Client, Reliable) void ClientCityDone(int32 Req, bool bOk);
	/** Client: city received, number of the last update and finished requests. */
	FNWCityState CityState;
	int32 CitySeq = 0;
	TArray<TPair<int32, bool>> CityDone;

private:
	void LoadClaims();
	bool bClaimsLoaded = false;
	bool bClaimsLoading = false;
	bool bClaimBusy = false;
	FString ClaimsPeriod;
	// Storage on the server.
	int64 StorageVersion = 0;
	bool bStorageBusy = false;
	TArray<FNWItem> MemStorage;   // without backend (PIE)
	TArray<FString> MemTabNames;

	/** Storage operation write (NWOps): PUT storage with the version; 409 rereads and redoes, 208 rereads the version. */
	NWOps::FCommit StorageCommit();
	/** Runs a storage change as a written operation (NWOps::Run). */
	void RunStorageOp(TFunction<bool(UNWInteractorComponent&)> Apply);
	// City on the server.
	bool bCityOpen = false;
	FString CityRegion;
	bool NearCityBoard() const;
	/** Reads the city (or uses Json, an action's response), keeps the copy and sends it to the owner; then Then. */
	void CitySend(TSharedPtr<FJsonObject> Json = nullptr, TFunction<void()> Then = nullptr);
	void CityFinish(int32 Req, bool bOk, TSharedPtr<FJsonObject> Json);
	void CityGovernor(int32 Req, const FString& Verb, const FString& Sub, TSharedRef<FJsonObject> Body);
	// Market on the server.
	bool bMarketOpen = false;
	bool NearTradingPost() const;
	void MarketSend(bool bForce);
	/** Rereads the book, sends the rows and then the end of request Req. */
	void MarketFinish(int32 Req, bool bOk);
	void MarketClaimCredits();
	/** Write of a market operation: Call writes and returns the status and the nw-core error code. */
	NWOps::FCommit MarketCommit(TFunction<void(const FNWCharacterRecord&, TFunction<void(int32, const FString&)>)> Call);
	/** Moves a stack (whole or Quantity) without writing; returns what changed (Quantity 0 = nothing; Tab = storage tab). */
	FNWItem StorageApply(bool bDeposit, int32 Slot, int32 Quantity, bool bWarn, uint8 Tab);
	/** Merges into the same stack in the same tab (Item.Tab) or takes a new slot in it. */
	static void StorageAdd(TArray<FNWItem>& Items, const FNWItem& Item);
	/** Takes from the same stack, preferably from the same tab. */
	static bool StorageRemove(TArray<FNWItem>& Items, const FNWItem& Item);
};

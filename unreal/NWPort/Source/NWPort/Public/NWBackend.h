#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Net/Core/Connection/NetEnums.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NWItems.h"
#include "NWBackend.generated.h"

class FJsonObject;

/** GameUserSettings.ini sections with the player's choices (local cache of what the server stores). */
namespace NWIni
{
	inline const TCHAR* Loadout = TEXT("NW.Loadout");
	inline const TCHAR* Mastery = TEXT("NW.Mastery");
}

/** Character as nw-core returns and receives it (position in Unreal cm). */
struct FNWCharacterRecord
{
	FString Id;
	FString Name;
	FString AccountName;
	FString Map;
	bool bHasPosition = false;
	FVector Position = FVector::ZeroVector;
	float Yaw = 0.f;
	float Health = -1.f;          // < 0 = no value (spawns with full health)
	FString ActiveWeapon;
	TSharedPtr<FJsonObject> Loadout;  // { "Sword": ["ShieldBash", ...], ... }
	TSharedPtr<FJsonObject> Mastery;  // { "sword": ["id", ...], ... }
	TSharedPtr<FJsonObject> Progression;  // { "level": 3, "xp": 1200, "spent": [5], "trade": [4] } (UNWProgressionComponent)
	float StorageCapacity = -1.f;         // shared storage (kg, sum of the sources); < 0 = not sent
	int64 Coins = 0;
	TArray<FNWItem> Items;
	bool bFirstJoin = false;          // first join: the server gives the starter kit
	bool bAdmin = false;              // admin account: test commands (F10) on the server

	static TOptional<FNWCharacterRecord> FromJson(const TSharedPtr<FJsonObject>& Obj);
	FString StateJson() const;
};

/** Character in the login screen list. */
struct FNWCharacterSummary
{
	FString Id;
	FString Name;
	FString ActiveWeapon;
	FDateTime LastSeen;
	bool bHasLastSeen = false;
};

/**
 * Talks to nw-core (Rust backend).
 *
 * Server: `-nwbackend=<url>` and NW_SERVER_SECRET turn on the pass requirement on connect; without them the
 * server accepts anyone and saves nothing (arena, local tests).
 *
 * Client: backend and server address come from [NW.Server] Backend/Address in DefaultGame.ini (or
 * `-nwbackend=` / `-nwconnect=`). With a backend configured, the game opens on the login screen (FNWLoginMenu).
 * When nw-core knows the server of each map (NW_MAP_SERVERS), the join returns the address and `-nwconnect` is not needed.
 * Tests without a screen: `-nwlogin=<account> -nwchar=<character>` with the password in NW_PASSWORD joins directly.
 */
UCLASS()
class NWPORT_API UNWBackend : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	bool IsServerEnabled() const { return !BaseUrl.IsEmpty() && !ServerSecret.IsEmpty(); }

	// ---- client
	using FDone = TFunction<void(bool bOk, const FString& Error)>;
	/** Login screen active: client with backend, outside a server and without automatic join. */
	bool WantsFrontEnd(const UWorld* World) const;
	bool HasSession() const { return !SessionToken.IsEmpty(); }
	bool IsBusy() const { return Busy > 0; }
	const FString& GetAccountName() const { return AccountName; }
	const TArray<FNWCharacterSummary>& GetCharacters() const { return Characters; }
	/** Last connection failure to the game server (shown and cleared by the login screen). */
	FString TakeConnectionError() { FString E = MoveTemp(ConnectionError); ConnectionError.Reset(); return E; }

	void Login(const FString& Name, const FString& Password, FDone Done);
	void Register(const FString& Invite, const FString& Name, const FString& Password, FDone Done);
	void RefreshCharacters(FDone Done);
	void CreateCharacter(const FString& Name, FDone Done);
	void DeleteCharacter(const FString& Id, FDone Done);
	/** Requests the pass and connects to the game server. */
	void Play(const FString& CharacterId, FDone Done);
	void Logout();

	using FRedeemDone = TFunction<void(TOptional<FNWCharacterRecord> Record, const FString& Error)>;
	void RedeemTicket(const FString& Ticket, FRedeemDone Done);
	/** Server: moves the character to the server of Map (one server per map, NW_MAP_SERVERS in nw-core). nw-core saves the
	 *  map without a position and returns a new pass; Url = `<address>?ticket=<pass>` for ClientTravel. */
	void Travel(const FString& CharacterId, const FString& Map, TFunction<void(bool bOk, const FString& Url, const FString& Error)> Done);
	/** Saves the state; Reason goes to the nw-core item log ("periodic", "logout", "craft:<recipe>"...). Done receives
	 *  the status (204 written, 208 already written). */
	/** Extra: additional fields in the body (e.g. `city_tax`, the city's crafting tax, in the same transaction). */
	void SaveState(const FNWCharacterRecord& Record, const FString& Reason = TEXT("save"), TFunction<void(int32 Status)> Done = nullptr,
		TSharedPtr<FJsonObject> Extra = nullptr);
	/** Ground bag (step 2 of the writes): dropping writes the Record's bag and the ground bag in one transaction; picking up
	 *  writes the bag and what is left (empty = gone; 409 ground_gone = expired or taken); the list has live bags. */
	struct FGroundBag { FString Id; FString Map; FVector Position = FVector::ZeroVector; double ExpiresIn = 0.0; TArray<FNWItem> Items; };
	void DropGround(const FNWCharacterRecord& Record, const FString& Map, const FVector& Position, int32 TtlSeconds, const TArray<FNWItem>& Items,
		const FString& Reason, TFunction<void(int32 Status, const FGroundBag& Bag)> Done);
	void TakeGround(const FNWCharacterRecord& Record, const FString& BagId, const TArray<FNWItem>& Remaining, const FString& Reason,
		TFunction<void(int32 Status)> Done);
	void ListGround(const FString& CharacterId, TFunction<void(bool bOk, const TArray<FGroundBag>& Bags)> Done);

	// Market (docs/game/city.md "Trading Post"): one for everyone ("global" territory). nw-core error in Code
	// (order_closed, storage_full, too_many_orders...).
	struct FMarketOrder
	{
		FString Id;
		bool bSell = true;
		FString CharacterId;
		FString Seller;          // name of the owning character
		FNWItem Item;            // Quantity = what is left to trade
		int64 UnitPrice = 0;
		double ExpiresIn = 0.0;
	};
	void ListOrders(TFunction<void(bool bOk, const TArray<FMarketOrder>& Orders)> Done);
	void CreateOrder(const FNWCharacterRecord& Record, bool bSell, const FNWItem& Item, int64 UnitPrice, int32 DurationHours, const FString& Reason,
		TFunction<void(int32 Status, const FString& Code)> Done);
	void FillOrder(const FNWCharacterRecord& Record, const FString& OrderId, int32 Quantity, const FNWItem* Delivered, const FString& Reason,
		TFunction<void(int32 Status, const FString& Code)> Done);
	void CancelOrder(const FString& CharacterId, const FString& OrderId, TFunction<void(int32 Status)> Done);

	// ---- city governance (backend/src/routes/city.rs). Done: status, error code, city (body).
	using FCityDone = TFunction<void(int32 Status, const FString& Code, TSharedPtr<FJsonObject> City)>;
	void GetCity(const FString& Region, const FString& CharacterId, FCityDone Done);
	/** Sell raw material to the city (bSellRaw) or buy from the list; Record already with the bag and coins after. */
	void CityTrade(const FNWCharacterRecord& Record, const FString& Region, bool bSellRaw, FName Item, int32 Quantity, int64 UnitPrice,
		const FString& Reason, FCityDone Done);
	/** Buy the city at the seen price; Record already without the coins. */
	void CityBuy(const FNWCharacterRecord& Record, const FString& Region, int64 Price, const FString& Reason, FCityDone Done);
	/** Records coins the character did not earn because of the cap (log only; the coins do not change). */
	void CoinsCap(const FString& CharacterId, int64 Amount, const FString& Reason);
	/** Bid in the city auction; Record already without the bid coins. */
	void CityBid(const FNWCharacterRecord& Record, const FString& Region, int64 Amount, const FString& Reason, FCityDone Done);
	/** Governor actions without a bag: Sub = "" (PUT prices/tax), "sale", "projects". */
	void CityAction(const FString& Verb, const FString& Region, const FString& Sub, TSharedRef<FJsonObject> Body, FCityDone Done);
	/** Pending sale credits and the instant up to which they count (the claim carries that instant). */
	void PendingCredits(const FString& CharacterId, TFunction<void(bool bOk, int64 Amount, const FString& Until)> Done);
	void ClaimCredits(const FNWCharacterRecord& Record, const FString& Until, TFunction<void(int32 Status, int64 Amount)> Done);

	// ---- editor Play with the local test nw-core (docs/tech/backend-testing.md)
	/** Editor only: reads backend/dev.env (NW_BACKEND, NW_SERVER_SECRET, NW_PIE_ACCOUNT, NW_PIE_CHARACTER, NW_TEST_PASSWORD)
	 *  and enables the backend in standalone Play. No NW_PIE_ACCOUNT: returns false and Play runs without backend. */
	bool EnablePie();
	/** Back to playing without backend (nw-core down). */
	void DisablePie();
	bool IsPie() const { return bPie; }
	/** Joins with the editor account and character: login, pass and redeem, like a client on the dedicated server. */
	void PieEnter(FRedeemDone Done);
	/** Editor Play: the account's character after CurrentId (creates one when the account has only this one) and its
	 *  redeemed record. */
	void PieSwitch(const FString& CurrentId, FRedeemDone Done);
	// NPC gifts (area 4, NWNpcRewards.h): what was already picked up ("<kind>:<npc>:<period>") and the claim, written
	// together with the Record's bag and coins (Status 201 ok, 409 already picked up).
	void ListNpcRewards(const FString& CharacterId, const FString& Period, TFunction<void(bool bOk, const TArray<FString>& Keys)> Done);
	void ClaimNpcReward(const FNWCharacterRecord& Record, const FString& Npc, const FString& Kind, const FString& Period,
		TFunction<void(bool bOk, int32 Status)> Done);
	// Shared storage (area 4, location "shared"): read and write with the version read; the write also carries the bag and
	// the Record's coins, the unit weight of each item (catalog), the tab names and the capacity summed by the
	// server. Status 200 ok, 409 the storage changed (read again).
	/** Serial number of a signed item; the character is the crafter (docs/game/items.md, Signature). */
	void IssueSerial(const FString& CharacterId, TFunction<void(bool bOk, const FString& Serial)> Done);
	void GetStorage(const FString& CharacterId, const FString& Location,
		TFunction<void(bool bOk, int64 Version, const TArray<FNWItem>& Items, const TArray<FString>& TabNames)> Done);
	/** Status 208 = nw-core had already applied this write (resent after losing the response): read again, without
	 *  redoing the change. */
	void PutStorage(const FNWCharacterRecord& Record, const FString& Location, int64 Version, const TArray<FNWItem>& Items,
		const TArray<FString>& TabNames, float Capacity, const FString& Reason, TFunction<void(int32 Status, int64 NewVersion)> Done);

private:
	/**
	 * Bag, coins and storage writes (docs/tech/server.md "Recordings"): one queue per character, one at a time, in
	 * order. Each carries the session opened when redeeming the pass and an increasing number; nw-core refuses an old
	 * write. No response (network, nw-core restarting): retries with the same number (up to MaxWriteTries); nw-core
	 * answers 409 duplicate_write if it had already applied it, which becomes 208 for the caller.
	 */
	void Write(const FString& CharacterId, const FString& Verb, const FString& Path, TSharedRef<FJsonObject> Body, const FString& Reason,
		TFunction<void(int32 Status, TSharedPtr<FJsonObject> Body)> Done);
	void PumpWrites(const FString& CharacterId);
	struct FPendingWrite
	{
		FString Verb, Path;
		TSharedPtr<FJsonObject> Body;
		TFunction<void(int32, TSharedPtr<FJsonObject>)> Done;
		int32 Tries = 0;
	};
	struct FWriteQueue
	{
		int64 Session = -1;
		int64 NextSeq = 0;
		bool bBusy = false;
		TArray<FPendingWrite> Pending;
	};
	TMap<FString, FWriteQueue> Writes;
	/** Last written bag and equipment of each character: the save sends only the slots that changed
	 *  (inventory_patch). Updated on every write that carries the bag. */
	TMap<FString, TArray<FNWItem>> PersistedBag;
	void RememberBag(const FNWCharacterRecord& Record) { PersistedBag.Add(Record.Id, Record.Items); }
	static constexpr int32 MaxWriteTries = 6;
	using FJsonDone = TFunction<void(int32 Status, TSharedPtr<FJsonObject> Body)>;
	void Request(const FString& Verb, const FString& Path, const FString& Body, bool bServer, FJsonDone Done);

	void ClientAutoConnect();
	void OpenSession(const TSharedPtr<FJsonObject>& Json);
	void SaveSession() const;
	void HandleNetworkFailure(UWorld* World, class UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);
	static FString ErrorMessage(int32 Status, const TSharedPtr<FJsonObject>& Body);

	FString BaseUrl;
	FString ServerSecret;
	FString SessionToken;
	FString AccountName;
	FString ConnectAddress;
	FString ConnectionError;
	TArray<FNWCharacterSummary> Characters;
	int32 Busy = 0;
	bool bAutoConnect = false;
	bool bPie = false;
	FString PieAccount, PiePassword, PieCharacter;
	FDelegateHandle NetworkFailureHandle, TravelFailureHandle;
};

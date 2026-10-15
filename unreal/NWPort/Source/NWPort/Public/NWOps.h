#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "NWItems.h"

class ANWCharacter;
struct FNWCharacterRecord;

/**
 * Operations recorded before they take effect (step 2 of the recordings, docs/tech/server.md "Recordings").
 *
 * An operation (store, craft, refine, improve, drop, pick up from the ground) runs like this, one at a time per character:
 * 1. Apply changes the real state (bag, coins, storage) as usual; client notices made with NWOps::Hold
 *    (loot, craft result) are held back; so is the overflow that would go to the ground (Grant with a full bag).
 * 2. The result is copied and the state goes back to what it was in the same cycle: replication (end of cycle) shows nothing.
 * 3. Commit records to nw-core (state, storage or ground).
 * 4. Recorded: the result takes effect (state, notices, sack on the ground) and the player's Record moves to it. Failed: nothing changes and the
 *    player sees the notice. Without nw-core (Play without a backend), applies immediately.
 * While recording, bag and coin changes from outside (gathering, loot, moving items, potion) wait (NWOps::Defer) and the
 * periodic save waits (NWOps::After). Progression (trade skill XP) is not rolled back.
 */
struct NWPORT_API FNWOpState
{
	TArray<FNWItem> Items;
	int64 Coins = 0;
	TArray<FNWItem> Storage;
	TArray<FString> TabNames;
	bool bHasStorage = false;
};

namespace NWOps
{
	/** Ok recorded; Failed not recorded (generic notice); Rejected nw-core refused and whoever recorded already gave the reason;
	 *  Retry redoes it on top of the new state. */
	enum class EResult : uint8 { Ok, Failed, Retry, Rejected };
	/** Records the result (After = Record with the new state; S1 = the new state) and calls Fin. */
	using FCommit = TFunction<void(const FNWCharacterRecord& After, const FNWOpState& S1, TFunction<void(EResult)> Fin)>;

	/** Queues an operation. Apply returns false when nothing changed (refusal, no material). Done(true) when it took effect. */
	NWPORT_API void Run(ANWCharacter* Who, const FString& Reason, TFunction<bool()> Apply, FCommit Commit, TFunction<void(bool)> Done = nullptr);
	/** Records the whole state (PUT state; bag, coins and progression). */
	NWPORT_API FCommit CommitState(ANWCharacter* Who, const FString& Reason);
	/** Same, with extra body fields read at record time (Apply already ran): e.g. the city tax. */
	NWPORT_API FCommit CommitState(ANWCharacter* Who, const FString& Reason, TFunction<TSharedPtr<FJsonObject>()> Extra);

	/** With nw-core (operations record); without it, they apply immediately. */
	NWPORT_API bool Persists(const ANWCharacter* Who);
	/** Reason for the operations opened inside this scope (e.g. "dev:<command>", "gather"), for the item log. */
	struct NWPORT_API FReason
	{
		explicit FReason(const FString& InReason);
		~FReason();
	private:
		FString Previous;
	};
	NWPORT_API FString CurrentReason(const TCHAR* Default);
	/** An operation is recording: outside changes wait. */
	NWPORT_API bool IsBusy(const ANWCharacter* Who);
	/** Inside the Apply of an operation. */
	NWPORT_API bool IsApplying(const ANWCharacter* Who);
	/** If an operation is recording, stores Fn for after it and returns true. */
	NWPORT_API bool Defer(ANWCharacter* Who, TFunction<void()> Fn);
	/** Client notice: inside Apply, only if the operation takes effect; outside, immediately. */
	NWPORT_API void Hold(ANWCharacter* Who, TFunction<void()> Fn);
	/** Items that would go to the ground in Apply (full bag): they go out in a sack only if the operation takes effect. */
	NWPORT_API void Overflow(ANWCharacter* Who, const TArray<FNWItem>& Items);
	/** After the current operation (or immediately, if there is none). */
	NWPORT_API void After(ANWCharacter* Who, TFunction<void()> Fn);

	NWPORT_API FNWOpState Capture(const ANWCharacter* Who);
	NWPORT_API void Restore(ANWCharacter* Who, const FNWOpState& S);
}

#pragma once

#include "CoreMinimal.h"

struct FNWItem;
class ANWNpc;

/**
 * NPC gifts (docs/game/city.md, "NPC gifts"; data in Data/World/npc_rewards.json, tools/npc_rewards.json).
 * One group per trade skill: each week one NPC of the group, drawn the same for everyone (city + trade skill + week), delivers
 * in that city. Starting items once per character per trade skill; weekly gift once per week, city and trade skill.
 * Key of what was already claimed (same on the server, the client and nw-core): "<kind>:<npc>:<period>".
 */
class NWPORT_API FNWNpcRewards
{
public:
	static FNWNpcRewards& Get();

	enum class EState : uint8 { None, Available, Claimed };

	/** Tuesday of the current week (reset 08:00 UTC = 05:00 BRT), "YYYY-MM-DD". */
	FString Period(const FDateTime& NowUtc = FDateTime::UtcNow()) const;
	/** The group's NPC that delivers this week (variant), among the NPCs of the same trade skill on the map. */
	FString Chosen(const UWorld* World, const FString& City, const FString& Type, const FString& InPeriod) const;
	bool HasStarter(const FString& Type) const;
	static FString Key(const FString& Kind, const FString& Npc, const FString& InPeriod) { return Kind + TEXT(":") + Npc + TEXT(":") + InPeriod; }
	static FString StarterNpc(const FString& Type) { return Type; }
	static FString WeeklyNpc(const FString& City, const FString& Type) { return City + TEXT("_") + Type; }
	/** NPC state for whoever has these keys: only the week's drawn one has something; Claimed = already took everything this week. */
	EState StateFor(const ANWNpc* Npc, const TArray<FString>& Claims) const;
	/** Items of the starting gift (rolled ranges) or of the weekly one (1 from each list: material + argueiro). */
	void Starter(const FString& Type, TArray<FNWItem>& Out) const;
	/** OutCoins: coins of the weekly gift (cents). */
	void Weekly(const FString& Type, TArray<FNWItem>& Out, int64* OutCoins = nullptr) const;

private:
	FNWNpcRewards();
	struct FRange { FName Item; int32 Min = 0; int32 Max = 0; };
	struct FType
	{
		TArray<FRange> Starter;
		/** Weekly: 1 item rolled from each list (material, argueiro). */
		TArray<TArray<FRange>> WeeklyLists;
		int64 WeeklyCoinsMin = 0, WeeklyCoinsMax = 0;
	};
	TMap<FString, FType> Types;
};

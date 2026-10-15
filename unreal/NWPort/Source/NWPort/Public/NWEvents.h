#pragma once

#include "CoreMinimal.h"

class AActor;
class ANWCharacter;

/**
 * Game events on the server, for systems that need to know what happened without touching who does it (area 4:
 * level tutorials, docs/game/city.md). Who fires them: kill in UNWCombatComponent::Die, trade skill XP in
 * UNWProgressionComponent::AddTradeXp (gathering and refining), refining in FNWCrafting::Refine.
 */
struct NWPORT_API FNWEvents
{
	static FNWEvents& Get();

	/** Someone died (Victim); Killer may be null. */
	TMulticastDelegate<void(AActor* Killer, AActor* Victim)> OnKill;
	/** Character gained XP in a trade skill (gathering, skinning, refining). */
	TMulticastDelegate<void(ANWCharacter* Who, FName Tradeskill, int32 Xp)> OnTradeXp;
	/** Character refined a recipe Count times at a station (type: smelting, carpentry...). */
	TMulticastDelegate<void(ANWCharacter* Who, const FString& Station, FName Recipe, int32 Count)> OnRefine;
};

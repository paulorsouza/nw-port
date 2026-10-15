#pragma once

#include "CoreMinimal.h"

class AActor;
class ANWCharacter;

/**
 * Eventos de jogo no servidor, para sistemas que precisam saber o que aconteceu sem mexer em quem faz (esteira 4:
 * tutoriais de nível, docs/game/city.md). Quem dispara: abate em UNWCombatComponent::Die, XP de ofício em
 * UNWProgressionComponent::AddTradeXp (coleta e refino), refino em FNWCrafting::Refine.
 */
struct NWPORT_API FNWEvents
{
	static FNWEvents& Get();

	/** Alguém morreu (Victim); Killer pode ser nulo. */
	TMulticastDelegate<void(AActor* Killer, AActor* Victim)> OnKill;
	/** Personagem ganhou XP num ofício (coleta, esfolar, refino). */
	TMulticastDelegate<void(ANWCharacter* Who, FName Tradeskill, int32 Xp)> OnTradeXp;
	/** Personagem refinou Count vezes uma receita numa estação (tipo: smelting, carpentry...). */
	TMulticastDelegate<void(ANWCharacter* Who, const FString& Station, FName Recipe, int32 Count)> OnRefine;
};

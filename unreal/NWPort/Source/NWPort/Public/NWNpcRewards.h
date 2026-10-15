#pragma once

#include "CoreMinimal.h"

struct FNWItem;
class ANWNpc;

/**
 * Brindes dos NPCs (docs/sistemas.md, "Brindes dos NPCs"; dados em Data/World/npc_rewards.json, tools/npc_rewards.json).
 * Um grupo por ofício: a cada semana um NPC do grupo, sorteado igual para todos (cidade + ofício + semana), entrega
 * naquela cidade. Itens iniciais uma vez por personagem por ofício; brinde semanal uma vez por semana, cidade e ofício.
 * Chave do que já foi pego (igual no servidor, no cliente e no nw-core): "<kind>:<npc>:<period>".
 */
class NWPORT_API FNWNpcRewards
{
public:
	static FNWNpcRewards& Get();

	enum class EState : uint8 { None, Available, Claimed };

	/** Terça da semana atual (renovação 08:00 UTC = 05:00 BRT), "AAAA-MM-DD". */
	FString Period(const FDateTime& NowUtc = FDateTime::UtcNow()) const;
	/** NPC do grupo que entrega nesta semana (variante), entre os NPCs do mesmo ofício no mapa. */
	FString Chosen(const UWorld* World, const FString& City, const FString& Type, const FString& InPeriod) const;
	bool HasStarter(const FString& Type) const;
	static FString Key(const FString& Kind, const FString& Npc, const FString& InPeriod) { return Kind + TEXT(":") + Npc + TEXT(":") + InPeriod; }
	static FString StarterNpc(const FString& Type) { return Type; }
	static FString WeeklyNpc(const FString& City, const FString& Type) { return City + TEXT("_") + Type; }
	/** Estado do NPC para quem tem estas chaves: só o sorteado da semana tem algo; Claimed = já pegou tudo da semana. */
	EState StateFor(const ANWNpc* Npc, const TArray<FString>& Claims) const;
	/** Itens do inicial (faixas sorteadas) ou do semanal (1 de cada lista: material + argueiro). */
	void Starter(const FString& Type, TArray<FNWItem>& Out) const;
	/** OutCoins: moedas do brinde semanal (centavos). */
	void Weekly(const FString& Type, TArray<FNWItem>& Out, int64* OutCoins = nullptr) const;

private:
	FNWNpcRewards();
	struct FRange { FName Item; int32 Min = 0; int32 Max = 0; };
	struct FType
	{
		TArray<FRange> Starter;
		/** Semanal: 1 item sorteado de cada lista (material, argueiro). */
		TArray<TArray<FRange>> WeeklyLists;
		int64 WeeklyCoinsMin = 0, WeeklyCoinsMax = 0;
	};
	TMap<FString, FType> Types;
};

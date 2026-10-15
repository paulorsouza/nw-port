#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "NWItems.h"

class ANWCharacter;
struct FNWCharacterRecord;

/**
 * Operações gravadas antes de valer (etapa 2 das gravações, docs/plano-servidor.md "Gravações").
 *
 * Uma operação (guardar, fabricar, refinar, melhorar, soltar, pegar do chão) roda assim, uma por vez por personagem:
 * 1. Apply muda o estado real (bolsa, moedas, armazém) como sempre; os avisos ao cliente feitos com NWOps::Hold
 *    (saque, resultado da fabricação) ficam guardados; o excesso que iria para o chão (Grant com bolsa cheia) também.
 * 2. O resultado é copiado e o estado volta ao de antes no mesmo ciclo: a replicação (fim do ciclo) não mostra nada.
 * 3. Commit grava no nw-core (estado, armazém ou chão).
 * 4. Gravou: o resultado vale (estado, avisos, saco no chão) e o Record do jogador passa a ele. Falhou: nada muda e o
 *    jogador vê o aviso. Sem nw-core (Play sem backend), vale na hora.
 * Enquanto grava, mudanças de bolsa e moedas de fora (coleta, saque, mover itens, poção) esperam (NWOps::Defer) e o
 * salvamento periódico espera (NWOps::After). A progressão (XP de ofício) não volta atrás.
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
	/** Ok gravou; Failed não gravou (aviso genérico); Rejected o nw-core recusou e quem gravou já avisou o motivo;
	 *  Retry refaz em cima do estado novo. */
	enum class EResult : uint8 { Ok, Failed, Retry, Rejected };
	/** Grava o resultado (After = Record com o estado novo; S1 = o estado novo) e chama Fin. */
	using FCommit = TFunction<void(const FNWCharacterRecord& After, const FNWOpState& S1, TFunction<void(EResult)> Fin)>;

	/** Enfileira uma operação. Apply devolve false quando não mudou nada (recusa, sem material). Done(true) quando valeu. */
	NWPORT_API void Run(ANWCharacter* Who, const FString& Reason, TFunction<bool()> Apply, FCommit Commit, TFunction<void(bool)> Done = nullptr);
	/** Grava estado inteiro (PUT state; bolsa, moedas e progressão). */
	NWPORT_API FCommit CommitState(ANWCharacter* Who, const FString& Reason);
	/** Igual, com campos a mais no corpo lidos na hora de gravar (o Apply já rodou): ex. o imposto da cidade. */
	NWPORT_API FCommit CommitState(ANWCharacter* Who, const FString& Reason, TFunction<TSharedPtr<FJsonObject>()> Extra);

	/** Com nw-core (as operações gravam); sem, valem na hora. */
	NWPORT_API bool Persists(const ANWCharacter* Who);
	/** Motivo das operações abertas dentro deste escopo (ex. "dev:<comando>", "gather"), para o log de itens. */
	struct NWPORT_API FReason
	{
		explicit FReason(const FString& InReason);
		~FReason();
	private:
		FString Previous;
	};
	NWPORT_API FString CurrentReason(const TCHAR* Default);
	/** Uma operação está gravando: mudanças de fora esperam. */
	NWPORT_API bool IsBusy(const ANWCharacter* Who);
	/** Dentro do Apply de uma operação. */
	NWPORT_API bool IsApplying(const ANWCharacter* Who);
	/** Se uma operação está gravando, guarda Fn para depois dela e devolve true. */
	NWPORT_API bool Defer(ANWCharacter* Who, TFunction<void()> Fn);
	/** Aviso ao cliente: dentro do Apply, só se a operação valer; fora, na hora. */
	NWPORT_API void Hold(ANWCharacter* Who, TFunction<void()> Fn);
	/** Itens que iriam para o chão no Apply (bolsa cheia): saem num saco só se a operação valer. */
	NWPORT_API void Overflow(ANWCharacter* Who, const TArray<FNWItem>& Items);
	/** Depois da operação em curso (ou na hora, sem nenhuma). */
	NWPORT_API void After(ANWCharacter* Who, TFunction<void()> Fn);

	NWPORT_API FNWOpState Capture(const ANWCharacter* Who);
	NWPORT_API void Restore(ANWCharacter* Who, const FNWOpState& S);
}

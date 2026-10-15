#include "NWOps.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "NWBackend.h"
#include "NWCharacter.h"
#include "NWInteractable.h"
#include "NWInventoryComponent.h"
#include "NWLootBag.h"
#include "NWPlayerController.h"
#include "NWText.h"

DEFINE_LOG_CATEGORY_STATIC(LogNWOps, Log, All);

namespace
{
	struct FOp
	{
		FString Reason;
		TFunction<bool()> Apply;
		NWOps::FCommit Commit;
		TFunction<void(bool)> Done;
		int32 Retries = 0;
	};

	struct FQueue
	{
		TArray<FOp> Pending;
		bool bBusy = false;       // gravando
		bool bApplying = false;   // dentro do Apply
		TArray<TFunction<void()>> Held, Deferred, AfterFns;
		TArray<FNWItem> Over;
		FVector OverAt = FVector::ZeroVector;
	};

	TMap<TWeakObjectPtr<ANWCharacter>, FQueue> Queues;
	FString ReasonScope;

	bool SameItems(const TArray<FNWItem>& A, const TArray<FNWItem>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 i = 0; i < A.Num(); ++i)
		{
			if (!A[i].SameKind(B[i]) || A[i].Quantity != B[i].Quantity || A[i].Container != B[i].Container || A[i].Slot != B[i].Slot || A[i].Tab != B[i].Tab)
			{
				return false;
			}
		}
		return true;
	}

	bool SameState(const FNWOpState& A, const FNWOpState& B)
	{
		return A.Coins == B.Coins && SameItems(A.Items, B.Items) && SameItems(A.Storage, B.Storage) && A.TabNames == B.TabNames;
	}

	UNWBackend* BackendOf(const ANWCharacter* Who)
	{
		const UGameInstance* GI = Who && Who->GetWorld() ? Who->GetWorld()->GetGameInstance() : nullptr;
		UNWBackend* B = GI ? GI->GetSubsystem<UNWBackend>() : nullptr;
		return B && B->IsServerEnabled() ? B : nullptr;
	}

	void RunAll(TArray<TFunction<void()>>& Fns)
	{
		TArray<TFunction<void()>> Now = MoveTemp(Fns);
		Fns.Reset();
		for (TFunction<void()>& F : Now)
		{
			F();
		}
	}

	void Pump(TWeakObjectPtr<ANWCharacter> WeakWho);

	void Finish(TWeakObjectPtr<ANWCharacter> WeakWho, TWeakObjectPtr<ANWPlayerController> WeakPC, FOp Op, const FNWOpState& S1,
		const FNWCharacterRecord& R1, TArray<TFunction<void()>> Held, NWOps::EResult Res)
	{
		FQueue* Q = Queues.Find(WeakWho);
		if (!Q)
		{
			return;
		}
		Q->bBusy = false;
		ANWCharacter* Who = WeakWho.Get();
		ANWPlayerController* PC = WeakPC.Get();
		bool bOk = Res == NWOps::EResult::Ok;
		if (Res == NWOps::EResult::Retry && Op.Retries < 1)
		{
			// O nw-core pediu para refazer em cima do estado novo (armazém mudou): roda a mesma operação de novo.
			++Op.Retries;
			Q->Pending.Insert(MoveTemp(Op), 0);
			Pump(WeakWho);
			return;
		}
		if (bOk)
		{
			if (Who)
			{
				NWOps::Restore(Who, S1);
			}
			if (PC)
			{
				PC->Record = R1;   // um salvamento depois (sair, cair) já parte do estado gravado
			}
			RunAll(Held);
		}
		else
		{
			UE_LOG(LogNWOps, Warning, TEXT("operação %s não gravada: nada mudou"), *Op.Reason);
			if (PC && Res != NWOps::EResult::Rejected)
			{
				PC->Warning(NWText::Key(TEXT("nw.save.failed")));
			}
		}
		if (Op.Done)
		{
			Op.Done(bOk);
		}
		if (FQueue* Q2 = Queues.Find(WeakWho))
		{
			RunAll(Q2->Deferred);
		}
		Pump(WeakWho);
	}

	void Pump(TWeakObjectPtr<ANWCharacter> WeakWho)
	{
		while (true)
		{
			FQueue* Q = Queues.Find(WeakWho);
			if (!Q || Q->bBusy)
			{
				return;
			}
			if (Q->Pending.Num() == 0)
			{
				RunAll(Q->Deferred);
				RunAll(Q->AfterFns);
				if (FQueue* Q2 = Queues.Find(WeakWho); Q2 && !Q2->bBusy && Q2->Pending.Num() == 0 && Q2->Deferred.Num() == 0 && Q2->AfterFns.Num() == 0)
				{
					Queues.Remove(WeakWho);
				}
				return;
			}
			FOp Op = MoveTemp(Q->Pending[0]);
			Q->Pending.RemoveAt(0);
			ANWCharacter* Who = WeakWho.Get();
			if (!Who)
			{
				continue;
			}
			ANWPlayerController* PC = Cast<ANWPlayerController>(Who->GetController());
			UNWBackend* B = BackendOf(Who);
			const bool bPersist = B && PC && PC->Record;

			const FNWOpState S0 = NWOps::Capture(Who);
			Q->bApplying = true;
			Q->Held.Reset();
			Q->Over.Reset();
			const bool bChanged = Op.Apply();
			Q = Queues.Find(WeakWho);
			Q->bApplying = false;
			TArray<TFunction<void()>> Held = MoveTemp(Q->Held);
			TArray<FNWItem> Over = MoveTemp(Q->Over);
			const FVector OverAt = Q->OverAt;
			Q->Held.Reset();
			Q->Over.Reset();
			if (!bChanged)
			{
				NWOps::Restore(Who, S0);
				if (Op.Done)
				{
					Op.Done(false);
				}
				continue;
			}
			if (!bPersist)
			{
				// Sem nw-core: vale na hora.
				RunAll(Held);
				if (Over.Num() > 0 && ANWLootBag::Drop(Who, Over, false) && PC)
				{
					PC->Warning(NWText::Key(TEXT("nw.inv.bag_full_ground")));
				}
				if (Op.Done)
				{
					Op.Done(true);
				}
				continue;
			}
			// Resultado copiado e estado de volta ao de antes no mesmo ciclo (o cliente não vê nada até gravar).
			const FNWOpState S1 = NWOps::Capture(Who);
			if (Over.Num() == 0 && SameState(S0, S1))
			{
				// Nada de item, moeda ou armazém mudou: não há o que gravar.
				RunAll(Held);
				if (Op.Done)
				{
					Op.Done(true);
				}
				continue;
			}
			FNWCharacterRecord R1 = *PC->Record;
			Who->CaptureTo(R1);
			NWOps::Restore(Who, S0);
			Q->bBusy = true;
			TWeakObjectPtr<ANWPlayerController> WeakPC(PC);
			if (Over.Num() == 0)
			{
				FOp Copy = Op;
				Op.Commit(R1, S1, [WeakWho, WeakPC, Copy, S1, R1, Held](NWOps::EResult Res) mutable
				{
					Finish(WeakWho, WeakPC, MoveTemp(Copy), S1, R1, MoveTemp(Held), Res);
				});
				return;
			}
			// Excesso para o chão: o saco e a bolsa numa transação (ground), depois o resto da operação.
			for (int32 i = 0; i < Over.Num(); ++i)
			{
				Over[i].Slot = i;
			}
			const FString Map = UWorld::RemovePIEPrefix(Who->GetWorld()->GetMapName());
			FOp Copy = Op;
			B->DropGround(R1, Map, OverAt, int32(ANWLootBag::LifeSeconds), Over, Op.Reason, [WeakWho, WeakPC, Copy, S1, R1, Held, Over, OverAt](int32 Status, const UNWBackend::FGroundBag& Bag) mutable
			{
				if (Status != 201)
				{
					Finish(WeakWho, WeakPC, MoveTemp(Copy), S1, R1, MoveTemp(Held), NWOps::EResult::Failed);
					return;
				}
				// O saco já está gravado (com a bolsa sem os itens): ele aparece mesmo se o resto falhar.
				Held.Add([WeakPC, Over, OverAt, Id = Bag.Id, Life = Bag.ExpiresIn]()
				{
					if (ANWPlayerController* P = WeakPC.Get())
					{
						ANWLootBag::Spawn(P, OverAt, Over, Id, Life > 1.0 ? float(Life) : ANWLootBag::LifeSeconds);
					}
				});
				FOp Inner = Copy;
				Copy.Commit(R1, S1, [WeakWho, WeakPC, Inner, S1, R1, Held](NWOps::EResult Res) mutable
				{
					if (Res != NWOps::EResult::Ok)
					{
						UE_LOG(LogNWOps, Error, TEXT("operação %s: saco gravado, resto não (%d); vale o que foi gravado"), *Inner.Reason, int32(Res));
					}
					Finish(WeakWho, WeakPC, MoveTemp(Inner), S1, R1, MoveTemp(Held), NWOps::EResult::Ok);
				});
			});
			return;
		}
	}
}

namespace NWOps
{
	FNWOpState Capture(const ANWCharacter* Who)
	{
		FNWOpState S;
		if (Who && Who->Inventory)
		{
			S.Items = Who->Inventory->Items;
			S.Coins = Who->Inventory->Coins;
		}
		if (Who && Who->Interactor)
		{
			S.Storage = Who->Interactor->StorageItems;
			S.TabNames = Who->Interactor->StorageTabNames;
			S.bHasStorage = true;
		}
		return S;
	}

	void Restore(ANWCharacter* Who, const FNWOpState& S)
	{
		if (Who && Who->Inventory)
		{
			Who->Inventory->Items = S.Items;
			Who->Inventory->Coins = S.Coins;
			Who->Inventory->Changed();
		}
		if (Who && Who->Interactor && S.bHasStorage)
		{
			Who->Interactor->StorageItems = S.Storage;
			Who->Interactor->StorageTabNames = S.TabNames;
		}
	}

	void Run(ANWCharacter* Who, const FString& Reason, TFunction<bool()> Apply, FCommit Commit, TFunction<void(bool)> Done)
	{
		if (!Who || !Who->HasAuthority())
		{
			return;
		}
		Queues.FindOrAdd(Who).Pending.Add({Reason, MoveTemp(Apply), MoveTemp(Commit), MoveTemp(Done)});
		Pump(Who);
	}

	FCommit CommitState(ANWCharacter* Who, const FString& Reason, TFunction<TSharedPtr<FJsonObject>()> Extra)
	{
		TWeakObjectPtr<ANWCharacter> WeakWho(Who);
		return [WeakWho, Reason, Extra](const FNWCharacterRecord& After, const FNWOpState&, TFunction<void(EResult)> Fin)
		{
			UNWBackend* B = BackendOf(WeakWho.Get());
			if (!B)
			{
				Fin(EResult::Failed);
				return;
			}
			B->SaveState(After, Reason, [Fin](int32 Status) { Fin(Status == 204 || Status == 208 ? EResult::Ok : EResult::Failed); },
				Extra ? Extra() : nullptr);
		};
	}

	FCommit CommitState(ANWCharacter* Who, const FString& Reason)
	{
		TWeakObjectPtr<ANWCharacter> WeakWho(Who);
		return [WeakWho, Reason](const FNWCharacterRecord& After, const FNWOpState&, TFunction<void(EResult)> Fin)
		{
			UNWBackend* B = BackendOf(WeakWho.Get());
			if (!B)
			{
				Fin(EResult::Failed);
				return;
			}
			B->SaveState(After, Reason, [Fin](int32 Status) { Fin(Status == 204 || Status == 208 ? EResult::Ok : EResult::Failed); });
		};
	}

	bool Persists(const ANWCharacter* Who)
	{
		const ANWPlayerController* PC = Who ? Cast<ANWPlayerController>(Who->GetController()) : nullptr;
		return BackendOf(Who) && PC && PC->Record;
	}

	FReason::FReason(const FString& InReason) : Previous(ReasonScope)
	{
		ReasonScope = InReason;
	}

	FReason::~FReason()
	{
		ReasonScope = Previous;
	}

	FString CurrentReason(const TCHAR* Default)
	{
		return ReasonScope.IsEmpty() ? FString(Default) : ReasonScope;
	}

	bool IsBusy(const ANWCharacter* Who)
	{
		const FQueue* Q = Who ? Queues.Find(const_cast<ANWCharacter*>(Who)) : nullptr;
		return Q && Q->bBusy;
	}

	bool IsApplying(const ANWCharacter* Who)
	{
		const FQueue* Q = Who ? Queues.Find(const_cast<ANWCharacter*>(Who)) : nullptr;
		return Q && Q->bApplying;
	}

	bool Defer(ANWCharacter* Who, TFunction<void()> Fn)
	{
		FQueue* Q = Who ? Queues.Find(Who) : nullptr;
		if (!Q || !Q->bBusy)
		{
			return false;
		}
		Q->Deferred.Add(MoveTemp(Fn));
		return true;
	}

	void Hold(ANWCharacter* Who, TFunction<void()> Fn)
	{
		FQueue* Q = Who ? Queues.Find(Who) : nullptr;
		if (Q && Q->bApplying)
		{
			Q->Held.Add(MoveTemp(Fn));
			return;
		}
		Fn();
	}

	void Overflow(ANWCharacter* Who, const TArray<FNWItem>& Items)
	{
		FQueue* Q = Who ? Queues.Find(Who) : nullptr;
		if (!Q || !Q->bApplying)
		{
			ANWLootBag::Drop(Who, Items, false);
			return;
		}
		if (Q->Over.Num() == 0)
		{
			Q->OverAt = ANWLootBag::DropPoint(Who);
		}
		Q->Over.Append(Items);
	}

	void After(ANWCharacter* Who, TFunction<void()> Fn)
	{
		FQueue* Q = Who ? Queues.Find(Who) : nullptr;
		if (!Q || (!Q->bBusy && Q->Pending.Num() == 0))
		{
			Fn();
			return;
		}
		Q->AfterFns.Add(MoveTemp(Fn));
	}
}

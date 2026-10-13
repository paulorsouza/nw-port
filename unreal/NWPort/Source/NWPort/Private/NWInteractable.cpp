#include "NWInteractable.h"

#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "NWBackend.h"
#include "NWCrafting.h"
#include "NWCharacter.h"
#include "NWInventoryComponent.h"
#include "NWItems.h"
#include "NWNpc.h"
#include "NWNpcRewards.h"
#include "NWPlayerController.h"
#include "NWRules.h"
#include "NWShop.h"
#include "NWGameMode.h"
#include "NWProgressionComponent.h"
#include "NWText.h"

namespace
{
	TArray<TWeakObjectPtr<UNWInteractableComponent>> GInteractables;
}

UNWInteractableComponent::UNWInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;   // SetActive(false) tira do foco (ex. NPC ambiente)
}

void UNWInteractableComponent::OnRegister()
{
	Super::OnRegister();
	GInteractables.AddUnique(this);
}

void UNWInteractableComponent::OnUnregister()
{
	GInteractables.RemoveAll([this](const TWeakObjectPtr<UNWInteractableComponent>& P) { return !P.IsValid() || P.Get() == this; });
	Super::OnUnregister();
}

const TArray<TWeakObjectPtr<UNWInteractableComponent>>& UNWInteractableComponent::All()
{
	return GInteractables;
}

FVector UNWInteractableComponent::FocusLocation() const
{
	return GetOwner() ? GetOwner()->GetActorLocation() + FVector(0.f, 0.f, FocusHeight) : FVector::ZeroVector;
}

UNWInteractorComponent::UNWInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(true);
}

void UNWInteractorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UNWInteractorComponent, RewardClaims, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UNWInteractorComponent, StorageLocation, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UNWInteractorComponent, StorageItems, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UNWInteractorComponent, StorageCapacity, COND_OwnerOnly);
}

namespace
{
	UNWBackend* BackendOf(const UActorComponent* C)
	{
		const UGameInstance* GI = C->GetWorld() ? C->GetWorld()->GetGameInstance() : nullptr;
		UNWBackend* B = GI ? GI->GetSubsystem<UNWBackend>() : nullptr;
		return B && B->IsServerEnabled() ? B : nullptr;
	}

	ANWPlayerController* ControllerOf(const UActorComponent* C)
	{
		const APawn* Pawn = Cast<APawn>(C->GetOwner());
		return Pawn ? Cast<ANWPlayerController>(Pawn->GetController()) : nullptr;
	}
}

void UNWInteractorComponent::LoadClaims()
{
	// Servidor: o que o personagem já pegou nesta semana (nw-core). Sem backend (PIE), começa vazio e fica na memória.
	const FString Period = FNWNpcRewards::Get().Period();
	if (bClaimsLoading || (bClaimsLoaded && ClaimsPeriod == Period))
	{
		return;
	}
	UNWBackend* B = BackendOf(this);
	ANWPlayerController* PC = ControllerOf(this);
	if (!B)
	{
		bClaimsLoaded = true;
		ClaimsPeriod = Period;
		return;
	}
	if (!PC || !PC->Record)
	{
		return;
	}
	bClaimsLoading = true;
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	B->ListNpcRewards(PC->Record->Id, Period, [Self, Period](bool bOk, const TArray<FString>& Keys)
	{
		if (!Self.IsValid())
		{
			return;
		}
		Self->bClaimsLoading = false;
		if (bOk)
		{
			Self->RewardClaims = Keys;
			Self->bClaimsLoaded = true;
			Self->ClaimsPeriod = Period;
		}
	});
}

void UNWInteractorComponent::ServerNpcInteract_Implementation(const FString& Variant)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	LoadClaims();
	if (!Me || !bClaimsLoaded || bClaimBusy)
	{
		return;
	}
	const ANWNpc* Npc = nullptr;
	for (TActorIterator<ANWNpc> It(GetWorld()); It; ++It)
	{
		if (It->Variant == Variant)
		{
			Npc = *It;
			break;
		}
	}
	// Distância conferida no servidor (o cliente só pede).
	if (!Npc || FVector::Dist2D(Npc->GetActorLocation(), Me->GetActorLocation()) > 400.f)
	{
		return;
	}
	FNWNpcRewards& R = FNWNpcRewards::Get();
	if (R.StateFor(Npc, RewardClaims) != FNWNpcRewards::EState::Available)
	{
		return;
	}
	FString Kind, NpcKey, Period;
	TArray<FNWItem> Items;
	if (R.HasStarter(Npc->Type) && !RewardClaims.Contains(FNWNpcRewards::Key(TEXT("starter"), FNWNpcRewards::StarterNpc(Npc->Type), TEXT("once"))))
	{
		Kind = TEXT("starter");
		NpcKey = FNWNpcRewards::StarterNpc(Npc->Type);
		Period = TEXT("once");
		R.Starter(Npc->Type, Items);
	}
	else
	{
		Kind = TEXT("weekly");
		NpcKey = FNWNpcRewards::WeeklyNpc(Npc->City, Npc->Type);
		Period = R.Period();
		R.Weekly(Npc->Type, Items);
	}
	const FString ClaimKey = FNWNpcRewards::Key(Kind, NpcKey, Period);
	Me->Inventory->Grant(Items, 0);
	RewardClaims.AddUnique(ClaimKey);
	UE_LOG(LogTemp, Log, TEXT("Brinde %s de %s: %d itens"), *ClaimKey, *Variant, Items.Num());
	// nw-core: grava a coleta junto com a bolsa (uma transação). 409 = já pego em outra sessão: só registra no log.
	UNWBackend* B = BackendOf(this);
	ANWPlayerController* PC = ControllerOf(this);
	if (B && PC && PC->Record)
	{
		Me->CaptureTo(*PC->Record);
		bClaimBusy = true;
		TWeakObjectPtr<UNWInteractorComponent> Self(this);
		B->ClaimNpcReward(*PC->Record, NpcKey, Kind, Period, [Self, ClaimKey](bool bOk, int32 Status)
		{
			if (Self.IsValid())
			{
				Self->bClaimBusy = false;
			}
			if (!bOk)
			{
				UE_LOG(LogTemp, Warning, TEXT("Brinde %s: nw-core respondeu %d"), *ClaimKey, Status);
			}
		});
	}
}

void UNWInteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APawn* Me = Cast<APawn>(GetOwner());
	if (Me && GetOwnerRole() == ROLE_Authority && Me->IsPlayerControlled())
	{
		LoadClaims();   // servidor: carrega (e renova na terça) o que já foi pego, para o ícone dos NPCs
	}
	if (!Me || !Me->IsLocallyControlled() || !Me->IsPlayerControlled())
	{
		Focus = nullptr;
		return;
	}
	// O mais perto dentro do alcance, à frente da câmera (até ~70° para o lado).
	const FVector From = Me->GetActorLocation();
	const FVector Fwd = Me->GetControlRotation().Vector().GetSafeNormal2D();
	UNWInteractableComponent* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<UNWInteractableComponent>& P : UNWInteractableComponent::All())
	{
		UNWInteractableComponent* I = P.Get();
		if (!I || !I->IsActive() || I->GetWorld() != GetWorld())
		{
			continue;
		}
		const FVector To = I->GetOwner()->GetActorLocation() - From;
		const float Dist = To.Size2D();
		if (Dist > I->Radius || Dist >= BestDist || FMath::Abs(To.Z) > 250.f)
		{
			continue;
		}
		if (Dist > 80.f && FVector::DotProduct(Fwd, To.GetSafeNormal2D()) < 0.35f)
		{
			continue;
		}
		Best = I;
		BestDist = Dist;
	}
	Focus = Best;
}

bool UNWInteractorComponent::InteractPressed()
{
	UNWInteractableComponent* I = Focus.Get();
	if (!I)
	{
		return false;
	}
	if (I->OnInteract)
	{
		I->OnInteract(Cast<ANWCharacter>(GetOwner()));
	}
	return true;
}

void UNWInteractorComponent::ServerRefine_Implementation(const FString& StationType, FName Recipe, int32 Count)
{
	const FString Error = FNWCrafting::Get().Refine(Cast<ANWCharacter>(GetOwner()), StationType, Recipe, Count);
	if (!Error.IsEmpty())
	{
		if (ANWPlayerController* PC = ControllerOf(this))
		{
			PC->Warning(NWText::Key(Error));
		}
	}
}

// ---------------------------------------------------------------- fabricar (docs/oficio.md)

void UNWInteractorComponent::ServerCraft_Implementation(const FString& StationType, FName Recipe)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	FNWItem Signed;   // o item sorteado (vazio no treino); assinado só com Signer
	const FString Error = FNWCrafting::Get().Craft(Me, StationType, Recipe, &Signed);
	ANWPlayerController* PC = ControllerOf(this);
	if (!Error.IsEmpty())
	{
		if (PC)
		{
			PC->Warning(NWText::Key(Error));
		}
		return;
	}
	auto Finish = [](UNWInteractorComponent* Self, const FNWItem& Item)
	{
		ANWCharacter* Who = Cast<ANWCharacter>(Self->GetOwner());
		ANWPlayerController* PC2 = ControllerOf(Self);
		if (Who && Who->Inventory && !Item.ItemId.IsNone())
		{
			Who->Inventory->Grant({Item}, 0);
			Self->ClientCraftResult(FNWItem(), Item, 255);
		}
		if (ANWGameMode* GM = Self->GetWorld()->GetAuthGameMode<ANWGameMode>(); GM && PC2)
		{
			GM->SavePlayer(PC2);
		}
	};
	if (Signed.ItemId.IsNone() || Signed.Signer.IsEmpty())
	{
		Finish(this, Signed);
		return;
	}
	// Item assinado: número de série do nw-core (sem backend, um local). Sem o número, sai sem assinatura.
	UNWBackend* B = BackendOf(this);
	if (!B || !PC || !PC->Record)
	{
		Signed.Serial = TEXT("local-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		Finish(this, Signed);
		return;
	}
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	B->IssueSerial(PC->Record->Id, [Self, Signed, Finish](bool bOk, const FString& Serial) mutable
	{
		if (!Self.IsValid())
		{
			return;
		}
		if (bOk)
		{
			Signed.Serial = Serial;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Fabricar: sem número de série do nw-core; item sai sem assinatura"));
			Signed.Signer.Reset();
		}
		Finish(Self.Get(), Signed);
	});
}

void UNWInteractorComponent::ServerImprove_Implementation(const FString& StationType, int32 BagSlot, uint8 Kind)
{
	if (Kind >= uint8(ENWImprove::Num))
	{
		return;
	}
	FNWItem Before, After;
	const FString Error = FNWCrafting::Get().Improve(Cast<ANWCharacter>(GetOwner()), StationType, BagSlot, ENWImprove(Kind), &Before, &After);
	ANWPlayerController* PC = ControllerOf(this);
	if (!Error.IsEmpty())
	{
		if (PC)
		{
			PC->Warning(NWText::Key(Error));
		}
		return;
	}
	ClientCraftResult(Before, After, Kind);
	if (ANWGameMode* GM = GetWorld()->GetAuthGameMode<ANWGameMode>(); GM && PC)
	{
		GM->SavePlayer(PC);
	}
}

void UNWInteractorComponent::ClientCraftResult_Implementation(const FNWItem& Before, const FNWItem& After, uint8 Kind)
{
	CraftResult.Before = Before;
	CraftResult.After = After;
	CraftResult.Kind = Kind;
	++CraftResult.Seq;
}

// ---------------------------------------------------------------- armazém

float UNWInteractorComponent::StorageWeight() const
{
	float W = 0.f;
	for (const FNWItem& I : StorageItems)
	{
		if (const FNWItemDef* D = FNWItemCatalog::Get().Find(I.ItemId))
		{
			W += D->Weight * I.Quantity;
		}
	}
	return W;
}

void UNWInteractorComponent::ServerShopBuy_Implementation(const FString& ShopId, FName Item, int32 Quantity)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	const FNWShop* Shop = FNWShops::Get().Find(ShopId);
	const FNWShopItem* Offer = Shop ? Shop->Find(Item) : nullptr;
	const FNWItemDef* Def = FNWItemCatalog::Get().Find(Item);
	ANWPlayerController* PC = ControllerOf(this);
	if (!Me || !Me->Inventory || !Me->Progression || !Offer || !Def || Quantity <= 0)
	{
		return;
	}
	const ANWNpc* Npc = nullptr;
	for (TActorIterator<ANWNpc> It(GetWorld()); It; ++It)
	{
		if (It->Variant == Shop->Npc)
		{
			Npc = *It;
			break;
		}
	}
	if (!Npc || FVector::Dist2D(Npc->GetActorLocation(), Me->GetActorLocation()) > 400.f)
	{
		return;
	}
	const FString Period = FNWNpcRewards::Get().Period();
	const int32 Bought = Me->Progression->ShopBought(Item, Period);
	if (Offer->WeeklyLimit > 0 && Bought + Quantity > Offer->WeeklyLimit)
	{
		if (PC)
		{
			PC->Warning(NWText::Key(TEXT("nw.shop.weekly_limit"), {FString::FromInt(Bought), FString::FromInt(Offer->WeeklyLimit)}));
		}
		return;
	}
	if (Me->Inventory->Coins < Offer->Price * Quantity)
	{
		if (PC)
		{
			PC->Warning(NWText::Key(TEXT("ui_insufficient_funds")));
		}
		return;
	}
	if (Me->Inventory->TotalWeight() + Def->Weight * Quantity > Me->Inventory->MaxWeight)
	{
		if (PC)
		{
			PC->Warning(NWText::Key(TEXT("ui_inventoryfull")));
		}
		return;
	}
	FNWItem New;
	New.ItemId = Item;
	New.Quantity = Quantity;
	const int32 Added = Quantity - Me->Inventory->AddItem(New);
	if (Added <= 0)
	{
		if (PC)
		{
			PC->Warning(NWText::Key(TEXT("ui_inventoryfull")));
		}
		return;
	}
	// Só paga o que entrou na bolsa (espaços cheios).
	Me->Inventory->AddCoins(-Offer->Price * Added);
	Me->Progression->AddShopBought(Item, Added, Period);
	New.Quantity = Added;
	Me->Inventory->ClientLoot({New}, 0);
	UE_LOG(LogTemp, Log, TEXT("Loja %s: %s x%d por %lld"), *ShopId, *Item.ToString(), Added, Offer->Price * Added);
	// Bolsa, moedas e o limite da semana vão ao nw-core na hora (PUT state).
	if (ANWGameMode* GM = GetWorld()->GetAuthGameMode<ANWGameMode>())
	{
		GM->SavePlayer(PC);
	}
}

void UNWInteractorComponent::StorageAdd(TArray<FNWItem>& Items, const FNWItem& Item)
{
	for (FNWItem& I : Items)
	{
		if (I.SameKind(Item))
		{
			I.Quantity += Item.Quantity;
			return;
		}
	}
	FNWItem New = Item;
	New.Slot = 0;
	for (const FNWItem& I : Items)
	{
		New.Slot = FMath::Max(New.Slot, I.Slot + 1);
	}
	New.Container = ENWItemContainer::Bag;
	Items.Add(New);
}

bool UNWInteractorComponent::StorageRemove(TArray<FNWItem>& Items, const FNWItem& Item)
{
	for (FNWItem& I : Items)
	{
		if (I.SameKind(Item) && I.Quantity >= Item.Quantity)
		{
			I.Quantity -= Item.Quantity;
			Items.RemoveAll([](const FNWItem& X) { return X.Quantity <= 0; });
			return true;
		}
	}
	return false;
}

void UNWInteractorComponent::ServerStorageOpen_Implementation(const FString& Location)
{
	const ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	bool bNear = false;
	for (TActorIterator<ANWStation> It(GetWorld()); It && Me; ++It)
	{
		bNear |= It->Type == TEXT("storage") && FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation()) < 500.f;
	}
	if (!bNear || bStorageBusy)
	{
		return;
	}
	StorageLocation = Location;
	// storage.base_weight (R, sistemas-002): StorageMedium = 10000 décimos; o port guarda peso em kg. O acréscimo por
	// standing (storage.per_point) e os baús da casa entram quando houver standing e casas.
	StorageCapacity = float(FNWRules::Num(TEXT("storage.base_weight"), 10000.0) / 10.0);
	UNWBackend* B = BackendOf(this);
	ANWPlayerController* PC = ControllerOf(this);
	if (!B || !PC || !PC->Record)
	{
		StorageItems = MemStorage.FindOrAdd(Location);
		StorageVersion = 0;
		return;
	}
	bStorageBusy = true;
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	B->GetStorage(PC->Record->Id, Location, [Self, Location](bool bOk, int64 Version, const TArray<FNWItem>& Items)
	{
		if (Self.IsValid())
		{
			Self->bStorageBusy = false;
			if (bOk && Self->StorageLocation == Location)
			{
				Self->StorageItems = Items;
				Self->StorageVersion = Version;
			}
		}
	});
}

void UNWInteractorComponent::ServerStorageClose_Implementation()
{
	StorageLocation.Reset();
	StorageItems.Reset();
}

void UNWInteractorComponent::ServerStorageMove_Implementation(bool bDeposit, int32 Slot, int32 Quantity)
{
	if (StorageLocation.IsEmpty() || bStorageBusy || Quantity <= 0)
	{
		return;
	}
	const FNWItem Moved = StorageApply(bDeposit, Slot, Quantity, true);
	if (Moved.Quantity > 0)
	{
		StorageSave({Moved}, bDeposit, true);
	}
}

void UNWInteractorComponent::ServerStorageMoveMany_Implementation(bool bDeposit, const TArray<int32>& Slots)
{
	if (StorageLocation.IsEmpty() || bStorageBusy || Slots.Num() == 0)
	{
		return;
	}
	TArray<FNWItem> Moved;
	bool bWarned = false;
	for (int32 Slot : Slots)
	{
		const FNWItem M = StorageApply(bDeposit, Slot, MAX_int32, !bWarned);
		if (M.Quantity > 0)
		{
			Moved.Add(M);
		}
		else
		{
			bWarned = true;   // um aviso só (armazém ou bolsa cheia)
		}
	}
	if (Moved.Num() > 0)
	{
		StorageSave(Moved, bDeposit, true);
	}
}

FNWItem UNWInteractorComponent::StorageApply(bool bDeposit, int32 Slot, int32 Quantity, bool bWarn)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	FNWItem Moved;
	Moved.Quantity = 0;
	if (!Me || !Me->Inventory)
	{
		return Moved;
	}
	if (bDeposit)
	{
		const FNWItem* In = Me->Inventory->At(ENWItemContainer::Bag, Slot);
		const FNWItemDef* D = In ? FNWItemCatalog::Get().Find(In->ItemId) : nullptr;
		if (!In)
		{
			return Moved;
		}
		// Cabe no armazém só o que o peso deixa.
		const float Unit = D ? D->Weight : 0.f;
		const int32 Want = FMath::Min(Quantity, In->Quantity);
		const int32 Fits = Unit > 0.f ? FMath::FloorToInt((StorageCapacity - StorageWeight()) / Unit) : Want;
		const int32 N = FMath::Min(Want, Fits);
		if (N <= 0)
		{
			if (bWarn)
			{
				if (ANWPlayerController* PC = ControllerOf(this))
				{
					PC->Warning(NWText::Key(TEXT("ui_storage_is_full")));
				}
			}
			return Moved;
		}
		Moved = Me->Inventory->TakeAt(Slot, N);
		StorageAdd(StorageItems, Moved);
		return Moved;
	}
	const FNWItem* In = StorageItems.FindByPredicate([Slot](const FNWItem& I) { return I.Slot == Slot; });
	if (!In)
	{
		return Moved;
	}
	Moved = *In;
	Moved.Quantity = FMath::Min(Quantity, In->Quantity);
	const int32 Left = Me->Inventory->AddItem(Moved);   // o que não coube na bolsa fica no armazém
	Moved.Quantity -= Left;
	if (Moved.Quantity > 0)
	{
		StorageRemove(StorageItems, Moved);
	}
	return Moved;
}

void UNWInteractorComponent::StorageSave(const TArray<FNWItem>& Moved, bool bDeposit, bool bRetry)
{
	UNWBackend* B = BackendOf(this);
	ANWPlayerController* PC = ControllerOf(this);
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!B || !PC || !PC->Record || !Me)
	{
		MemStorage.Add(StorageLocation, StorageItems);
		return;
	}
	// nw-core: bolsa e armazém numa transação, com a versão lida. 409 = o armazém mudou (mercado entregou): lê de
	// novo, refaz a mesma mudança em cima do novo e grava uma vez mais.
	Me->CaptureTo(*PC->Record);
	bStorageBusy = true;
	const FString Location = StorageLocation;
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	B->PutStorage(*PC->Record, Location, StorageVersion, StorageItems, [Self, Location, Moved, bDeposit, bRetry](int32 Status, int64 NewVersion)
	{
		if (!Self.IsValid())
		{
			return;
		}
		Self->bStorageBusy = false;
		if (Status == 200)
		{
			Self->StorageVersion = NewVersion;
			return;
		}
		UNWBackend* B2 = BackendOf(Self.Get());
		ANWPlayerController* PC2 = ControllerOf(Self.Get());
		if (Status != 409 || !bRetry || !B2 || !PC2 || !PC2->Record)
		{
			UE_LOG(LogTemp, Warning, TEXT("Armazém %s: nw-core respondeu %d"), *Location, Status);
			return;
		}
		Self->bStorageBusy = true;
		B2->GetStorage(PC2->Record->Id, Location, [Self, Location, Moved, bDeposit](bool bOk, int64 Version, const TArray<FNWItem>& Items)
		{
			if (!Self.IsValid())
			{
				return;
			}
			Self->bStorageBusy = false;
			if (!bOk || Self->StorageLocation != Location)
			{
				return;
			}
			Self->StorageItems = Items;
			Self->StorageVersion = Version;
			for (const FNWItem& M : Moved)
			{
				if (bDeposit)
				{
					StorageAdd(Self->StorageItems, M);
				}
				else
				{
					StorageRemove(Self->StorageItems, M);
				}
			}
			Self->StorageSave(Moved, bDeposit, false);
		});
	});
}

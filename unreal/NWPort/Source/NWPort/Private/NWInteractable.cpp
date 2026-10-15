#include "NWInteractable.h"
#include "NWLootBag.h"

#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "NWBackend.h"
#include "NWCity.h"
#include "NWCrafting.h"
#include "NWCharacter.h"
#include "NWCombatComponent.h"
#include "NWInventoryComponent.h"
#include "NWItems.h"
#include "NWItemRules.h"
#include "NWNpc.h"
#include "NWNpcRewards.h"
#include "NWPlayerController.h"
#include "NWRules.h"
#include "NWShop.h"
#include "NWGameMode.h"
#include "NWProgressionComponent.h"
#include "NWText.h"
#include "NWOps.h"

namespace
{
	TArray<TWeakObjectPtr<UNWInteractableComponent>> GInteractables;
}

UNWInteractableComponent::UNWInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;   // SetActive(false) removes it from focus (e.g. ambient NPC)
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
	DOREPLIFETIME_CONDITION(UNWInteractorComponent, StorageTabNames, COND_OwnerOnly);
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
	// Server: what the character already picked up this week (nw-core). No backend (PIE): starts empty, kept in memory.
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

void UNWInteractorComponent::ServerLootTake_Implementation(ANWLootBag* Bag)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!Me || !Bag || Bag->GetOwner() != Me->GetController()
		|| FVector::Dist(Bag->GetActorLocation(), Me->GetActorLocation()) > Bag->Interact->Radius + 200.f)
	{
		return;   // owner only, from up close (2 m margin for the character's height)
	}
	Bag->TakeAll(Me);
}

void UNWInteractorComponent::ServerLootTakeItem_Implementation(ANWLootBag* Bag, int32 Slot, int32 Quantity)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!Me || !Bag || Bag->GetOwner() != Me->GetController()
		|| FVector::Dist(Bag->GetActorLocation(), Me->GetActorLocation()) > Bag->Interact->Radius + 200.f)
	{
		return;
	}
	Bag->Take(Me, Slot, Quantity);
}

void UNWInteractorComponent::ServerNpcInteract_Implementation(const FString& Variant)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	TWeakObjectPtr<UNWInteractorComponent> WeakSelf(this);
	if (NWOps::Defer(Me, [WeakSelf, Variant]() { if (WeakSelf.IsValid()) { WeakSelf->ServerNpcInteract_Implementation(Variant); } }))
	{
		return;   // the gift writes the bag: waits for the operation in progress
	}
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
	// Distance checked on the server (the client only asks).
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
	int64 Coins = 0;
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
		R.Weekly(Npc->Type, Items, &Coins);
	}
	const FString ClaimKey = FNWNpcRewards::Key(Kind, NpcKey, Period);
	UE_LOG(LogTemp, Log, TEXT("Gift %s from %s: %d items"), *ClaimKey, *Variant, Items.Num());
	// Written operation: the gift and the bag with it in one transaction (npc-rewards); 409 = already taken in another
	// session, nothing goes in. Without nw-core, it goes in right away.
	bClaimBusy = true;
	NWOps::Run(Me, TEXT("npc_reward:") + ClaimKey,
		[WeakSelf, Me, Items, Coins]()
		{
			Me->Inventory->Grant(Items, Coins);
			return WeakSelf.IsValid();
		},
		[WeakSelf, NpcKey, Kind, Period, ClaimKey](const FNWCharacterRecord& After, const FNWOpState&, TFunction<void(NWOps::EResult)> Fin)
		{
			UNWBackend* B = WeakSelf.IsValid() ? BackendOf(WeakSelf.Get()) : nullptr;
			if (!B)
			{
				Fin(NWOps::EResult::Failed);
				return;
			}
			B->ClaimNpcReward(After, NpcKey, Kind, Period, [Fin, ClaimKey](bool bOk, int32 Status)
			{
				if (!bOk)
				{
					UE_LOG(LogTemp, Warning, TEXT("Gift %s: nw-core answered %d"), *ClaimKey, Status);
				}
				Fin(bOk ? NWOps::EResult::Ok : NWOps::EResult::Failed);
			});
		},
		[WeakSelf, ClaimKey](bool bOk)
		{
			if (WeakSelf.IsValid())
			{
				WeakSelf->bClaimBusy = false;
				if (bOk)
				{
					WeakSelf->RewardClaims.AddUnique(ClaimKey);
				}
			}
		});
}

void UNWInteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APawn* Me = Cast<APawn>(GetOwner());
	if (Me && GetOwnerRole() == ROLE_Authority && Me->IsPlayerControlled())
	{
		LoadClaims();   // server: loads (and renews on Tuesday) what was already picked up, for the NPC icon
	}
	if (!Me || !Me->IsLocallyControlled() || !Me->IsPlayerControlled())
	{
		Focus = nullptr;
		return;
	}
	// The closest within range, in front of the camera (up to ~70° to the side).
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
	// Written operation (NWOps): material goes out and refined item goes in only after nw-core writes.
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	const FString Reason = FString::Printf(TEXT("refine:%s:%d"), *Recipe.ToString(), Count);
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	TSharedRef<FNWCityTaxNote> Tax = MakeShared<FNWCityTaxNote>();   // station city's tax, goes along in the write
	NWOps::Run(Me, Reason,
		[Self, Me, StationType, Recipe, Count, Tax]()
		{
			const FString Error = FNWCrafting::Get().Refine(Me, StationType, Recipe, Count, &Tax.Get());
			if (!Error.IsEmpty())
			{
				if (ANWPlayerController* PC = Self.IsValid() ? ControllerOf(Self.Get()) : nullptr)
				{
					PC->Warning(NWText::Key(Error));
				}
				return false;
			}
			return true;
		},
		NWOps::CommitState(Me, Reason, [Tax]() { return Tax->Json(); }));
}

// ---------------------------------------------------------------- crafting (docs/game/crafting.md)

void UNWInteractorComponent::ServerCraft_Implementation(const FString& StationType, FName Recipe)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	ANWPlayerController* PC = ControllerOf(this);
	UNWBackend* B = BackendOf(this);
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	// A signed item carries a nw-core serial number. The roll is inside the operation (synchronous): for weapon, shield
	// and tool the number is requested first (if the item does not come out signed, the number goes unused).
	auto Start = [Self, StationType, Recipe](const FString& Serial)
	{
		UNWInteractorComponent* C = Self.Get();
		ANWCharacter* Who = C ? Cast<ANWCharacter>(C->GetOwner()) : nullptr;
		if (!Who)
		{
			return;
		}
		const FString Reason = TEXT("craft:") + Recipe.ToString();
		TSharedRef<FNWCityTaxNote> Tax = MakeShared<FNWCityTaxNote>();
		NWOps::Run(Who, Reason,
			[Self, Who, StationType, Recipe, Serial, Tax]()
			{
				FNWItem Signed;   // the rolled item (empty in training); signed only with Signer
				const FString Error = FNWCrafting::Get().Craft(Who, StationType, Recipe, &Signed, &Tax.Get());
				if (!Error.IsEmpty())
				{
					if (ANWPlayerController* P = Self.IsValid() ? ControllerOf(Self.Get()) : nullptr)
					{
						P->Warning(NWText::Key(Error));
					}
					return false;
				}
				if (!Signed.ItemId.IsNone())
				{
					if (!Signed.Signer.IsEmpty())
					{
						Signed.Serial = Serial;
						if (Signed.Serial.IsEmpty())
						{
							UE_LOG(LogTemp, Warning, TEXT("Craft: no serial number from nw-core; item comes out unsigned"));
							Signed.Signer.Reset();
						}
					}
					Who->Inventory->Grant({Signed}, 0);
					NWOps::Hold(Who, [Self, Signed]() { if (Self.IsValid()) { Self->ClientCraftResult(FNWItem(), Signed, 255); } });
				}
				return true;
			},
			NWOps::CommitState(Who, Reason, [Tax]() { return Tax->Json(); }));
	};
	const FNWRecipe* R = FNWCrafting::Get().Find(Recipe);
	if (!B || !PC || !PC->Record)
	{
		Start(TEXT("local-") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
		return;
	}
	if (!R || !R->bGear)
	{
		Start(FString());
		return;
	}
	B->IssueSerial(PC->Record->Id, [Start](bool bOk, const FString& Serial) { Start(bOk ? Serial : FString()); });
}

void UNWInteractorComponent::ServerImprove_Implementation(const FString& StationType, int32 BagSlot, uint8 Kind, uint8 Mat)
{
	if (Kind >= uint8(ENWImprove::Num))
	{
		return;
	}
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	const FString Reason = FString::Printf(TEXT("improve:%d:%d"), Kind, BagSlot);
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	NWOps::Run(Me, Reason,
		[Self, Me, StationType, BagSlot, Kind, Mat]()
		{
			FNWItem Before, After;
			const FString Error = FNWCrafting::Get().Improve(Me, StationType, BagSlot, ENWImprove(Kind), Mat, &Before, &After);
			if (!Error.IsEmpty())
			{
				if (ANWPlayerController* PC = Self.IsValid() ? ControllerOf(Self.Get()) : nullptr)
				{
					PC->Warning(NWText::Key(Error));
				}
				return false;
			}
			NWOps::Hold(Me, [Self, Before, After, Kind]() { if (Self.IsValid()) { Self->ClientCraftResult(Before, After, Kind); } });
			return true;
		},
		NWOps::CommitState(Me, Reason));
}

void UNWInteractorComponent::ClientCraftResult_Implementation(const FNWItem& Before, const FNWItem& After, uint8 Kind)
{
	CraftResult.Before = Before;
	CraftResult.After = After;
	CraftResult.Kind = Kind;
	++CraftResult.Seq;
}

// ---------------------------------------------------------------- storage

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
	// Written operation (NWOps): the same function runs inside it; item, coins and limit apply after the write.
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	if (Me && !NWOps::IsApplying(Me) && NWOps::Persists(Me))
	{
		const FString Reason = FString::Printf(TEXT("shop:%s:%d"), *Item.ToString(), Quantity);
		NWOps::Run(Me, Reason, [Self, ShopId, Item, Quantity]() { if (Self.IsValid()) { Self->ServerShopBuy_Implementation(ShopId, Item, Quantity); } return Self.IsValid(); },
			NWOps::CommitState(Me, Reason));
		return;
	}
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
	// Only pays for what went into the bag (full slots).
	Me->Inventory->AddCoins(-Offer->Price * Added);
	Me->Progression->AddShopBought(Item, Added, Period);
	New.Quantity = Added;
	TWeakObjectPtr<UNWInventoryComponent> Inv(Me->Inventory);
	NWOps::Hold(Me, [Inv, New]() { if (Inv.IsValid()) { Inv->ClientLoot({New}, 0); } });
	UE_LOG(LogTemp, Log, TEXT("Shop %s: %s x%d for %lld"), *ShopId, *Item.ToString(), Added, Offer->Price * Added);
	// Bag, coins and the week's limit: written by the operation (above).
}

void UNWInteractorComponent::StorageAdd(TArray<FNWItem>& Items, const FNWItem& Item)
{
	for (FNWItem& I : Items)
	{
		if (I.Tab == Item.Tab && I.SameKind(Item))
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
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (FNWItem& I : Items)
		{
			if ((Pass == 1 || I.Tab == Item.Tab) && I.SameKind(Item) && I.Quantity >= Item.Quantity)
			{
				I.Quantity -= Item.Quantity;
				Items.RemoveAll([](const FNWItem& X) { return X.Quantity <= 0; });
				return true;
			}
		}
	}
	return false;
}

namespace
{
	const TCHAR* SharedStorage = TEXT("shared");
	constexpr int32 MaxTabName = 24;
}

float UNWInteractorComponent::StorageCapacityNow() const
{
	// docs/game/city.md "Storage": no base; each known city adds armazem_por_cidade.
	const ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	const int32 Cities = Me && Me->Progression ? Me->Progression->Cities.Num() : 0;
	return FNWItemRules::Get().StoragePerCity * float(Cities);
}

void UNWInteractorComponent::ServerStorageOpen_Implementation(const FString& City)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	const ANWStation* Near = nullptr;
	for (TActorIterator<ANWStation> It(GetWorld()); It && Me; ++It)
	{
		if (It->Type == TEXT("storage") && FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation()) < 500.f)
		{
			Near = *It;
			break;
		}
	}
	if (!Near || bStorageBusy)
	{
		return;
	}
	// Whoever reached the pantry is in the city: now knows it (the city area also marks it, ANWNpcField).
	if (Me->Progression && Me->Progression->DiscoverCity(Near->City.IsEmpty() ? City : Near->City))
	{
		if (ANWPlayerController* PC = ControllerOf(this))
		{
			PC->Warning(NWText::Key(TEXT("nw.storage.city_known"), {FString::FromInt(FMath::RoundToInt(FNWItemRules::Get().StoragePerCity))}));
		}
	}
	StorageLocation = SharedStorage;
	StorageCapacity = StorageCapacityNow();
	StorageTabNames.SetNum(FNWItemRules::Get().StorageTabs);
	UNWBackend* B = BackendOf(this);
	ANWPlayerController* PC = ControllerOf(this);
	if (!B || !PC || !PC->Record)
	{
		StorageItems = MemStorage;
		StorageTabNames = MemTabNames;
		StorageTabNames.SetNum(FNWItemRules::Get().StorageTabs);
		StorageVersion = 0;
		return;
	}
	bStorageBusy = true;
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	B->GetStorage(PC->Record->Id, SharedStorage, [Self](bool bOk, int64 Version, const TArray<FNWItem>& Items, const TArray<FString>& Names)
	{
		if (Self.IsValid())
		{
			Self->bStorageBusy = false;
			if (bOk && !Self->StorageLocation.IsEmpty())
			{
				Self->StorageItems = Items;
				Self->StorageTabNames = Names;
				Self->StorageTabNames.SetNum(FNWItemRules::Get().StorageTabs);
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

NWOps::FCommit UNWInteractorComponent::StorageCommit()
{
	// Storage and bag in one transaction, with the version read. 409 (market delivered in the meantime): rereads and
	// the operation is redone on top of the new one; 208 (already written, the response was lost): only the version.
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	return [Self](const FNWCharacterRecord& After, const FNWOpState& S1, TFunction<void(NWOps::EResult)> Fin)
	{
		UNWBackend* B = Self.IsValid() ? BackendOf(Self.Get()) : nullptr;
		if (!B)
		{
			Fin(NWOps::EResult::Failed);
			return;
		}
		B->PutStorage(After, SharedStorage, Self->StorageVersion, S1.Storage, S1.TabNames, Self->StorageCapacity, TEXT("storage"),
			[Self, Fin](int32 Status, int64 NewVersion)
		{
			if (!Self.IsValid())
			{
				Fin(NWOps::EResult::Failed);
				return;
			}
			if (Status == 200)
			{
				Self->StorageVersion = NewVersion;
				Fin(NWOps::EResult::Ok);
				return;
			}
			UNWBackend* B2 = BackendOf(Self.Get());
			ANWPlayerController* PC2 = ControllerOf(Self.Get());
			if ((Status != 208 && Status != 409) || !B2 || !PC2 || !PC2->Record)
			{
				UE_LOG(LogTemp, Warning, TEXT("Storage: nw-core answered %d"), Status);
				Fin(NWOps::EResult::Failed);
				return;
			}
			B2->GetStorage(PC2->Record->Id, SharedStorage, [Self, Fin, Status](bool bOk, int64 Version, const TArray<FNWItem>& Items, const TArray<FString>& Names)
			{
				if (!Self.IsValid() || !bOk)
				{
					Fin(NWOps::EResult::Failed);
					return;
				}
				Self->StorageVersion = Version;
				if (Status == 208)
				{
					Fin(NWOps::EResult::Ok);
					return;
				}
				Self->StorageItems = Items;
				Self->StorageTabNames = Names;
				Self->StorageTabNames.SetNum(FNWItemRules::Get().StorageTabs);
				Fin(NWOps::EResult::Retry);
			});
		});
	};
}

void UNWInteractorComponent::RunStorageOp(TFunction<bool(UNWInteractorComponent&)> Apply)
{
	if (StorageLocation.IsEmpty() || bStorageBusy)
	{
		return;
	}
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	NWOps::Run(Cast<ANWCharacter>(GetOwner()), TEXT("storage"),
		[Self, Apply]() { return Self.IsValid() && !Self->StorageLocation.IsEmpty() && Apply(*Self); },
		StorageCommit(),
		[Self](bool bOk)
		{
			// Without nw-core (Play without backend): the storage stays in the server memory.
			if (bOk && Self.IsValid() && !BackendOf(Self.Get()))
			{
				Self->MemStorage = Self->StorageItems;
				Self->MemTabNames = Self->StorageTabNames;
			}
		});
}

void UNWInteractorComponent::ServerStorageMove_Implementation(bool bDeposit, int32 Slot, int32 Quantity, uint8 Tab)
{
	if (Quantity > 0)
	{
		RunStorageOp([bDeposit, Slot, Quantity, Tab](UNWInteractorComponent& C) { return C.StorageApply(bDeposit, Slot, Quantity, true, Tab).Quantity > 0; });
	}
}

void UNWInteractorComponent::ServerStorageMoveMany_Implementation(bool bDeposit, const TArray<int32>& Slots, uint8 Tab)
{
	if (Slots.Num() == 0)
	{
		return;
	}
	RunStorageOp([bDeposit, Slots, Tab](UNWInteractorComponent& C)
	{
		bool bMoved = false, bWarned = false;
		for (int32 Slot : Slots)
		{
			const bool bOne = C.StorageApply(bDeposit, Slot, MAX_int32, !bWarned, Tab).Quantity > 0;
			bMoved |= bOne;
			bWarned |= !bOne;   // a single warning (storage or bag full)
		}
		return bMoved;
	});
}

void UNWInteractorComponent::ServerStorageSetTab_Implementation(int32 Slot, uint8 Tab)
{
	if (Tab >= FNWItemRules::Get().StorageTabs)
	{
		return;
	}
	RunStorageOp([Slot, Tab](UNWInteractorComponent& C)
	{
		const FNWItem* In = C.StorageItems.FindByPredicate([Slot](const FNWItem& I) { return I.Slot == Slot; });
		if (!In || In->Tab == Tab)
		{
			return false;
		}
		const FNWItem From = *In;
		FNWItem To = From;
		To.Tab = Tab;
		StorageRemove(C.StorageItems, From);
		StorageAdd(C.StorageItems, To);
		return true;
	});
}

void UNWInteractorComponent::ServerStorageRenameTab_Implementation(uint8 Tab, const FString& Name)
{
	if (Tab >= FNWItemRules::Get().StorageTabs)
	{
		return;
	}
	const FString Clean = Name.TrimStartAndEnd().Left(MaxTabName);
	RunStorageOp([Tab, Clean](UNWInteractorComponent& C)
	{
		C.StorageTabNames.SetNum(FNWItemRules::Get().StorageTabs);
		if (C.StorageTabNames[Tab] == Clean)
		{
			return false;
		}
		C.StorageTabNames[Tab] = Clean;
		return true;
	});
}

FNWItem UNWInteractorComponent::StorageApply(bool bDeposit, int32 Slot, int32 Quantity, bool bWarn, uint8 Tab)
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
		// Storage takes only what the weight allows; over capacity (the market delivers even when full) stores nothing.
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
		Moved.Tab = uint8(FMath::Clamp(int32(Tab), 0, FNWItemRules::Get().StorageTabs - 1));
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
	FNWItem ToBag = Moved;
	ToBag.Tab = 0;
	const int32 Left = Me->Inventory->AddItem(ToBag);   // what did not fit in the bag stays in the storage
	Moved.Quantity -= Left;
	if (Moved.Quantity > 0)
	{
		StorageRemove(StorageItems, Moved);
	}
	return Moved;
}

void UNWInteractorComponent::ServerFastTravel_Implementation()
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	ANWPlayerController* PC = Me ? Cast<ANWPlayerController>(Me->GetController()) : nullptr;
	if (!PC || Me->Combat->State == ENWCombatState::Dead)
	{
		return;
	}
	const ANWStation* From = nullptr;
	for (TActorIterator<ANWStation> It(GetWorld()); It; ++It)
	{
		if (It->Type == TEXT("teleporter") && FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation()) < 600.f)
		{
			From = *It;
		}
	}
	if (!From)
	{
		return;
	}
	// The game refuses fast travel when overloaded (ui_fast_travel_error_encumbered).
	if (Me->Inventory && Me->Inventory->IsEncumbered())
	{
		PC->Warning(NWText::Key(TEXT("ui_fast_travel_error_encumbered")));
		return;
	}
	const FString To = NWCity::OtherTownMap(From->City);
	if (!To.IsEmpty())
	{
		PC->TravelToMap(To);
	}
}


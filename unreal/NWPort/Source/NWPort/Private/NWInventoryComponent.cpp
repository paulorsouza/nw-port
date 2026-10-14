#include "NWInventoryComponent.h"
#include "NWDevRegistry.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "NWCharacter.h"
#include "NWCombatComponent.h"
#include "NWHUD.h"
#include "NWItemRules.h"
#include "NWPlayerController.h"
#include "NWText.h"
#include "Internationalization/Internationalization.h"
#include "Net/UnrealNetwork.h"

UNWInventoryComponent::UNWInventoryComponent()
{
	SetIsReplicatedByDefault(true);
}

void UNWInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UNWInventoryComponent, Items, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UNWInventoryComponent, Coins, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UNWInventoryComponent, PotionReadyAt, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UNWInventoryComponent, MaxWeight, COND_OwnerOnly);
}

float UNWInventoryComponent::ServerNow() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
}

float UNWInventoryComponent::GetPotionCooldown() const
{
	return FMath::Max(0.f, PotionReadyAt - ServerNow());
}

const FNWItem* UNWInventoryComponent::At(ENWItemContainer Container, int32 Slot) const
{
	return Items.FindByPredicate([&](const FNWItem& I) { return I.Container == Container && I.Slot == Slot; });
}

FNWItem* UNWInventoryComponent::Find(ENWItemContainer Container, int32 Slot)
{
	return Items.FindByPredicate([&](const FNWItem& I) { return I.Container == Container && I.Slot == Slot; });
}

int32 UNWInventoryComponent::CountOf(FName ItemId) const
{
	int32 N = 0;
	for (const FNWItem& I : Items)
	{
		N += I.ItemId == ItemId ? I.Quantity : 0;
	}
	return N;
}

int32 UNWInventoryComponent::Take(FName ItemId, int32 Quantity)
{
	int32 Left = Quantity;
	for (FNWItem& I : Items)
	{
		if (Left > 0 && I.Container == ENWItemContainer::Bag && I.ItemId == ItemId)
		{
			const int32 T = FMath::Min(Left, I.Quantity);
			I.Quantity -= T;
			Left -= T;
		}
	}
	Items.RemoveAll([](const FNWItem& X) { return X.Quantity <= 0; });
	if (Left < Quantity)
	{
		Changed();
	}
	return Quantity - Left;
}

FNWItem UNWInventoryComponent::TakeAt(int32 BagSlot, int32 Quantity)
{
	FNWItem Out;
	for (FNWItem& I : Items)
	{
		if (I.Container == ENWItemContainer::Bag && I.Slot == BagSlot && Quantity > 0)
		{
			Out = I;
			Out.Quantity = FMath::Min(Quantity, I.Quantity);
			I.Quantity -= Out.Quantity;
			break;
		}
	}
	Items.RemoveAll([](const FNWItem& X) { return X.Quantity <= 0; });
	if (Out.Quantity > 0)
	{
		Changed();
	}
	return Out;
}

int32 UNWInventoryComponent::FreeBagSlot() const
{
	for (int32 S = 0; S < BagSize; ++S)
	{
		if (!At(ENWItemContainer::Bag, S))
		{
			return S;
		}
	}
	return INDEX_NONE;
}

float UNWInventoryComponent::TotalWeight() const
{
	float W = 0.f;
	for (const FNWItem& I : Items)
	{
		if (const FNWItemDef* D = FNWItemCatalog::Get().Find(I.ItemId))
		{
			W += D->Weight * I.Quantity;
		}
	}
	return W;
}

int32 UNWInventoryComponent::BestPotionSlot() const
{
	int32 Best = INDEX_NONE, BestTier = -1;
	for (const FNWItem& I : Items)
	{
		const FNWItemDef* D = FNWItemCatalog::Get().Find(I.ItemId);
		if (I.Container == ENWItemContainer::Bag && D && D->Heal > 0.f && D->Tier > BestTier)
		{
			Best = I.Slot;
			BestTier = D->Tier;
		}
	}
	return Best;
}

float UNWInventoryComponent::DamageMultiplier(FName WeaponSet) const
{
	const int32 Slot = WeaponSet == TEXT("Hatchet") ? NWEquip::Hatchet : NWEquip::Sword;
	const FNWItem* W = At(ENWItemContainer::Equip, Slot);
	if (!W || W->GearScore <= 0)
	{
		return 1.f;
	}
	// Gear score 100 -> 0,6x, 600 -> 1,4x (o jogo escala o dano base pelo gear score).
	return FMath::GetMappedRangeValueClamped(FVector2f(100.f, 600.f), FVector2f(0.6f, 1.4f), float(W->GearScore));
}

void UNWInventoryComponent::Changed()
{
	GetOwner()->ForceNetUpdate();
}

int32 UNWInventoryComponent::AddItem(const FNWItem& In)
{
	const FNWItemDef* D = FNWItemCatalog::Get().Find(In.ItemId);
	if (!D || In.Quantity <= 0)
	{
		return In.Quantity;
	}
	int32 Left = In.Quantity;
	// Junta nas pilhas que já existem (armas não empilham: Stack = 1).
	for (FNWItem& I : Items)
	{
		if (Left > 0 && I.Container == ENWItemContainer::Bag && I.SameKind(In) && I.Quantity < D->Stack)
		{
			const int32 Take = FMath::Min(Left, D->Stack - I.Quantity);
			I.Quantity += Take;
			Left -= Take;
		}
	}
	while (Left > 0)
	{
		const int32 Slot = FreeBagSlot();
		if (Slot == INDEX_NONE)
		{
			break;
		}
		FNWItem New = In;
		New.Container = ENWItemContainer::Bag;
		New.Slot = Slot;
		New.Quantity = FMath::Min(Left, D->Stack);
		Items.Add(New);
		Left -= New.Quantity;
	}
	Changed();
	return Left;
}

FString UNWInventoryComponent::FormatCoins(int64 Cents)
{
	FNumberFormattingOptions O;
	O.MinimumFractionalDigits = 2;
	O.MaximumFractionalDigits = 2;
	const FCulturePtr Culture = FInternationalization::Get().GetCulture(NWText::Lang() == TEXT("pt-br") ? TEXT("pt-BR") : TEXT("en-US"));
	return FText::AsNumber(double(Cents) / 100.0, &O, Culture).ToString();
}

void UNWInventoryComponent::AddCoins(int64 Amount)
{
	Coins = FMath::Max<int64>(0, Coins + Amount);
	Changed();
}

void UNWInventoryComponent::Load(const TArray<FNWItem>& InItems, int64 InCoins)
{
	Items.Reset();
	for (const FNWItem& I : InItems)
	{
		// Item que saiu do catálogo some (não trava a entrada).
		if (FNWItemCatalog::Get().Find(I.ItemId) && I.Quantity > 0)
		{
			Items.Add(I);
		}
	}
	Coins = InCoins;
	Changed();
}

void UNWInventoryComponent::GiveStarterKit()
{
	auto Equip = [this](const TCHAR* Id, int32 Slot, int32 GearScore)
	{
		FNWItem I;
		I.ItemId = Id;
		I.Quantity = 1;
		I.Container = ENWItemContainer::Equip;
		I.Slot = Slot;
		I.GearScore = GearScore;
		Items.Add(I);
	};
	Equip(TEXT("1hSwordT2"), NWEquip::Sword, 250);
	Equip(TEXT("1hHatchetT2"), NWEquip::Hatchet, 250);
	Equip(TEXT("1hShieldAT2"), NWEquip::Shield, 250);
	GiveTools();
	FNWItem Potions;
	Potions.ItemId = TEXT("PotionHealthT5");
	Potions.Quantity = 5;
	AddItem(Potions);
	AddCoins(50);
}

void UNWInventoryComponent::GiveTools()
{
	static const TCHAR* Tools[] = {TEXT("2hPickT1"), TEXT("1hSickleT1"), TEXT("1hHatchetT1"), TEXT("1hSkinningKnifeT1")};
	for (int32 i = 0; i < UE_ARRAY_COUNT(Tools); ++i)
	{
		const int32 Slot = NWEquip::FirstTool + i;
		// Só se não tem nenhuma ferramenta desse tipo (equipada ou na bolsa).
		const bool bHasKind = Items.ContainsByPredicate([Slot](const FNWItem& I)
		{
			const FNWItemDef* D = FNWItemCatalog::Get().Find(I.ItemId);
			return D && NWEquip::SlotForSet(D->Set) == Slot;
		});
		if (!bHasKind && !At(ENWItemContainer::Equip, Slot))
		{
			FNWItem I;
			I.ItemId = Tools[i];
			I.Quantity = 1;
			I.Container = ENWItemContainer::Equip;
			I.Slot = Slot;
			FNWItemCatalog::Get().RollStats(I);
			Items.Add(I);
		}
	}
	Changed();
}

const FNWItemDef* UNWInventoryComponent::ToolFor(FName GatherType) const
{
	for (int32 Slot = NWEquip::FirstTool; Slot <= NWEquip::LastTool; ++Slot)
	{
		const FNWItem* I = At(ENWItemContainer::Equip, Slot);
		const FNWItemDef* D = I ? FNWItemCatalog::Get().Find(I->ItemId) : nullptr;
		if (D && D->Gather == GatherType)
		{
			return D;
		}
	}
	return nullptr;
}

bool UNWInventoryComponent::CanEquip(const FNWItem& Item, int32 EquipSlot) const
{
	const FNWItemDef* D = FNWItemCatalog::Get().Find(Item.ItemId);
	if (!D)
	{
		return false;
	}
	return !D->Set.IsNone() && NWEquip::SlotForSet(D->Set) == EquipSlot;
}

void UNWInventoryComponent::ServerMove_Implementation(ENWItemContainer FromContainer, int32 FromSlot, ENWItemContainer ToContainer, int32 ToSlot)
{
	const int32 ToMax = ToContainer == ENWItemContainer::Bag ? BagSize : NWEquip::Count;
	FNWItem* From = Find(FromContainer, FromSlot);
	if (!From || ToSlot < 0 || ToSlot >= ToMax || (FromContainer == ToContainer && FromSlot == ToSlot))
	{
		return;
	}
	FNWItem* To = Find(ToContainer, ToSlot);
	// Equipar: o item tem que servir no espaço; trocar: o que sai tem que servir onde o outro estava.
	if (ToContainer == ENWItemContainer::Equip && !CanEquip(*From, ToSlot))
	{
		return;
	}
	if (To && FromContainer == ENWItemContainer::Equip && !CanEquip(*To, FromSlot))
	{
		return;
	}
	// Mesma pilha na bolsa: junta.
	if (To && To->SameKind(*From) && ToContainer == ENWItemContainer::Bag && FromContainer == ENWItemContainer::Bag)
	{
		const FNWItemDef* D = FNWItemCatalog::Get().Find(From->ItemId);
		const int32 Take = D ? FMath::Min(From->Quantity, D->Stack - To->Quantity) : 0;
		if (Take > 0)
		{
			To->Quantity += Take;
			From->Quantity -= Take;
			if (From->Quantity <= 0)
			{
				Items.RemoveAll([&](const FNWItem& I) { return I.Container == FromContainer && I.Slot == FromSlot; });
			}
			Changed();
			return;
		}
	}
	if (To)
	{
		To->Container = FromContainer;
		To->Slot = FromSlot;
	}
	From->Container = ToContainer;
	From->Slot = ToSlot;
	// Vincula ao equipar (docs/itens.md): equipado uma vez, fica preso ao personagem.
	if (ToContainer == ENWItemContainer::Equip && From->Bind == 1)
	{
		From->Bind = 2;
	}
	Changed();
}

void UNWInventoryComponent::ServerUseBestPotion_Implementation()
{
	const int32 Slot = BestPotionSlot();
	if (Slot != INDEX_NONE)
	{
		ServerUse_Implementation(Slot);
	}
}

void UNWInventoryComponent::ServerUse_Implementation(int32 BagSlot)
{
	FNWItem* I = Find(ENWItemContainer::Bag, BagSlot);
	const FNWItemDef* D = I ? FNWItemCatalog::Get().Find(I->ItemId) : nullptr;
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!I || !D || !Me || D->Heal <= 0.f || Me->Combat->State == ENWCombatState::Dead || GetPotionCooldown() > 0.f)
	{
		return;
	}
	// Poção: cura fixa do jogo escalada para a nossa vida máxima + porcentagem da vida.
	UNWCombatComponent* C = Me->Combat;
	const float Amount = D->Heal * (C->MaxHealth / GameMaxHealth) + D->HealPercent * C->MaxHealth;
	C->Health = FMath::Min(C->MaxHealth, C->Health + Amount);
	PotionReadyAt = ServerNow() + FMath::Max(1.f, D->Cooldown);
	if (--I->Quantity <= 0)
	{
		Items.RemoveAll([&](const FNWItem& X) { return X.Container == ENWItemContainer::Bag && X.Slot == BagSlot; });
	}
	Changed();
}

void UNWInventoryComponent::Warn(const TCHAR* Key) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (ANWPlayerController* PC = Pawn ? Cast<ANWPlayerController>(Pawn->GetController()) : nullptr)
	{
		PC->Warning(NWText::Key(Key));
	}
}

FString UNWInventoryComponent::Unsocket(ENWItemContainer Container, int32 Slot)
{
	const FNWItem* Item = Find(Container, Slot);
	if (!Item || Item->Gem.IsNone())
	{
		return TEXT("nw.item.gem_none");
	}
	const FName Gem = Item->Gem;
	int32 Qty = 0;
	const FName Cost = FNWItemRules::Get().UnsocketCost(Gem, Qty);
	if (Cost.IsNone() || CountOf(Cost) < Qty)
	{
		return TEXT("nw.item.gem_cost");
	}
	// A gema volta para a bolsa: precisa de pilha com espaço ou de espaço livre.
	const FNWItemDef* GD = FNWItemCatalog::Get().Find(Gem);
	FNWItem Back;
	Back.ItemId = Gem;
	Back.Quantity = 1;
	const bool bStack = GD && Items.ContainsByPredicate([&](const FNWItem& I) { return I.Container == ENWItemContainer::Bag && I.SameKind(Back) && I.Quantity < GD->Stack; });
	if (!GD || (!bStack && FreeBagSlot() == INDEX_NONE))
	{
		return TEXT("nw.inv.bag_full");
	}
	Take(Cost, Qty);
	if (FNWItem* Again = Find(Container, Slot))
	{
		Again->Gem = NAME_None;
	}
	AddItem(Back);
	return FString();
}

void UNWInventoryComponent::ServerSocket_Implementation(int32 GemBagSlot, ENWItemContainer Container, int32 Slot)
{
	const FNWItem* GemStack = At(ENWItemContainer::Bag, GemBagSlot);
	const FNWItem* Item = At(Container, Slot);
	const FNWItemDef* D = Item ? FNWItemCatalog::Get().Find(Item->ItemId) : nullptr;
	if (!GemStack || !D || (Container == ENWItemContainer::Bag && Slot == GemBagSlot))
	{
		return;
	}
	const FName Gem = GemStack->ItemId;
	if (!FNWItemRules::Get().CanSocket(*D, *Item, Gem))
	{
		Warn(TEXT("nw.item.gem_wrong"));
		return;
	}
	// Slot ocupado: retirar antes, pagando (docs/itens.md "Slot"); acoplar não troca.
	if (!Item->Gem.IsNone())
	{
		Warn(TEXT("nw.item.gem_occupied"));
		return;
	}
	if (Take(Gem, 1) < 1)
	{
		return;
	}
	if (FNWItem* Target = Find(Container, Slot))
	{
		Target->Gem = Gem;
	}
	Changed();
}

void UNWInventoryComponent::ServerUnsocket_Implementation(ENWItemContainer Container, int32 Slot)
{
	const FString Err = Unsocket(Container, Slot);
	if (!Err.IsEmpty())
	{
		Warn(*Err);
		return;
	}
	Changed();
}

void UNWInventoryComponent::ServerDestroy_Implementation(int32 BagSlot, int32 Quantity)
{
	FNWItem* Item = Items.FindByPredicate([&](const FNWItem& X) { return X.Container == ENWItemContainer::Bag && X.Slot == BagSlot; });
	if (!Item || Quantity <= 0)
	{
		return;
	}
	if (Quantity < Item->Quantity)
	{
		Item->Quantity -= Quantity;
	}
	else
	{
		Items.RemoveAll([&](const FNWItem& X) { return X.Container == ENWItemContainer::Bag && X.Slot == BagSlot; });
	}
	Changed();
}

void UNWInventoryComponent::Grant(const TArray<FNWItem>& Gained, int64 GainedCoins)
{
	TArray<FNWItem> Kept;
	bool bFull = false;
	for (const FNWItem& Item : Gained)
	{
		const int32 Left = AddItem(Item);
		if (Left < Item.Quantity)
		{
			FNWItem K = Item;
			K.Quantity = Item.Quantity - Left;
			Kept.Add(K);
		}
		bFull |= Left > 0;
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	ANWPlayerController* PC = Pawn ? Cast<ANWPlayerController>(Pawn->GetController()) : nullptr;
	if (bFull && PC)
	{
		PC->Warning(NWText::Key(TEXT("nw.inv.bag_full")));
	}
	AddCoins(GainedCoins);
	if (Kept.Num() > 0 || GainedCoins > 0)
	{
		ClientLoot(Kept, GainedCoins);
	}
}

void UNWInventoryComponent::ClientLoot_Implementation(const TArray<FNWItem>& Gained, int64 CoinsGained)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (ANWHUD* HUD = PC ? Cast<ANWHUD>(PC->GetHUD()) : nullptr)
	{
		HUD->ShowLoot(Gained, CoinsGained);
	}
}

// Moedas de dev (F10): +100,00 (10.000 centavos) para testar loja, refino e mercado. Grava com o estado (PUT state).
NW_REGISTER_DEV(DevCoins, TEXT("sistemas.dev_coins"), TEXT("nw.dev.dev_coins"),
	[](ANWPlayerController* PC)
	{
		const ANWCharacter* C = PC ? Cast<ANWCharacter>(PC->GetPawn()) : nullptr;
		if (C && C->Inventory)
		{
			C->Inventory->AddCoins(10000);
		}
	},
	[](const ANWPlayerController* PC)
	{
		const ANWCharacter* C = PC ? Cast<ANWCharacter>(PC->GetPawn()) : nullptr;
		return FText::FromString(C && C->Inventory ? UNWInventoryComponent::FormatCoins(C->Inventory->Coins) : FString());
	});

// Bolsa de dev (F10): peso máximo de 10.000.000 para testar refino e ofício sem sobrecarga. Não vai para o nw-core.
NW_REGISTER_DEV(DevBag, TEXT("sistemas.dev_bag"), TEXT("nw.dev.dev_bag"),
	[](ANWPlayerController* PC)
	{
		const ANWCharacter* C = PC ? Cast<ANWCharacter>(PC->GetPawn()) : nullptr;
		if (C && C->Inventory)
		{
			C->Inventory->MaxWeight = C->Inventory->MaxWeight > UNWInventoryComponent::BaseMaxWeight
				? UNWInventoryComponent::BaseMaxWeight : 10000000.f;
		}
	},
	[](const ANWPlayerController* PC)
	{
		const ANWCharacter* C = PC ? Cast<ANWCharacter>(PC->GetPawn()) : nullptr;
		return FText::FromString(C && C->Inventory && C->Inventory->MaxWeight > UNWInventoryComponent::BaseMaxWeight
			? TEXT("10.000.000") : TEXT("1.000"));
	});

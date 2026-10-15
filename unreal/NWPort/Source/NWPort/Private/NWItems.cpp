#include "NWItems.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NWText.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

int32 NWEquip::SlotForSet(FName Set)
{
	static const TMap<FName, int32> Slots = {
		{TEXT("Sword"), Sword}, {TEXT("Hatchet"), Hatchet}, {TEXT("Shield"), Shield}, {TEXT("Pick"), Pick},
		{TEXT("Sickle"), Sickle}, {TEXT("LoggingAxe"), LoggingAxe}, {TEXT("SkinningKnife"), SkinningKnife},
		{TEXT("Head"), Head}, {TEXT("Chest"), Chest}, {TEXT("Hands"), Hands}, {TEXT("Legs"), Legs}, {TEXT("Feet"), Feet},
		{TEXT("Amulet"), Amulet}, {TEXT("Ring"), Ring}, {TEXT("Earring"), Earring}};
	const int32* S = Slots.Find(Set);
	return S ? *S : INDEX_NONE;
}

FNWItemCatalog& FNWItemCatalog::Get()
{
	static FNWItemCatalog Catalog;
	if (!Catalog.bLoaded)
	{
		Catalog.Load();
	}
	return Catalog;
}

void FNWItemCatalog::Reload()
{
	Items.Reset();
	LootTables.Reset();
	GameTables.Reset();
	Load();
}

void FNWItemCatalog::Load()
{
	bLoaded = true;
	FString Raw;
	TArray<TSharedPtr<FJsonValue>> List;
	if (FFileHelper::LoadFileToString(Raw, *NWText::DataPath(TEXT("Data/Items/items.json")))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), List))
	{
		for (const TSharedPtr<FJsonValue>& V : List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid())
			{
				continue;
			}
			FNWItemDef D;
			D.Id = FName(O->GetStringField(TEXT("id")));
			D.Type = O->GetStringField(TEXT("type"));
			D.Set = FName(O->GetStringField(TEXT("set")));
			D.Tier = O->GetIntegerField(TEXT("tier"));
			D.Stack = FMath::Max(1, O->GetIntegerField(TEXT("stack")));
			D.Icon = O->GetStringField(TEXT("icon"));
			D.GearScoreMin = O->GetIntegerField(TEXT("gs_min"));
			D.GearScoreMax = O->GetIntegerField(TEXT("gs_max"));
			O->TryGetNumberField(TEXT("damage"), D.Damage);
			O->TryGetNumberField(TEXT("heal"), D.Heal);
			O->TryGetNumberField(TEXT("heal_pct"), D.HealPercent);
			O->TryGetNumberField(TEXT("cooldown"), D.Cooldown);
			FString Gather;
			if (O->TryGetStringField(TEXT("gather"), Gather))
			{
				D.Gather = FName(Gather);
			}
			FString Armor;
			if (O->TryGetStringField(TEXT("armor"), Armor) && !Armor.IsEmpty())
			{
				D.Armor = FName(Armor);
			}
			O->TryGetNumberField(TEXT("efficiency"), D.Efficiency);
			O->TryGetNumberField(TEXT("weight"), D.Weight);
			O->TryGetStringField(TEXT("name"), D.Name);
			O->TryGetStringField(TEXT("description"), D.Description);
			O->TryGetStringField(TEXT("type_name"), D.TypeName);
			FString UiClass;
			if (O->TryGetStringField(TEXT("ui_class"), UiClass))
			{
				D.UiClass = FName(*UiClass);
			}
			O->TryGetBoolField(TEXT("round"), D.bRound);
			Items.Add(D.Id, D);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("NW: no Data/Items/items.*.json (tools/make_items.py)"));
	}

	TSharedPtr<FJsonObject> Root;
	if (FFileHelper::LoadFileToString(Raw, *(FPaths::ProjectDir() / TEXT("Data/Items/loot.json")))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) && Root.IsValid())
	{
		for (const auto& Pair : Root->Values)
		{
			const TSharedPtr<FJsonObject> T = Pair.Value->AsObject();
			FLootTable Table;
			const TArray<TSharedPtr<FJsonValue>>& Coins = T->GetArrayField(TEXT("coins"));
			Table.CoinsMin = Coins.Num() > 0 ? int32(Coins[0]->AsNumber()) : 0;
			Table.CoinsMax = Coins.Num() > 1 ? int32(Coins[1]->AsNumber()) : Table.CoinsMin;
			for (const TSharedPtr<FJsonValue>& RV : T->GetArrayField(TEXT("rolls")))
			{
				const TSharedPtr<FJsonObject> R = RV->AsObject();
				FRoll Roll;
				Roll.Chance = R->GetNumberField(TEXT("chance"));
				for (const TSharedPtr<FJsonValue>& P : R->GetArrayField(TEXT("pick")))
				{
					Roll.Pick.Add(FName(P->AsString()));
				}
				const TArray<TSharedPtr<FJsonValue>>& Q = R->GetArrayField(TEXT("qty"));
				Roll.QtyMin = Q.Num() > 0 ? int32(Q[0]->AsNumber()) : 1;
				Roll.QtyMax = Q.Num() > 1 ? int32(Q[1]->AsNumber()) : Roll.QtyMin;
				Table.Rolls.Add(Roll);
			}
			LootTables.Add(FName(*Pair.Key), Table);
		}
	}

	if (FFileHelper::LoadFileToString(Raw, *(FPaths::ProjectDir() / TEXT("Data/Items/loottables.json")))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) && Root.IsValid())
	{
		for (const auto& Pair : Root->Values)
		{
			const TSharedPtr<FJsonObject> T = Pair.Value->AsObject();
			FGameTable Table;
			Table.bAnd = T->GetBoolField(TEXT("and"));
			Table.MaxRoll = T->GetIntegerField(TEXT("max"));
			FString Bonus;
			Table.BonusMode = T->TryGetStringField(TEXT("bonus"), Bonus) ? (Bonus == TEXT("add") ? 1 : Bonus == TEXT("clamp") ? 2 : 0) : 0;
			for (const TSharedPtr<FJsonValue>& EV : T->GetArrayField(TEXT("e")))
			{
				const TSharedPtr<FJsonObject> E = EV->AsObject();
				FGameEntry Entry;
				FString Name;
				if (E->TryGetStringField(TEXT("item"), Name))
				{
					Entry.Item = FName(Name);
				}
				else if (E->TryGetStringField(TEXT("table"), Name))
				{
					Entry.Table = FName(Name);
				}
				const TArray<TSharedPtr<FJsonValue>>& Q = E->GetArrayField(TEXT("q"));
				Entry.QtyMin = Q.Num() > 0 ? int32(Q[0]->AsNumber()) : 1;
				Entry.QtyMax = Q.Num() > 1 ? int32(Q[1]->AsNumber()) : Entry.QtyMin;
				Entry.Threshold = E->GetIntegerField(TEXT("t"));
				Table.Entries.Add(Entry);
			}
			GameTables.Add(FName(*Pair.Key), MoveTemp(Table));
		}
	}
}

void FNWItemCatalog::RollGame(FName Table, TArray<FNWItem>& OutItems, int32 RollBonus) const
{
	RollGameRec(Table, OutItems, 0, RollBonus);
	// Merges duplicates (the same gem can come out in two rolls).
	for (int32 i = 0; i < OutItems.Num(); ++i)
	{
		for (int32 j = OutItems.Num() - 1; j > i; --j)
		{
			if (OutItems[j].ItemId == OutItems[i].ItemId && OutItems[i].GearScore == 0 && OutItems[j].GearScore == 0)
			{
				OutItems[i].Quantity += OutItems[j].Quantity;
				OutItems.RemoveAt(j);
			}
		}
	}
}

void FNWItemCatalog::RollGameRec(FName Table, TArray<FNWItem>& OutItems, int32 Depth, int32 RollBonus) const
{
	const FGameTable* T = GameTables.Find(Table);
	if (!T || Depth > 8)
	{
		return;
	}
	int32 Roll = T->MaxRoll > 0 ? FMath::RandRange(0, T->MaxRoll) : 0;
	if (T->BonusMode == 1)
	{
		Roll += RollBonus;
	}
	else if (T->BonusMode == 2)
	{
		Roll = FMath::Min(Roll + RollBonus, T->MaxRoll);
	}
	TArray<const FGameEntry*, TInlineAllocator<16>> Hits;
	for (const FGameEntry& E : T->Entries)
	{
		if (E.Threshold <= Roll)
		{
			Hits.Add(&E);
		}
	}
	if (!T->bAnd && Hits.Num() > 1)
	{
		// "or": keeps the highest range the die reached; tie = random pick.
		int32 Top = 0;
		for (const FGameEntry* E : Hits)
		{
			Top = FMath::Max(Top, E->Threshold);
		}
		Hits.RemoveAll([Top](const FGameEntry* E) { return E->Threshold != Top; });
		const FGameEntry* One = Hits[FMath::RandHelper(Hits.Num())];
		Hits = {One};
	}
	for (const FGameEntry* E : Hits)
	{
		const int32 Qty = FMath::RandRange(E->QtyMin, FMath::Max(E->QtyMin, E->QtyMax));
		if (!E->Table.IsNone())
		{
			for (int32 i = 0; i < Qty; ++i)
			{
				RollGameRec(E->Table, OutItems, Depth + 1, RollBonus);
			}
		}
		else if (Qty > 0 && Find(E->Item))
		{
			FNWItem Item;
			Item.ItemId = E->Item;
			Item.Quantity = Qty;
			RollStats(Item);
			OutItems.Add(Item);
		}
	}
}

void FNWItemCatalog::RollStats(FNWItem& Item) const
{
	const FNWItemDef* D = Find(Item.ItemId);
	if (!D || D->Type != TEXT("weapon"))
	{
		return;
	}
	Item.GearScore = FMath::RandRange(D->GearScoreMin, FMath::Max(D->GearScoreMin, D->GearScoreMax));
	// Rarity goes up with luck and with tier (in the game it comes from perks; here it is just the chance).
	const float R = FMath::FRand() + D->Tier * 0.04f;
	Item.Rarity = R > 1.05f ? 4 : R > 0.95f ? 3 : R > 0.8f ? 2 : R > 0.55f ? 1 : 0;
}

void FNWItemCatalog::RollLoot(FName Table, TArray<FNWItem>& OutItems, int64& OutCoins) const
{
	const FLootTable* T = Loot(Table);
	if (!T)
	{
		return;
	}
	OutCoins += FMath::RandRange(T->CoinsMin, T->CoinsMax);
	for (const FRoll& Roll : T->Rolls)
	{
		if (Roll.Pick.Num() == 0 || FMath::FRand() > Roll.Chance)
		{
			continue;
		}
		FNWItem Item;
		Item.ItemId = Roll.Pick[FMath::RandHelper(Roll.Pick.Num())];
		Item.Quantity = FMath::RandRange(Roll.QtyMin, Roll.QtyMax);
		RollStats(Item);
		OutItems.Add(Item);
	}
}

FLinearColor NWRarityColor(uint8 Rarity)
{
	switch (Rarity)
	{
	case 1: return FLinearColor(FColor(0x2C, 0x9A, 0x47));   // uncommon (green)
	case 2: return FLinearColor(FColor(0x2E, 0x7F, 0xD1));   // rare (blue)
	case 3: return FLinearColor(FColor(0x9A, 0x4D, 0xD6));   // epic (purple)
	case 4: return FLinearColor(FColor(0xE0, 0x8A, 0x2C));   // legendary (orange)
	default: return FLinearColor(FColor(0x8C, 0x8C, 0x8C));  // common
	}
}

TSharedRef<FJsonObject> NWItemProps::Write(const FNWItem& Item)
{
	TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
	if (Item.Attr != 255)
	{
		P->SetNumberField(TEXT("attr"), Item.Attr);
	}
	if (Item.Quality >= 0.f)
	{
		P->SetNumberField(TEXT("quality"), Item.Quality);
	}
	if (Item.Perks.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (FName Perk : Item.Perks)
		{
			Arr.Add(MakeShared<FJsonValueString>(Perk.ToString()));
		}
		P->SetArrayField(TEXT("perks"), Arr);
	}
	if (!Item.Gem.IsNone())
	{
		P->SetStringField(TEXT("gem"), Item.Gem.ToString());
	}
	if (Item.Bind == 1)
	{
		P->SetStringField(TEXT("bind"), TEXT("equip"));
	}
	else if (Item.Bind == 2)
	{
		P->SetBoolField(TEXT("bound"), true);   // nw-core refuses a bound item on the Trading Post
	}
	if (!Item.Signer.IsEmpty())
	{
		P->SetStringField(TEXT("signed"), Item.Signer);
	}
	if (!Item.Serial.IsEmpty())
	{
		P->SetStringField(TEXT("serial"), Item.Serial);
	}
	return P;
}

void NWItemProps::Read(FNWItem& Item, const TSharedPtr<FJsonObject>& Props)
{
	if (!Props.IsValid())
	{
		return;
	}
	double N = 0.0;
	if (Props->TryGetNumberField(TEXT("attr"), N))
	{
		Item.Attr = uint8(FMath::Clamp(int32(N), 0, 255));
	}
	if (Props->TryGetNumberField(TEXT("quality"), N))
	{
		Item.Quality = float(N);
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Props->TryGetArrayField(TEXT("perks"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			Item.Perks.Add(FName(*V->AsString()));
		}
	}
	FString S;
	if (Props->TryGetStringField(TEXT("gem"), S))
	{
		Item.Gem = FName(*S);
	}
	bool bBound = false;
	if (Props->TryGetBoolField(TEXT("bound"), bBound) && bBound)
	{
		Item.Bind = 2;
	}
	else if (Props->TryGetStringField(TEXT("bind"), S) && S == TEXT("equip"))
	{
		Item.Bind = 1;
	}
	Props->TryGetStringField(TEXT("signed"), Item.Signer);
	Props->TryGetStringField(TEXT("serial"), Item.Serial);
}

TSharedRef<FJsonObject> NWItemProps::WriteItem(const FNWItem& Item)
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("item_id"), Item.ItemId.ToString());
	J->SetNumberField(TEXT("quantity"), Item.Quantity);
	J->SetNumberField(TEXT("gear_score"), Item.GearScore);
	J->SetNumberField(TEXT("rarity"), Item.Rarity);
	J->SetObjectField(TEXT("props"), Write(Item));
	return J;
}

FNWItem NWItemProps::ReadItem(const TSharedPtr<FJsonObject>& Json)
{
	FNWItem Item;
	if (!Json.IsValid())
	{
		return Item;
	}
	Item.ItemId = FName(*Json->GetStringField(TEXT("item_id")));
	Item.Quantity = int32(Json->GetNumberField(TEXT("quantity")));
	Item.GearScore = int32(Json->GetNumberField(TEXT("gear_score")));
	Item.Rarity = uint8(Json->GetNumberField(TEXT("rarity")));
	const TSharedPtr<FJsonObject>* Props = nullptr;
	if (Json->TryGetObjectField(TEXT("props"), Props))
	{
		Read(Item, *Props);
	}
	return Item;
}

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
		{TEXT("Sickle"), Sickle}, {TEXT("LoggingAxe"), LoggingAxe}, {TEXT("SkinningKnife"), SkinningKnife}};
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
			O->TryGetNumberField(TEXT("efficiency"), D.Efficiency);
			O->TryGetNumberField(TEXT("weight"), D.Weight);
			O->TryGetStringField(TEXT("name"), D.Name);
			O->TryGetStringField(TEXT("description"), D.Description);
			O->TryGetStringField(TEXT("type_name"), D.TypeName);
			Items.Add(D.Id, D);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("NW: sem Data/Items/items.*.json (tools/make_items.py)"));
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

void FNWItemCatalog::RollGame(FName Table, TArray<FNWItem>& OutItems) const
{
	RollGameRec(Table, OutItems, 0);
	// Junta repetidos (a mesma gema pode sair em duas rolagens).
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

void FNWItemCatalog::RollGameRec(FName Table, TArray<FNWItem>& OutItems, int32 Depth) const
{
	const FGameTable* T = GameTables.Find(Table);
	if (!T || Depth > 8)
	{
		return;
	}
	const int32 Roll = T->MaxRoll > 0 ? FMath::RandRange(0, T->MaxRoll) : 0;
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
		// "or": fica a faixa mais alta que o dado alcançou; empate = sorteio.
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
				RollGameRec(E->Table, OutItems, Depth + 1);
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
	// Raridade sobe com a sorte e com o tier (no jogo vem das vantagens; aqui é só a chance).
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
	case 1: return FLinearColor(FColor(0x2C, 0x9A, 0x47));   // incomum (verde)
	case 2: return FLinearColor(FColor(0x2E, 0x7F, 0xD1));   // raro (azul)
	case 3: return FLinearColor(FColor(0x9A, 0x4D, 0xD6));   // épico (roxo)
	case 4: return FLinearColor(FColor(0xE0, 0x8A, 0x2C));   // lendário (laranja)
	default: return FLinearColor(FColor(0x8C, 0x8C, 0x8C));  // comum
	}
}

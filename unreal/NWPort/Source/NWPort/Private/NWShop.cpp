#include "NWShop.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FNWShops& FNWShops::Get()
{
	static FNWShops Instance;
	static bool bLoaded = false;
	if (!bLoaded)
	{
		bLoaded = true;
		Instance.Load();
	}
	return Instance;
}

void FNWShops::Load()
{
	FString Raw;
	TSharedPtr<FJsonObject> Root;
	const FString File = FPaths::ProjectDir() / TEXT("Data/World/shops.json");
	if (!FFileHelper::LoadFileToString(Raw, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Shops: could not read %s"), *File);
		return;
	}
	const TSharedPtr<FJsonObject>* ShopsJson = nullptr;
	if (!Root->TryGetObjectField(TEXT("lojas"), ShopsJson))
	{
		return;
	}
	for (const auto& Pair : (*ShopsJson)->Values)
	{
		const TSharedPtr<FJsonObject> L = Pair.Value->AsObject();
		if (!L.IsValid())
		{
			continue;
		}
		FNWShop& S = Shops.AddDefaulted_GetRef();
		S.Id = *Pair.Key;
		L->TryGetStringField(TEXT("cidade"), S.City);
		L->TryGetStringField(TEXT("npc"), S.Npc);
		const TArray<TSharedPtr<FJsonValue>>* ItemsJson = nullptr;
		if (L->TryGetArrayField(TEXT("itens"), ItemsJson))
		{
			for (const TSharedPtr<FJsonValue>& V : *ItemsJson)
			{
				const TSharedPtr<FJsonObject> I = V->AsObject();
				FNWShopItem& It = S.Items.AddDefaulted_GetRef();
				It.Id = FName(*I->GetStringField(TEXT("id")));
				It.Price = int64(I->GetNumberField(TEXT("preco")));
				I->TryGetNumberField(TEXT("limite_semanal"), It.WeeklyLimit);
			}
		}
	}
}

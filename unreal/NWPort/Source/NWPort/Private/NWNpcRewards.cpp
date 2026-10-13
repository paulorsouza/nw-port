#include "NWNpcRewards.h"

#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NWItems.h"
#include "NWNpc.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FNWNpcRewards& FNWNpcRewards::Get()
{
	static FNWNpcRewards Instance;
	return Instance;
}

FNWNpcRewards::FNWNpcRewards()
{
	FString Raw;
	TSharedPtr<FJsonObject> Root;
	const FString File = FPaths::ProjectDir() / TEXT("Data/World/npc_rewards.json");
	if (!FFileHelper::LoadFileToString(Raw, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Brindes: não li %s"), *File);
		return;
	}
	auto Ranges = [](const TArray<TSharedPtr<FJsonValue>>& In, TArray<FRange>& Out)
	{
		for (const TSharedPtr<FJsonValue>& V : In)
		{
			const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
			if (A.Num() == 3)
			{
				Out.Add({FName(*A[0]->AsString()), int32(A[1]->AsNumber()), int32(A[2]->AsNumber())});
			}
		}
	};
	for (const auto& Pair : Root->GetObjectField(TEXT("tipos"))->Values)
	{
		const TSharedPtr<FJsonObject> T = Pair.Value->AsObject();
		FType& Type = Types.Add(*Pair.Key);
		Ranges(T->GetArrayField(TEXT("inicial")), Type.Starter);
		for (const auto& List : T->GetObjectField(TEXT("semanal"))->Values)
		{
			Ranges(List.Value->AsArray(), Type.WeeklyLists.AddDefaulted_GetRef());
		}
	}
}

FString FNWNpcRewards::Period(const FDateTime& NowUtc) const
{
	// Semana começa terça 08:00 UTC: recua 8 h e volta até a terça (EDayOfWeek: segunda = 0).
	const FDateTime Shifted = NowUtc - FTimespan::FromHours(8.0);
	const int32 Back = (int32(Shifted.GetDayOfWeek()) - int32(EDayOfWeek::Tuesday) + 7) % 7;
	const FDateTime Start = Shifted.GetDate() - FTimespan::FromDays(Back);
	return FString::Printf(TEXT("%04d-%02d-%02d"), Start.GetYear(), Start.GetMonth(), Start.GetDay());
}

FString FNWNpcRewards::Chosen(const UWorld* World, const FString& City, const FString& Type, const FString& InPeriod) const
{
	TArray<FString> Pool;
	for (TActorIterator<ANWNpc> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->Type == Type && !It->DisplayName.IsEmpty())
		{
			Pool.AddUnique(It->Variant);
		}
	}
	if (Pool.IsEmpty())
	{
		return FString();
	}
	Pool.Sort();
	const uint32 H = FCrc::StrCrc32(*(City + TEXT("|") + Type + TEXT("|") + InPeriod));
	return Pool[H % uint32(Pool.Num())];
}

bool FNWNpcRewards::HasStarter(const FString& Type) const
{
	const FType* T = Types.Find(Type);
	return T && T->Starter.Num() > 0;
}

FNWNpcRewards::EState FNWNpcRewards::StateFor(const ANWNpc* Npc, const TArray<FString>& Claims) const
{
	if (!Npc || Npc->Type.IsEmpty() || !Types.Contains(Npc->Type))
	{
		return EState::None;
	}
	const FString P = Period();
	if (Chosen(Npc->GetWorld(), Npc->City, Npc->Type, P) != Npc->Variant)
	{
		return EState::None;
	}
	const bool bStarter = HasStarter(Npc->Type) && !Claims.Contains(Key(TEXT("starter"), StarterNpc(Npc->Type), TEXT("once")));
	const bool bWeekly = !Claims.Contains(Key(TEXT("weekly"), WeeklyNpc(Npc->City, Npc->Type), P));
	return bStarter || bWeekly ? EState::Available : EState::Claimed;
}

void FNWNpcRewards::Starter(const FString& Type, TArray<FNWItem>& Out) const
{
	if (const FType* T = Types.Find(Type))
	{
		for (const FRange& R : T->Starter)
		{
			FNWItem I;
			I.ItemId = R.Item;
			I.Quantity = FMath::RandRange(R.Min, R.Max);
			Out.Add(I);
		}
	}
}

void FNWNpcRewards::Weekly(const FString& Type, TArray<FNWItem>& Out) const
{
	const FType* T = Types.Find(Type);
	if (!T)
	{
		return;
	}
	for (const TArray<FRange>& List : T->WeeklyLists)
	{
		if (List.Num() > 0)
		{
			const FRange& R = List[FMath::RandRange(0, List.Num() - 1)];
			FNWItem I;
			I.ItemId = R.Item;
			I.Quantity = FMath::RandRange(R.Min, R.Max);
			Out.Add(I);
		}
	}
}

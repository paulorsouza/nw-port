#include "NWProgressionComponent.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Net/UnrealNetwork.h"
#include "NWHUD.h"
#include "NWText.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	struct FProgressionData
	{
		int32 BaseAttribute = 5;
		TArray<int64> LevelXp;      // [n] = XP total para chegar ao nível n+1
		TArray<int32> LevelPoints;  // [n] = pontos ganhos ao chegar ao nível n+1
		TMap<int32, float> ConstitutionHealth;
		struct FTrade { TArray<int32> Xp; TArray<float> Eff; };
		FTrade Trade[NWTradeskill::Num];

		static const FProgressionData& Get()
		{
			static FProgressionData Data = []
			{
				FProgressionData D;
				FString Raw;
				TSharedPtr<FJsonObject> Root;
				if (!FFileHelper::LoadFileToString(Raw, *(FPaths::ProjectDir() / TEXT("Data/Progression/progression.json")))
					|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
				{
					return D;
				}
				D.BaseAttribute = Root->GetIntegerField(TEXT("base_attribute"));
				for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("levels")))
				{
					D.LevelXp.Add(int64(V->AsObject()->GetNumberField(TEXT("xp"))));
					D.LevelPoints.Add(V->AsObject()->GetIntegerField(TEXT("points")));
				}
				const TSharedPtr<FJsonObject>* Attrs = nullptr;
				const TSharedPtr<FJsonObject>* Con = nullptr;
				if (Root->TryGetObjectField(TEXT("attributes"), Attrs) && (*Attrs)->TryGetObjectField(TEXT("constitution"), Con))
				{
					for (const auto& Pair : (*Con)->Values)
					{
						D.ConstitutionHealth.Add(FCString::Atoi(*Pair.Key), Pair.Value->AsObject()->GetNumberField(TEXT("health")));
					}
				}
				const TSharedPtr<FJsonObject>* Trades = nullptr;
				if (Root->TryGetObjectField(TEXT("tradeskills"), Trades))
				{
					for (int32 t = 0; t < NWTradeskill::Num; ++t)
					{
						const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
						if ((*Trades)->TryGetArrayField(NWTradeskill::Name(t).ToString(), Rows))
						{
							for (const TSharedPtr<FJsonValue>& R : *Rows)
							{
								D.Trade[t].Xp.Add(int32(R->AsObject()->GetNumberField(TEXT("xp"))));
								D.Trade[t].Eff.Add(float(R->AsObject()->GetNumberField(TEXT("eff"))));
							}
						}
					}
				}
				return D;
			}();
			return Data;
		}
	};
}

namespace NWTradeskill
{
	static const TCHAR* Names[Num] = {TEXT("Mining"), TEXT("Logging"), TEXT("Harvesting"), TEXT("Skinning")};

	int32 Index(FName Tradeskill)
	{
		for (int32 i = 0; i < Num; ++i)
		{
			if (Tradeskill == Names[i])
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	FName Name(int32 I) { return I >= 0 && I < Num ? FName(Names[I]) : NAME_None; }
	FString TextKey(int32 I) { return I >= 0 && I < Num ? TEXT("ui_") + FString(Names[I]).ToLower() : FString(); }
	FString Icon(int32 I)
	{
		return I >= 0 && I < Num ? FString::Printf(TEXT("lyshineui/images/icons/tradeskills/icon_tradeskill_%s.dds"), *FString(Names[I]).ToLower()) : FString();
	}
}

UNWProgressionComponent::UNWProgressionComponent()
{
	SetIsReplicatedByDefault(true);
	Spent.Init(0, int32(ENWAttribute::Num));
	TradeXp.Init(0, NWTradeskill::Num);
}

int32 UNWProgressionComponent::TradeMaxLevel(int32 Skill) const
{
	return Skill >= 0 && Skill < NWTradeskill::Num ? FMath::Max(0, FProgressionData::Get().Trade[Skill].Xp.Num() - 1) : 0;
}

int32 UNWProgressionComponent::TradeLevel(int32 Skill) const
{
	if (Skill < 0 || Skill >= NWTradeskill::Num || !TradeXp.IsValidIndex(Skill))
	{
		return 0;
	}
	const TArray<int32>& T = FProgressionData::Get().Trade[Skill].Xp;
	int32 L = 0;
	while (L + 1 < T.Num() && TradeXp[Skill] >= T[L + 1])
	{
		++L;
	}
	return L;
}

float UNWProgressionComponent::TradeProgress(int32 Skill) const
{
	const int32 L = TradeLevel(Skill);
	if (L >= TradeMaxLevel(Skill) || !TradeXp.IsValidIndex(Skill))
	{
		return 1.f;
	}
	const TArray<int32>& T = FProgressionData::Get().Trade[Skill].Xp;
	return FMath::Clamp(float(TradeXp[Skill] - T[L]) / float(FMath::Max(1, T[L + 1] - T[L])), 0.f, 1.f);
}

float UNWProgressionComponent::TradeEfficiency(int32 Skill) const
{
	const int32 L = TradeLevel(Skill);
	const TArray<float>& E = Skill >= 0 && Skill < NWTradeskill::Num ? FProgressionData::Get().Trade[Skill].Eff : TArray<float>();
	return E.IsValidIndex(L) ? E[L] : 0.f;
}

void UNWProgressionComponent::AddTradeXp(FName Tradeskill, int32 Amount)
{
	const int32 Skill = NWTradeskill::Index(Tradeskill);
	if (Skill == INDEX_NONE || Amount <= 0 || GetOwnerRole() != ROLE_Authority)
	{
		return;
	}
	const int32 Cap = FProgressionData::Get().Trade[Skill].Xp.Num() ? FProgressionData::Get().Trade[Skill].Xp.Last() : 0;
	TradeXp[Skill] = FMath::Min(TradeXp[Skill] + Amount, Cap);
	GetOwner()->ForceNetUpdate();
	ClientTradeXpGained(uint8(Skill), Amount, TradeLevel(Skill));
}

void UNWProgressionComponent::ClientTradeXpGained_Implementation(uint8 Skill, int32 Amount, int32 NewLevel)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (ANWHUD* Hud = PC ? Cast<ANWHUD>(PC->GetHUD()) : nullptr)
	{
		Hud->ShowTradeXp(Skill, Amount);
	}
}

void UNWProgressionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UNWProgressionComponent, Level, COND_None);
	DOREPLIFETIME_CONDITION(UNWProgressionComponent, Xp, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UNWProgressionComponent, Spent, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UNWProgressionComponent, TradeXp, COND_OwnerOnly);
}

int32 UNWProgressionComponent::MaxLevel() const
{
	return FMath::Max(1, FProgressionData::Get().LevelXp.Num());
}

int64 UNWProgressionComponent::XpForLevel(int32 InLevel)
{
	const TArray<int64>& T = FProgressionData::Get().LevelXp;
	return T.IsValidIndex(InLevel - 1) ? T[InLevel - 1] : 0;
}

float UNWProgressionComponent::LevelProgress() const
{
	if (Level >= MaxLevel())
	{
		return 1.f;
	}
	const int64 From = XpForLevel(Level), To = XpForLevel(Level + 1);
	return To > From ? FMath::Clamp(float(Xp - From) / float(To - From), 0.f, 1.f) : 1.f;
}

int32 UNWProgressionComponent::Attribute(ENWAttribute A) const
{
	return FProgressionData::Get().BaseAttribute + (Spent.IsValidIndex(int32(A)) ? Spent[int32(A)] : 0);
}

int32 UNWProgressionComponent::UnspentPoints() const
{
	const TArray<int32>& P = FProgressionData::Get().LevelPoints;
	int32 Earned = 0;
	for (int32 i = 1; i < Level && i < P.Num(); ++i)
	{
		Earned += P[i];
	}
	int32 Used = 0;
	for (int32 S : Spent)
	{
		Used += S;
	}
	return FMath::Max(0, Earned - Used);
}

float UNWProgressionComponent::ConstitutionHealth() const
{
	// A coluna Health já é o total acumulado para o valor (6 = 40, 14 = 360).
	return FProgressionData::Get().ConstitutionHealth.FindRef(Attribute(ENWAttribute::Constitution));
}

void UNWProgressionComponent::AddXp(int64 Amount)
{
	if (Amount <= 0 || GetOwnerRole() != ROLE_Authority)
	{
		return;
	}
	Xp += Amount;
	while (Level < MaxLevel() && Xp >= XpForLevel(Level + 1))
	{
		++Level;
	}
	GetOwner()->ForceNetUpdate();
	ClientXpGained(Amount, Level);
}

void UNWProgressionComponent::ServerSpendPoint_Implementation(ENWAttribute A)
{
	if (A < ENWAttribute::Num && UnspentPoints() > 0)
	{
		++Spent[int32(A)];
		GetOwner()->ForceNetUpdate();
	}
}

void UNWProgressionComponent::ClientXpGained_Implementation(int64 Amount, int32 NewLevel)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (ANWHUD* Hud = PC ? Cast<ANWHUD>(PC->GetHUD()) : nullptr)
	{
		Hud->ShowXp(Amount);
	}
}

#include "NWProgressionComponent.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Net/UnrealNetwork.h"
#include "NWHUD.h"
#include "NWPlayerController.h"
#include "NWCharacter.h"
#include "NWDevRegistry.h"
#include "NWEvents.h"
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
		struct FTrade { TArray<int32> Xp; TArray<float> Eff; TArray<int32> Roll; TArray<TArray<FIntPoint>> Gs; };
		FTrade Trade[NWTradeskill::Num];
		// Tutoriais de nível (tools/tutorials.json).
		struct FTutorial { FString Id; FString Kind; FString Target; int32 Count = 0; int32 Levels = 0; TMap<FString, FString> Name; };
		TArray<FTutorial> Tutorials;
		float KillLevelFraction = 0.f;
		int32 KillXpMaxLevel = 0;

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
								D.Trade[t].Roll.Add(int32(R->AsObject()->GetNumberField(TEXT("roll"))));
								TArray<FIntPoint>& Gs = D.Trade[t].Gs.AddDefaulted_GetRef();
								const TArray<TSharedPtr<FJsonValue>>* GsRows = nullptr;
								if (R->AsObject()->TryGetArrayField(TEXT("gs"), GsRows))
								{
									for (const TSharedPtr<FJsonValue>& G : *GsRows)
									{
										const TArray<TSharedPtr<FJsonValue>>& P = G->AsArray();
										Gs.Add(FIntPoint(int32(P[0]->AsNumber()), int32(P[1]->AsNumber())));
									}
								}
							}
						}
					}
				}
				const TSharedPtr<FJsonObject>* Tut = nullptr;
				if (Root->TryGetObjectField(TEXT("tutorials"), Tut))
				{
					for (const TSharedPtr<FJsonValue>& V : (*Tut)->GetArrayField(TEXT("missoes")))
					{
						const TSharedPtr<FJsonObject> M = V->AsObject();
						FTutorial& T = D.Tutorials.AddDefaulted_GetRef();
						T.Id = M->GetStringField(TEXT("id"));
						T.Kind = M->GetStringField(TEXT("tipo"));
						T.Target = M->GetStringField(TEXT("alvo"));
						T.Count = int32(M->GetNumberField(TEXT("quantidade")));
						T.Levels = int32(M->GetNumberField(TEXT("niveis")));
						for (const auto& N : M->GetObjectField(TEXT("nome"))->Values)
						{
							T.Name.Add(*N.Key, N.Value->AsString());
						}
					}
					const TSharedPtr<FJsonObject> After = (*Tut)->GetObjectField(TEXT("apos_tutoriais"));
					D.KillLevelFraction = float(After->GetNumberField(TEXT("abate_fracao_nivel")));
					D.KillXpMaxLevel = int32(After->GetNumberField(TEXT("nivel_max")));
				}
				return D;
			}();
			return Data;
		}
	};
}

namespace NWTradeskill
{
	static const TCHAR* Names[Num] = {TEXT("Mining"), TEXT("Logging"), TEXT("Harvesting"), TEXT("Skinning"), TEXT("Smelting"),
		TEXT("Woodworking"), TEXT("Leatherworking"), TEXT("Weaving"), TEXT("Stonecutting"), TEXT("Weaponsmithing"),
		TEXT("Engineering"), TEXT("Arcana"), TEXT("Jewelcrafting"), TEXT("Armoring")};

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
		// Ícones com o nome da mesa no jogo: masonary (cantaria, grafia do jogo), blacksmithing (armeiro), alchemy (arcano),
		// outfitting (armoraria).
		const FString File = I == Index(TEXT("Stonecutting")) ? FString(TEXT("masonary"))
			: I == Index(TEXT("Weaponsmithing")) ? FString(TEXT("blacksmithing"))
			: I == Index(TEXT("Arcana")) ? FString(TEXT("alchemy"))
			: I == Index(TEXT("Armoring")) ? FString(TEXT("outfitting"))
			: FString(I >= 0 && I < Num ? Names[I] : TEXT("")).ToLower();
		return I >= 0 && I < Num ? FString::Printf(TEXT("lyshineui/images/icons/tradeskills/icon_tradeskill_%s.dds"), *File) : FString();
	}
}

UNWProgressionComponent::UNWProgressionComponent()
{
	SetIsReplicatedByDefault(true);
	Spent.Init(0, int32(ENWAttribute::Num));
	TradeXp.Init(0, NWTradeskill::Num);
	TutorialCount.Init(0, FProgressionData::Get().Tutorials.Num());
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

int32 UNWProgressionComponent::MissingTradeLevel(FName Tradeskill, int32 Required) const
{
	const int32 Skill = NWTradeskill::Index(Tradeskill);
	if (!bFromBackend || Skill == INDEX_NONE || Required <= 0)
	{
		return 0;
	}
	return TradeLevel(Skill) >= Required ? 0 : Required;
}

FIntPoint UNWProgressionComponent::TradeGsBonus(int32 Skill, int32 ItemTier) const
{
	if (Skill < 0 || Skill >= NWTradeskill::Num)
	{
		return FIntPoint::ZeroValue;
	}
	const TArray<TArray<FIntPoint>>& Gs = FProgressionData::Get().Trade[Skill].Gs;
	const int32 L = TradeLevel(Skill);
	const int32 T = ItemTier - 2;   // colunas T2..T5
	return Gs.IsValidIndex(L) && Gs[L].IsValidIndex(T) ? Gs[L][T] : FIntPoint::ZeroValue;
}

int32 UNWProgressionComponent::TradeXpToNext(int32 Skill) const
{
	const int32 L = TradeLevel(Skill);
	const TArray<int32>& T = Skill >= 0 && Skill < NWTradeskill::Num ? FProgressionData::Get().Trade[Skill].Xp : TArray<int32>();
	return T.IsValidIndex(L + 1) && TradeXp.IsValidIndex(Skill) ? T[L + 1] - TradeXp[Skill] : 0;
}

int32 UNWProgressionComponent::TradeRollBonus(int32 Skill) const
{
	const int32 L = TradeLevel(Skill);
	const TArray<int32>& R = Skill >= 0 && Skill < NWTradeskill::Num ? FProgressionData::Get().Trade[Skill].Roll : TArray<int32>();
	return R.IsValidIndex(L) ? R[L] : 0;
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
	FNWEvents::Get().OnTradeXp.Broadcast(Cast<ANWCharacter>(GetOwner()), Tradeskill, Amount);
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
	DOREPLIFETIME_CONDITION(UNWProgressionComponent, TutorialCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UNWProgressionComponent, Crafted, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UNWProgressionComponent, ShopPeriod, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UNWProgressionComponent, ShopBoughtIds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UNWProgressionComponent, ShopBoughtQty, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UNWProgressionComponent, bFromBackend, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UNWProgressionComponent, Cities, COND_OwnerOnly);
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

void UNWProgressionComponent::Load(const TSharedPtr<FJsonObject>& Json)
{
	if (!Json.IsValid())
	{
		return;
	}
	bFromBackend = true;
	double Num = 0.0;
	if (Json->TryGetNumberField(TEXT("xp"), Num))
	{
		Xp = FMath::Max<int64>(0, int64(Num));
	}
	// O nível sai do XP pela tabela (o salvo é só conferência).
	Level = 1;
	while (Level < MaxLevel() && Xp >= XpForLevel(Level + 1))
	{
		++Level;
	}
	auto ReadInts = [&Json](const TCHAR* Name, TArray<int32>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Json->TryGetArrayField(Name, Arr))
		{
			for (int32 i = 0; i < Out.Num() && i < Arr->Num(); ++i)
			{
				Out[i] = FMath::Max(0, int32((*Arr)[i]->AsNumber()));
			}
		}
	};
	ReadInts(TEXT("spent"), Spent);
	ReadInts(TEXT("trade"), TradeXp);
	const TSharedPtr<FJsonObject>* Tut = nullptr;
	if (Json->TryGetObjectField(TEXT("tut"), Tut))
	{
		const auto& Ts = FProgressionData::Get().Tutorials;
		for (int32 i = 0; i < Ts.Num() && i < TutorialCount.Num(); ++i)
		{
			double N = 0.0;
			if ((*Tut)->TryGetNumberField(Ts[i].Id, N))
			{
				TutorialCount[i] = int32(N);
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Done = nullptr;
	Crafted.Reset();
	if (Json->TryGetArrayField(TEXT("crafted"), Done))
	{
		for (const TSharedPtr<FJsonValue>& V : *Done)
		{
			Crafted.AddUnique(FName(*V->AsString()));
		}
	}
	Cities.Reset();
	if (Json->TryGetArrayField(TEXT("cities"), Done))
	{
		for (const TSharedPtr<FJsonValue>& V : *Done)
		{
			Cities.AddUnique(V->AsString());
		}
	}
	const TSharedPtr<FJsonObject>* Shop = nullptr;
	ShopBoughtIds.Reset();
	ShopBoughtQty.Reset();
	ShopPeriod.Reset();
	if (Json->TryGetObjectField(TEXT("shop"), Shop))
	{
		(*Shop)->TryGetStringField(TEXT("period"), ShopPeriod);
		const TSharedPtr<FJsonObject>* Bought = nullptr;
		if ((*Shop)->TryGetObjectField(TEXT("bought"), Bought))
		{
			for (const auto& B : (*Bought)->Values)
			{
				ShopBoughtIds.Add(FName(*B.Key));
				ShopBoughtQty.Add(int32(B.Value->AsNumber()));
			}
		}
	}
	// Pontos gastos além dos ganhos (tabela mudou): zera a distribuição.
	int32 Used = 0;
	for (int32 S : Spent)
	{
		Used += S;
	}
	const TArray<int32>& P = FProgressionData::Get().LevelPoints;
	int32 Earned = 0;
	for (int32 i = 1; i < Level && i < P.Num(); ++i)
	{
		Earned += P[i];
	}
	if (Used > Earned)
	{
		Spent.Init(0, int32(ENWAttribute::Num));
	}
	GetOwner()->ForceNetUpdate();
}

TSharedRef<FJsonObject> UNWProgressionComponent::Save() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetNumberField(TEXT("level"), Level);
	J->SetNumberField(TEXT("xp"), double(Xp));
	auto WriteInts = [&J](const TCHAR* Name, const TArray<int32>& In)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (int32 V : In)
		{
			Arr.Add(MakeShared<FJsonValueNumber>(V));
		}
		J->SetArrayField(Name, Arr);
	};
	WriteInts(TEXT("spent"), Spent);
	WriteInts(TEXT("trade"), TradeXp);
	TSharedRef<FJsonObject> Tut = MakeShared<FJsonObject>();
	const auto& Ts = FProgressionData::Get().Tutorials;
	for (int32 i = 0; i < Ts.Num() && i < TutorialCount.Num(); ++i)
	{
		if (TutorialCount[i] != 0)
		{
			Tut->SetNumberField(Ts[i].Id, TutorialCount[i]);
		}
	}
	J->SetObjectField(TEXT("tut"), Tut);
	TArray<TSharedPtr<FJsonValue>> Done;
	for (FName R : Crafted)
	{
		Done.Add(MakeShared<FJsonValueString>(R.ToString()));
	}
	J->SetArrayField(TEXT("crafted"), Done);
	TArray<TSharedPtr<FJsonValue>> Known;
	for (const FString& City : Cities)
	{
		Known.Add(MakeShared<FJsonValueString>(City));
	}
	J->SetArrayField(TEXT("cities"), Known);
	TSharedRef<FJsonObject> Shop = MakeShared<FJsonObject>();
	Shop->SetStringField(TEXT("period"), ShopPeriod);
	TSharedRef<FJsonObject> Bought = MakeShared<FJsonObject>();
	for (int32 i = 0; i < ShopBoughtIds.Num() && i < ShopBoughtQty.Num(); ++i)
	{
		Bought->SetNumberField(ShopBoughtIds[i].ToString(), ShopBoughtQty[i]);
	}
	Shop->SetObjectField(TEXT("bought"), Bought);
	J->SetObjectField(TEXT("shop"), Shop);
	return J;
}

int32 UNWProgressionComponent::ShopBought(FName Item, const FString& Period) const
{
	const int32 i = ShopPeriod == Period ? ShopBoughtIds.IndexOfByKey(Item) : INDEX_NONE;
	return i != INDEX_NONE && ShopBoughtQty.IsValidIndex(i) ? ShopBoughtQty[i] : 0;
}

void UNWProgressionComponent::AddShopBought(FName Item, int32 Quantity, const FString& Period)
{
	if (GetOwnerRole() != ROLE_Authority || Quantity <= 0)
	{
		return;
	}
	if (ShopPeriod != Period)
	{
		// Semana nova: o limite volta a zero.
		ShopPeriod = Period;
		ShopBoughtIds.Reset();
		ShopBoughtQty.Reset();
	}
	const int32 i = ShopBoughtIds.IndexOfByKey(Item);
	if (i == INDEX_NONE)
	{
		ShopBoughtIds.Add(Item);
		ShopBoughtQty.Add(Quantity);
	}
	else
	{
		ShopBoughtQty[i] += Quantity;
	}
	GetOwner()->ForceNetUpdate();
}

void UNWProgressionComponent::MarkCrafted(FName Recipe)
{
	if (GetOwnerRole() == ROLE_Authority && !Crafted.Contains(Recipe))
	{
		Crafted.Add(Recipe);
		GetOwner()->ForceNetUpdate();
	}
}

bool UNWProgressionComponent::DiscoverCity(const FString& City)
{
	if (GetOwnerRole() != ROLE_Authority || City.IsEmpty() || Cities.Contains(City))
	{
		return false;
	}
	Cities.Add(City);
	GetOwner()->ForceNetUpdate();
	return true;
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

void UNWProgressionComponent::ServerSpendPoints_Implementation(const TArray<int32>& Points)
{
	int32 Sum = 0;
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		if (Points[i] < 0 || i >= int32(ENWAttribute::Num))
		{
			return;
		}
		Sum += Points[i];
	}
	if (Sum == 0 || Sum > UnspentPoints())
	{
		return;
	}
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		Spent[i] += Points[i];
	}
	GetOwner()->ForceNetUpdate();
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


// ---------------------------------------------------------------- tutoriais de nível (esteira 4)

void UNWProgressionComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}
	FNWEvents& E = FNWEvents::Get();
	KillHandle = E.OnKill.AddUObject(this, &UNWProgressionComponent::HandleKill);
	TradeHandle = E.OnTradeXp.AddUObject(this, &UNWProgressionComponent::HandleTradeXp);
	RefineHandle = E.OnRefine.AddUObject(this, &UNWProgressionComponent::HandleRefine);
}

void UNWProgressionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	FNWEvents& E = FNWEvents::Get();
	E.OnKill.Remove(KillHandle);
	E.OnTradeXp.Remove(TradeHandle);
	E.OnRefine.Remove(RefineHandle);
	Super::EndPlay(Reason);
}

int32 UNWProgressionComponent::TutorialNum() const
{
	return FProgressionData::Get().Tutorials.Num();
}

FString UNWProgressionComponent::TutorialName(int32 Index) const
{
	const auto& Ts = FProgressionData::Get().Tutorials;
	if (!Ts.IsValidIndex(Index))
	{
		return FString();
	}
	const FString* N = Ts[Index].Name.Find(NWText::Lang());
	return N ? *N : Ts[Index].Id;
}

int32 UNWProgressionComponent::TutorialGoal(int32 Index) const
{
	const auto& Ts = FProgressionData::Get().Tutorials;
	return Ts.IsValidIndex(Index) ? Ts[Index].Count : 0;
}

bool UNWProgressionComponent::TutorialsDone() const
{
	for (int32 C : TutorialCount)
	{
		if (C >= 0)
		{
			return false;
		}
	}
	return TutorialCount.Num() > 0;
}

void UNWProgressionComponent::AdvanceTutorials(const FString& Kind, const FString& Target, int32 Amount)
{
	const auto& Ts = FProgressionData::Get().Tutorials;
	for (int32 i = 0; i < Ts.Num() && i < TutorialCount.Num(); ++i)
	{
		if (TutorialCount[i] < 0 || Ts[i].Kind != Kind || Ts[i].Target != Target)
		{
			continue;
		}
		TutorialCount[i] = FMath::Min(TutorialCount[i] + Amount, Ts[i].Count);
		if (TutorialCount[i] >= Ts[i].Count)
		{
			// Concluída: o XP exato de N níveis pela tabela do jogo (decisão do dono: recompensa em níveis).
			TutorialCount[i] = -1;
			const int32 TargetLevel = FMath::Min(Level + Ts[i].Levels, MaxLevel());
			AddXp(XpForLevel(TargetLevel) - Xp);
			const APawn* Pawn = Cast<APawn>(GetOwner());
			if (ANWPlayerController* PC = Pawn ? Cast<ANWPlayerController>(Pawn->GetController()) : nullptr)
			{
				PC->Warning(NWText::Key(TEXT("nw.tut.done"), {TutorialName(i)}));
			}
		}
		GetOwner()->ForceNetUpdate();
	}
}

void UNWProgressionComponent::HandleKill(AActor* Killer, AActor* Victim)
{
	const ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	const APawn* V = Cast<APawn>(Victim);
	if (!Me || Killer != Me || !V || V->IsPlayerControlled() || !Me->WeaponSets.IsValidIndex(Me->ActiveWeapon))
	{
		return;
	}
	AdvanceTutorials(TEXT("abate"), Me->WeaponSets[Me->ActiveWeapon].Id.ToString(), 1);
	// Depois dos tutoriais: cada abate dá uma fração do XP do nível atual, até o nível máximo da regra.
	const FProgressionData& D = FProgressionData::Get();
	if (TutorialsDone() && Level < D.KillXpMaxLevel && D.KillLevelFraction > 0.f)
	{
		AddXp(FMath::Max<int64>(1, int64((XpForLevel(Level + 1) - XpForLevel(Level)) * D.KillLevelFraction)));
	}
}

void UNWProgressionComponent::HandleTradeXp(ANWCharacter* Who, FName Tradeskill, int32 Amount)
{
	const int32 Skill = NWTradeskill::Index(Tradeskill);
	if (Who == GetOwner() && Skill >= 0 && Skill < NWTradeskill::NumGathering)
	{
		AdvanceTutorials(TEXT("coleta"), Tradeskill.ToString(), 1);
	}
}

void UNWProgressionComponent::HandleRefine(ANWCharacter* Who, const FString& Station, FName Recipe, int32 Count)
{
	if (Who == GetOwner())
	{
		AdvanceTutorials(TEXT("refino"), Station, Count);
	}
}

// Menu de dev (F10): progresso dos tutoriais até existir a tela de missões. O clique mostra as que faltam no aviso.
NW_REGISTER_DEV(Tutorials, TEXT("sistemas.tutorials"), TEXT("nw.dev.tutorials"),
	[](ANWPlayerController* PC)
	{
		const ANWCharacter* C = PC ? Cast<ANWCharacter>(PC->GetPawn()) : nullptr;
		const UNWProgressionComponent* P = C ? C->Progression.Get() : nullptr;
		if (!P)
		{
			return;
		}
		TArray<FString> Left;
		for (int32 i = 0; i < P->TutorialNum() && i < P->TutorialCount.Num(); ++i)
		{
			if (P->TutorialCount[i] >= 0)
			{
				Left.Add(FString::Printf(TEXT("%s %d/%d"), *P->TutorialName(i), P->TutorialCount[i], P->TutorialGoal(i)));
			}
		}
		PC->Warning(Left.IsEmpty() ? NWText::Key(TEXT("nw.tut.all_done")) : FString::Join(Left, TEXT(" · ")));
	},
	[](const ANWPlayerController* PC)
	{
		const ANWCharacter* C = PC ? Cast<ANWCharacter>(PC->GetPawn()) : nullptr;
		const UNWProgressionComponent* P = C ? C->Progression.Get() : nullptr;
		int32 Done = 0;
		for (int32 N : P ? P->TutorialCount : TArray<int32>())
		{
			Done += N < 0 ? 1 : 0;
		}
		return FText::FromString(FString::Printf(TEXT("%d/%d"), Done, P ? P->TutorialNum() : 0));
	});

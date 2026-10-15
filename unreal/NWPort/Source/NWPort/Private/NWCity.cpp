#include "NWCity.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NWBackend.h"
#include "NWCharacter.h"
#include "NWCrafting.h"
#include "NWInteractable.h"
#include "NWInventoryComponent.h"
#include "NWItems.h"
#include "NWOps.h"
#include "NWPlayerController.h"
#include "NWText.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogNWCity, Log, All);

// ---------------------------------------------------------------- state

const FNWCityStation* FNWCityState::StationOf(const FString& Station) const
{
	return Stations.FindByPredicate([&Station](const FNWCityStation& X) { return X.Station == Station; });
}

bool FNWCityState::HasBuff(const FString& Station, const FString& Buff) const
{
	return Buffs.ContainsByPredicate([&](const FNWCityBuff& B) { return B.Station == Station && B.Buff == Buff; });
}

int64 FNWCityState::StockOf(FName Item) const
{
	const FNWCityStock* S = Stock.FindByPredicate([Item](const FNWCityStock& X) { return X.Item == Item; });
	return S ? S->Quantity : 0;
}

TSharedPtr<FJsonObject> FNWCityTaxNote::Json() const
{
	if (!IsSet())
	{
		return nullptr;
	}
	TSharedRef<FJsonObject> T = MakeShared<FJsonObject>();
	T->SetStringField(TEXT("region"), Region);
	T->SetStringField(TEXT("station"), Station);
	T->SetNumberField(TEXT("amount"), double(Amount));
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetObjectField(TEXT("city_tax"), T);
	return Out;
}

namespace
{
	TSharedPtr<FJsonObject> LoadJson(const FString& Name)
	{
		FString Text;
		TSharedPtr<FJsonObject> Root;
		const FString File = FPaths::ProjectDir() / TEXT("Data/World") / Name;
		if (!FFileHelper::LoadFileToString(Text, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
		{
			UE_LOG(LogNWCity, Warning, TEXT("governance: could not read %s"), *File);
			return nullptr;
		}
		return Root;
	}

	TMap<FString, FNWCityState> Cache;
	TMap<FString, double> CacheAt;
	TSet<FString> Fetching;
}

namespace NWCity
{
	const FRules& Rules()
	{
		static FRules R = []()
		{
			FRules Out;
			if (TSharedPtr<FJsonObject> G = LoadJson(TEXT("governanca.json")))
			{
				for (const TSharedPtr<FJsonValue>& V : G->GetArrayField(TEXT("regioes")))
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					Out.CityToRegion.Add(O->GetStringField(TEXT("cidade")), O->GetStringField(TEXT("id")));
					FString Territory;
					if (O->TryGetStringField(TEXT("territorio"), Territory))
					{
						Out.CityTerritory.Add(O->GetStringField(TEXT("cidade")), Territory);
					}
				}
				auto Ups = [](const TArray<TSharedPtr<FJsonValue>>& In, const TCHAR* ToField, TArray<FUp>& Into)
				{
					for (const TSharedPtr<FJsonValue>& V : In)
					{
						const TSharedPtr<FJsonObject> O = V->AsObject();
						FUp U;
						U.To = int32(O->GetNumberField(ToField));
						U.Coins = int64(O->GetNumberField(TEXT("moedas")));
						for (const auto& M : O->GetObjectField(TEXT("materiais"))->Values)
						{
							U.Materials.Add({FName(FString(M.Key)), int64(M.Value->AsNumber())});
						}
						Into.Add(U);
					}
				};
				const TSharedPtr<FJsonObject> Tier = G->GetObjectField(TEXT("mesa_escala"));
				Out.TierInitial = int32(Tier->GetNumberField(TEXT("inicial")));
				Ups(Tier->GetArrayField(TEXT("subir")), TEXT("para"), Out.TierUps);
				for (const auto& E : Tier->GetObjectField(TEXT("efeito"))->Values)
				{
					const TSharedPtr<FJsonObject> O = E.Value->AsObject();
					Out.TierEffect.Add(FCString::Atoi(*FString(E.Key)), {int32(O->GetNumberField(TEXT("gs"))), float(O->GetNumberField(TEXT("qualidade")))});
				}
				const TSharedPtr<FJsonObject> Lv = G->GetObjectField(TEXT("mesa_nivel"));
				Out.LevelMax = int32(Lv->GetNumberField(TEXT("max")));
				Ups(Lv->GetArrayField(TEXT("subir")), TEXT("nivel"), Out.LevelUps);
				Out.GsPerLevel = int32(Lv->GetObjectField(TEXT("efeito"))->GetNumberField(TEXT("gs")));
				Out.QualityPerLevel = float(Lv->GetObjectField(TEXT("efeito"))->GetNumberField(TEXT("qualidade")));
				const TSharedPtr<FJsonObject> Arms = G->GetObjectField(TEXT("armas"));
				for (const TSharedPtr<FJsonValue>& V : Arms->GetArrayField(TEXT("tipos")))
				{
					Out.Weapons.Add(V->AsString());
				}
				Ups(Arms->GetArrayField(TEXT("subir")), TEXT("para"), Out.WeaponUps);
				Out.WorkHours = int32(G->GetNumberField(TEXT("obra_horas")));
				Out.RepairFraction = float(G->GetNumberField(TEXT("conserto_fracao")));
				for (const auto& B : G->GetObjectField(TEXT("buffs"))->Values)
				{
					Out.BuffCost.Add(FString(B.Key), int64(B.Value->AsObject()->GetNumberField(TEXT("custo"))));
					Out.BuffValue.Add(FString(B.Key), float(B.Value->AsObject()->GetNumberField(TEXT("valor"))));
				}
				Out.TaxDefault = float(G->GetObjectField(TEXT("imposto"))->GetNumberField(TEXT("padrao")));
				Out.GraceDays = int32(G->GetNumberField(TEXT("carencia_dias")));
				Out.CoinsMax = int64(G->GetNumberField(TEXT("moedas_max")));
				Out.ItemPriceMax = int64(G->GetNumberField(TEXT("preco_item_max")));
				const TSharedPtr<FJsonObject> Pr = G->GetObjectField(TEXT("premio"));
				Out.TenebMaterial = int64(Pr->GetNumberField(TEXT("tenebroso_materia_prima")));
				Out.TenebList = int64(Pr->GetNumberField(TEXT("tenebroso_lista")));
			}
			if (TSharedPtr<FJsonObject> P = LoadJson(TEXT("precos_referencia.json")))
			{
				for (const auto& I : P->GetObjectField(TEXT("itens"))->Values)
				{
					Out.Reference.Add(FName(I.Key), int64(I.Value->AsObject()->GetNumberField(TEXT("central"))));
				}
			}
			return Out;
		}();
		return R;
	}

	FString RegionOf(const FString& City)
	{
		return Rules().CityToRegion.FindRef(City);
	}

	bool Parse(const TSharedPtr<FJsonObject>& J, const FString& ViewerId, FNWCityState& Out)
	{
		if (!J || !J->HasField(TEXT("region")))
		{
			return false;
		}
		Out = FNWCityState();
		Out.bValid = true;
		Out.Region = J->GetStringField(TEXT("region"));
		Out.Name = J->GetStringField(TEXT("name"));
		FString Owner;
		Out.bHasOwner = J->TryGetStringField(TEXT("owner_id"), Owner) && !Owner.IsEmpty();
		Out.bMine = Out.bHasOwner && Owner == ViewerId;
		J->TryGetStringField(TEXT("owner_name"), Out.OwnerName);
		Out.Price = int64(J->GetNumberField(TEXT("price")));
		FString Since;
		FDateTime When;
		const TSharedPtr<FJsonObject>* Auction = nullptr;
		if (J->TryGetObjectField(TEXT("auction"), Auction))
		{
			const TSharedPtr<FJsonObject>& A = *Auction;
			Out.bAuction = true;
			Out.bAuctionForced = A->GetBoolField(TEXT("forced"));
			Out.AuctionStart = int64(A->GetNumberField(TEXT("start")));
			double Bid = -1.0;
			Out.AuctionBid = A->TryGetNumberField(TEXT("bid"), Bid) ? int64(Bid) : -1;
			A->TryGetStringField(TEXT("bidder_name"), Out.AuctionBidder);
			Out.AuctionMinBid = int64(A->GetNumberField(TEXT("min_bid")));
			Out.bAuctionMine = A->GetBoolField(TEXT("mine"));
			FString Ends;
			if (A->TryGetStringField(TEXT("ends_at"), Ends) && FDateTime::ParseIso8601(*Ends, When))
			{
				Out.AuctionEndsIn = float((When - FDateTime::UtcNow()).GetTotalSeconds());
			}
		}
		if (J->TryGetStringField(TEXT("owner_since"), Since) && FDateTime::ParseIso8601(*Since, When))
		{
			Out.SaleGraceDays = FMath::Max(0.f, float(Rules().GraceDays - (FDateTime::UtcNow() - When).GetTotalDays()));
		}
		Out.Treasury = int64(J->GetNumberField(TEXT("treasury")));
		Out.Tax = float(J->GetNumberField(TEXT("tax")));
		Out.TaxMin = float(J->GetNumberField(TEXT("tax_min")));
		Out.TaxMax = float(J->GetNumberField(TEXT("tax_max")));
		auto Prices = [&J](const TCHAR* Field, TArray<FNWCityPrice>& Into)
		{
			for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(Field))
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FNWCityPrice P;
				P.Item = FName(O->GetStringField(TEXT("item_id")));
				P.Price = int64(O->GetNumberField(TEXT("price")));
				P.Min = int64(O->GetNumberField(TEXT("min")));
				P.Max = int64(O->GetNumberField(TEXT("max")));
				P.Padrao = int64(O->GetNumberField(TEXT("padrao")));
				P.Used = int32(O->GetNumberField(TEXT("used")));
				P.Limit = int32(O->GetNumberField(TEXT("limit")));
				Into.Add(P);
			}
		};
		Prices(TEXT("buy"), Out.Buy);
		Prices(TEXT("sell"), Out.Sell);
		auto EndsIn = [](const TSharedPtr<FJsonObject>& O)
		{
			FString Ends;
			FDateTime At;
			return O->TryGetStringField(TEXT("work_ends_at"), Ends) && FDateTime::ParseIso8601(*Ends, At)
				? float((At - FDateTime::UtcNow()).GetTotalSeconds()) : 0.f;
		};
		for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("stations")))
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FNWCityStation St;
			St.Station = O->GetStringField(TEXT("station"));
			St.Tier = int32(O->GetNumberField(TEXT("tier")));
			St.Level = int32(O->GetNumberField(TEXT("level")));
			St.Reached = int32(O->GetNumberField(TEXT("reached")));
			St.Crafts = int32(O->GetNumberField(TEXT("crafts")));
			O->TryGetStringField(TEXT("work"), St.Work);
			double T = 0.0;
			St.WorkTarget = O->TryGetNumberField(TEXT("work_target"), T) ? int32(T) : 0;
			St.WorkEndsIn = EndsIn(O);
			Out.Stations.Add(St);
		}
		const TArray<TSharedPtr<FJsonValue>>* Arms = nullptr;
		if (J->TryGetArrayField(TEXT("weapons"), Arms))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arms)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FNWCityWeapon W;
				W.Weapon = O->GetStringField(TEXT("weapon"));
				W.Tier = int32(O->GetNumberField(TEXT("tier")));
				double T = 0.0;
				W.WorkTarget = O->TryGetNumberField(TEXT("work_target"), T) ? int32(T) : 0;
				W.WorkEndsIn = EndsIn(O);
				Out.Weapons.Add(W);
			}
		}
		double Defense = 0.0;
		Out.Defense = J->TryGetNumberField(TEXT("defense"), Defense) ? int32(Defense) : 0;
		for (const auto& S : J->GetObjectField(TEXT("stock"))->Values)
		{
			Out.Stock.Add({FName(S.Key), int64(S.Value->AsNumber())});
		}
		for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("buffs")))
		{
			const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
			if (A.Num() == 2)
			{
				Out.Buffs.Add({A[0]->AsString(), A[1]->AsString()});
			}
		}
		FString Next;
		if (J->TryGetStringField(TEXT("next_attack_at"), Next) && FDateTime::ParseIso8601(*Next, When))
		{
			Out.NextAttackIn = float((When - FDateTime::UtcNow()).GetTotalSeconds());
		}
		const TSharedPtr<FJsonObject>* Last = nullptr;
		if (J->TryGetObjectField(TEXT("last_attack"), Last))
		{
			Out.bHasLastAttack = true;
			Out.bLastAttackWon = (*Last)->GetBoolField(TEXT("won"));
			Out.LastAttackStrength = float((*Last)->GetNumberField(TEXT("strength")));
			for (const TSharedPtr<FJsonValue>& V : (*Last)->GetArrayField(TEXT("lost")))
			{
				Out.LastAttackLost.Add(V->AsString());
			}
		}
		Out.Week = J->GetStringField(TEXT("week"));
		const TSharedPtr<FJsonObject>* Week = nullptr;
		if (J->TryGetObjectField(TEXT("this_week"), Week))
		{
			double N = 0.0;
			Out.WeekRawUnits = (*Week)->TryGetNumberField(TEXT("raw_units"), N) ? int64(N) : 0;
			Out.WeekListUnits = (*Week)->TryGetNumberField(TEXT("list_units"), N) ? int64(N) : 0;
			Out.WeekBuffs = (*Week)->TryGetNumberField(TEXT("buffs"), N) ? int32(N) : 0;
		}
		return true;
	}

	const FNWCityState* Cached(const FString& Region)
	{
		return Cache.Find(Region);
	}

	void Store(const FNWCityState& State)
	{
		if (State.bValid)
		{
			Cache.Add(State.Region, State);
			CacheAt.Add(State.Region, FPlatformTime::Seconds());
		}
	}

	void RefreshIfOld(UWorld* World, const FString& Region, double MaxAge)
	{
		if (Region.IsEmpty() || !World || Fetching.Contains(Region))
		{
			return;
		}
		const double* At = CacheAt.Find(Region);
		if (At && FPlatformTime::Seconds() - *At < MaxAge)
		{
			return;
		}
		UGameInstance* GI = World->GetGameInstance();
		UNWBackend* B = GI ? GI->GetSubsystem<UNWBackend>() : nullptr;
		if (!B || !B->IsServerEnabled())
		{
			return;
		}
		Fetching.Add(Region);
		B->GetCity(Region, FString(), [Region](int32 Status, const FString&, TSharedPtr<FJsonObject> Json)
		{
			Fetching.Remove(Region);
			FNWCityState S;
			if (Status == 200 && Parse(Json, FString(), S))
			{
				Store(S);
			}
		});
	}

	void StationBonus(const FNWCityState* City, const FString& Station, int32& OutGs, float& OutQuality)
	{
		OutGs = 0;
		OutQuality = 0.f;
		const FNWCityStation* S = City ? City->StationOf(Station) : nullptr;
		if (!S)
		{
			return;
		}
		if (const TPair<int32, float>* E = Rules().TierEffect.Find(S->Tier))
		{
			OutGs += E->Key;
			OutQuality += E->Value;
		}
		OutGs += S->Level * Rules().GsPerLevel;
		OutQuality += S->Level * Rules().QualityPerLevel;
	}

	const FUp* NextStationUp(const FNWCityStation& S, bool& bOutRepair)
	{
		bOutRepair = false;
		const FRules& R = Rules();
		if (S.Tier < 5)
		{
			return R.TierUps.FindByPredicate([&S](const FUp& U) { return U.To == S.Tier + 1; });
		}
		if (S.Level >= R.LevelMax)
		{
			return nullptr;
		}
		bOutRepair = S.Level < S.Reached;
		return R.LevelUps.FindByPredicate([&S](const FUp& U) { return U.To == S.Level + 1; });
	}

	const FUp* NextWeaponUp(const FNWCityWeapon& W)
	{
		return Rules().WeaponUps.FindByPredicate([&W](const FUp& U) { return U.To == W.Tier + 1; });
	}

	int64 TaxFor(const FString& Region, const TMap<FName, int32>& Spent)
	{
		const FNWCityState* C = Cached(Region);
		const float Rate = C ? C->Tax : Rules().TaxDefault;
		double Value = 0.0;
		for (const TPair<FName, int32>& S : Spent)
		{
			Value += double(Rules().Reference.FindRef(S.Key)) * S.Value;
		}
		return int64(FMath::RoundToDouble(Value * Rate));
	}
}

// ---------------------------------------------------------------- server: city RPCs

namespace
{
	UNWBackend* CityBackend(const UActorComponent* C)
	{
		const UGameInstance* GI = C && C->GetWorld() ? C->GetWorld()->GetGameInstance() : nullptr;
		UNWBackend* B = GI ? GI->GetSubsystem<UNWBackend>() : nullptr;
		return B && B->IsServerEnabled() ? B : nullptr;
	}

	ANWPlayerController* CityPC(const UActorComponent* C)
	{
		const APawn* P = C ? Cast<APawn>(C->GetOwner()) : nullptr;
		return P ? Cast<ANWPlayerController>(P->GetController()) : nullptr;
	}

	void CityWarn(const UActorComponent* C, const FString& Key)
	{
		if (ANWPlayerController* PC = CityPC(C))
		{
			PC->Warning(NWText::Key(Key));
		}
	}

	/** Write of a city operation: 2xx ok; 400/409 nw-core refused (warning with the reason); anything else failed. */
	NWOps::FCommit CityCommit(TWeakObjectPtr<UNWInteractorComponent> Self, TSharedRef<TSharedPtr<FJsonObject>> Result,
		TFunction<void(UNWBackend*, const FNWCharacterRecord&, UNWBackend::FCityDone)> Call)
	{
		return [Self, Result, Call](const FNWCharacterRecord& After, const FNWOpState&, TFunction<void(NWOps::EResult)> Fin)
		{
			UNWBackend* B = Self.IsValid() ? CityBackend(Self.Get()) : nullptr;
			if (!B)
			{
				Fin(NWOps::EResult::Failed);
				return;
			}
			Call(B, After, [Self, Result, Fin](int32 Status, const FString& Code, TSharedPtr<FJsonObject> Json)
			{
				if (Status >= 200 && Status < 300)
				{
					*Result = Json;
					Fin(NWOps::EResult::Ok);
					return;
				}
				if (Status == 400 || Status == 409)
				{
					CityWarn(Self.Get(), TEXT("nw.city.") + (Code.IsEmpty() ? FString(TEXT("failed")) : Code));
					Fin(NWOps::EResult::Rejected);
					return;
				}
				Fin(NWOps::EResult::Failed);
			});
		};
	}
}

bool UNWInteractorComponent::NearCityBoard() const
{
	const AActor* Me = GetOwner();
	for (TActorIterator<ANWStation> It(GetWorld()); It && Me; ++It)
	{
		if ((It->Type == TEXT("cityboard") || It->Type == TEXT("governance"))
			&& FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation()) < 600.f && !NWCity::RegionOf(It->City).IsEmpty())
		{
			return true;
		}
	}
	return false;
}

void UNWInteractorComponent::CitySend(TSharedPtr<FJsonObject> Json, TFunction<void()> Then)
{
	ANWPlayerController* PC = CityPC(this);
	UNWBackend* B = CityBackend(this);
	const FString Me = PC && PC->Record ? PC->Record->Id : FString();
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	auto Send = [Self, Me, Then](TSharedPtr<FJsonObject> J)
	{
		if (!Self.IsValid())
		{
			return;
		}
		FNWCityState S;
		if (NWCity::Parse(J, Me, S))
		{
			FNWCityState Shared = S;
			Shared.bMine = false;
			NWCity::Store(Shared);
		}
		if (Self->bCityOpen)
		{
			Self->ClientCityState(S);
		}
		if (Then)
		{
			Then();
		}
	};
	if (Json)
	{
		Send(Json);
		return;
	}
	if (!B || CityRegion.IsEmpty())
	{
		Send(nullptr);
		return;
	}
	B->GetCity(CityRegion, Me, [Send](int32 Status, const FString&, TSharedPtr<FJsonObject> J) { Send(Status == 200 ? J : nullptr); });
}

void UNWInteractorComponent::CityFinish(int32 Req, bool bOk, TSharedPtr<FJsonObject> Json)
{
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	CitySend(bOk ? Json : nullptr, [Self, Req, bOk]() { if (Self.IsValid()) { Self->ClientCityDone(Req, bOk); } });
}

void UNWInteractorComponent::ClientCityState_Implementation(const FNWCityState& State)
{
	CityState = State;
	++CitySeq;
}

void UNWInteractorComponent::ClientCityDone_Implementation(int32 Req, bool bOk)
{
	CityDone.Add({Req, bOk});
}

void UNWInteractorComponent::ServerCityOpen_Implementation()
{
	const AActor* Me = GetOwner();
	CityRegion.Reset();
	for (TActorIterator<ANWStation> It(GetWorld()); It && Me; ++It)
	{
		if ((It->Type == TEXT("cityboard") || It->Type == TEXT("governance")) && FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation()) < 600.f)
		{
			CityRegion = NWCity::RegionOf(It->City);
			break;
		}
	}
	bCityOpen = !CityRegion.IsEmpty();
	if (bCityOpen)
	{
		MarketClaimCredits();   // governor prize and city sale arrive as credit
		CitySend();
	}
	else
	{
		ClientCityState(FNWCityState());
	}
}

void UNWInteractorComponent::ServerCityClose_Implementation()
{
	bCityOpen = false;
}

void UNWInteractorComponent::ServerCityRefresh_Implementation()
{
	if (bCityOpen && NearCityBoard())
	{
		CitySend();
	}
}

void UNWInteractorComponent::ServerCitySellRaw_Implementation(int32 Req, FName Item, int32 Quantity, int64 UnitPrice)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!Me || !bCityOpen || !NearCityBoard() || Quantity <= 0 || UnitPrice <= 0 || !CityBackend(this))
	{
		ClientCityDone(Req, false);
		return;
	}
	const FString Region = CityRegion;
	const FString Reason = FString::Printf(TEXT("city_sell:%s:%d"), *Item.ToString(), Quantity);
	const int64 Total = UnitPrice * Quantity;
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	TSharedRef<TSharedPtr<FJsonObject>> Result = MakeShared<TSharedPtr<FJsonObject>>();
	NWOps::Run(Me, Reason,
		[Self, Me, Item, Quantity, Total]()
		{
			if (Me->Inventory->CountOf(Item) < Quantity)
			{
				CityWarn(Self.Get(), TEXT("nw.city.missing"));
				return false;
			}
			Me->Inventory->Take(Item, Quantity);
			Me->Inventory->AddCoins(Total);
			TWeakObjectPtr<UNWInventoryComponent> Inv(Me->Inventory);
			NWOps::Hold(Me, [Inv, Total]() { if (Inv.IsValid()) { Inv->ClientLoot({}, Total); } });
			return true;
		},
		CityCommit(Self, Result, [Region, Item, Quantity, UnitPrice, Reason](UNWBackend* B, const FNWCharacterRecord& After, UNWBackend::FCityDone Done)
		{
			B->CityTrade(After, Region, true, Item, Quantity, UnitPrice, Reason, MoveTemp(Done));
		}),
		[Self, Req, Result](bool bOk) { if (Self.IsValid()) { Self->CityFinish(Req, bOk, *Result); } });
}

void UNWInteractorComponent::ServerCityBuyItem_Implementation(int32 Req, FName Item, int32 Quantity, int64 UnitPrice)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!Me || !bCityOpen || !NearCityBoard() || Quantity <= 0 || UnitPrice <= 0 || !CityBackend(this) || !FNWItemCatalog::Get().Find(Item))
	{
		ClientCityDone(Req, false);
		return;
	}
	const FString Region = CityRegion;
	const FString Reason = FString::Printf(TEXT("city_buy:%s:%d"), *Item.ToString(), Quantity);
	const int64 Total = UnitPrice * Quantity;
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	TSharedRef<TSharedPtr<FJsonObject>> Result = MakeShared<TSharedPtr<FJsonObject>>();
	NWOps::Run(Me, Reason,
		[Self, Me, Item, Quantity, Total]()
		{
			if (Me->Inventory->Coins < Total)
			{
				CityWarn(Self.Get(), TEXT("ui_insufficient_funds"));
				return false;
			}
			FNWItem Got;
			Got.ItemId = Item;
			Got.Quantity = Quantity;
			Me->Inventory->Grant({Got}, -Total);
			return true;
		},
		CityCommit(Self, Result, [Region, Item, Quantity, UnitPrice, Reason](UNWBackend* B, const FNWCharacterRecord& After, UNWBackend::FCityDone Done)
		{
			B->CityTrade(After, Region, false, Item, Quantity, UnitPrice, Reason, MoveTemp(Done));
		}),
		[Self, Req, Result](bool bOk) { if (Self.IsValid()) { Self->CityFinish(Req, bOk, *Result); } });
}

void UNWInteractorComponent::ServerCityBid_Implementation(int32 Req, int64 Amount)
{
	// Bid in the city auction: leaves the coins now and comes back as credit if someone outbids it.
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!Me || !bCityOpen || !NearCityBoard() || Amount <= 0 || !CityBackend(this))
	{
		ClientCityDone(Req, false);
		return;
	}
	const FString Region = CityRegion;
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	TSharedRef<TSharedPtr<FJsonObject>> Result = MakeShared<TSharedPtr<FJsonObject>>();
	NWOps::Run(Me, TEXT("city_bid"),
		[Self, Me, Amount]()
		{
			if (Me->Inventory->Coins < Amount)
			{
				CityWarn(Self.Get(), TEXT("ui_insufficient_funds"));
				return false;
			}
			Me->Inventory->AddCoins(-Amount);
			return true;
		},
		CityCommit(Self, Result, [Region, Amount](UNWBackend* B, const FNWCharacterRecord& After, UNWBackend::FCityDone Done)
		{
			B->CityBid(After, Region, Amount, TEXT("city_bid"), MoveTemp(Done));
		}),
		[Self, Req, Result](bool bOk) { if (Self.IsValid()) { Self->CityFinish(Req, bOk, *Result); } });
}

void UNWInteractorComponent::ServerCityBuy_Implementation(int32 Req, int64 Price)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!Me || !bCityOpen || !NearCityBoard() || Price <= 0 || !CityBackend(this))
	{
		ClientCityDone(Req, false);
		return;
	}
	const FString Region = CityRegion;
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	TSharedRef<TSharedPtr<FJsonObject>> Result = MakeShared<TSharedPtr<FJsonObject>>();
	NWOps::Run(Me, TEXT("city_buy_city"),
		[Self, Me, Price]()
		{
			if (Me->Inventory->Coins < Price)
			{
				CityWarn(Self.Get(), TEXT("ui_insufficient_funds"));
				return false;
			}
			Me->Inventory->AddCoins(-Price);
			return true;
		},
		CityCommit(Self, Result, [Region, Price](UNWBackend* B, const FNWCharacterRecord& After, UNWBackend::FCityDone Done)
		{
			B->CityBuy(After, Region, Price, TEXT("city_buy_city"), MoveTemp(Done));
		}),
		[Self, Req, Result](bool bOk) { if (Self.IsValid()) { Self->CityFinish(Req, bOk, *Result); } });
}

void UNWInteractorComponent::CityGovernor(int32 Req, const FString& Verb, const FString& Sub, TSharedRef<FJsonObject> Body)
{
	// Governor action without bag or coins: nw-core checks that it is the owner.
	UNWBackend* B = CityBackend(this);
	ANWPlayerController* PC = CityPC(this);
	if (!B || !PC || !PC->Record)
	{
		ClientCityDone(Req, false);
		return;
	}
	Body->SetStringField(TEXT("character_id"), PC->Record->Id);
	TWeakObjectPtr<UNWInteractorComponent> Self(this);
	B->CityAction(Verb, CityRegion, Sub, Body, [Self, Req](int32 Status, const FString& Code, TSharedPtr<FJsonObject> Json)
	{
		if (!Self.IsValid())
		{
			return;
		}
		const bool bOk = Status == 200;
		if (!bOk)
		{
			CityWarn(Self.Get(), TEXT("nw.city.") + (Code.IsEmpty() ? FString(TEXT("failed")) : Code));
		}
		Self->CityFinish(Req, bOk, Json);
	});
}

void UNWInteractorComponent::ServerCitySale_Implementation(int32 Req, int64 Price)
{
	if (!bCityOpen || !NearCityBoard())
	{
		ClientCityDone(Req, false);
		return;
	}
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	if (Price > 0)
	{
		Body->SetNumberField(TEXT("price"), double(Price));
	}
	else
	{
		Body->SetField(TEXT("price"), MakeShared<FJsonValueNull>());
	}
	CityGovernor(Req, TEXT("POST"), TEXT("sale"), Body);
}

void UNWInteractorComponent::ServerCityManage_Implementation(int32 Req, const TArray<FNWCityPrice>& Buy, const TArray<FNWCityPrice>& Sell, float Tax)
{
	if (!bCityOpen || !NearCityBoard())
	{
		ClientCityDone(Req, false);
		return;
	}
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	auto Prices = [](const TArray<FNWCityPrice>& List)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		for (const FNWCityPrice& P : List)
		{
			O->SetNumberField(P.Item.ToString(), double(P.Price));
		}
		return O;
	};
	if (Buy.Num())
	{
		Body->SetObjectField(TEXT("buy_prices"), Prices(Buy));
	}
	if (Sell.Num())
	{
		Body->SetObjectField(TEXT("sell_prices"), Prices(Sell));
	}
	if (Tax >= 0.f)
	{
		Body->SetNumberField(TEXT("tax"), Tax);
	}
	CityGovernor(Req, TEXT("PUT"), FString(), Body);
}

void UNWInteractorComponent::ServerCityProject_Implementation(int32 Req, const FString& Kind, const FString& Station, const FString& Buff)
{
	if (!bCityOpen || !NearCityBoard())
	{
		ClientCityDone(Req, false);
		return;
	}
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("kind"), Kind);
	if (!Station.IsEmpty())
	{
		// Weapon: the name goes in the "weapon" field; station and buff in "station".
		Body->SetStringField(Kind == TEXT("weapon") ? TEXT("weapon") : TEXT("station"), Station);
	}
	if (!Buff.IsEmpty())
	{
		Body->SetStringField(TEXT("buff"), Buff);
	}
	CityGovernor(Req, TEXT("POST"), TEXT("projects"), Body);
}

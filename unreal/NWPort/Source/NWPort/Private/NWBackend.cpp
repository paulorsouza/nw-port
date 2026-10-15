#include "NWBackend.h"
#include "NWText.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogNWBackend, Log, All);

static TSharedPtr<FJsonObject> ParseJson(const FString& Text)
{
	TSharedPtr<FJsonObject> Obj;
	// Resposta em lista (ex. sacos no chão): vem embrulhada em {"__array": [...]}.
	if (Text.TrimStart().StartsWith(TEXT("[")))
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Arr))
		{
			Obj = MakeShared<FJsonObject>();
			Obj->SetArrayField(TEXT("__array"), Arr);
		}
		return Obj;
	}
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Obj);
	return Obj;
}

static FString WriteJson(const TSharedRef<FJsonObject>& Obj)
{
	FString Out;
	FJsonSerializer::Serialize(Obj, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out));
	return Out;
}

TOptional<FNWCharacterRecord> FNWCharacterRecord::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid() || !Obj->HasTypedField<EJson::String>(TEXT("id")))
	{
		return {};
	}
	FNWCharacterRecord R;
	R.Id = Obj->GetStringField(TEXT("id"));
	R.Name = Obj->GetStringField(TEXT("name"));
	R.Map = Obj->GetStringField(TEXT("map"));
	R.ActiveWeapon = Obj->GetStringField(TEXT("active_weapon"));
	R.Yaw = Obj->GetNumberField(TEXT("yaw"));
	double Health = 0.0;
	if (Obj->TryGetNumberField(TEXT("health"), Health))
	{
		R.Health = Health;
	}
	const TSharedPtr<FJsonObject>* Pos = nullptr;
	if (Obj->TryGetObjectField(TEXT("position"), Pos))
	{
		R.bHasPosition = true;
		R.Position = FVector((*Pos)->GetNumberField(TEXT("x")), (*Pos)->GetNumberField(TEXT("y")), (*Pos)->GetNumberField(TEXT("z")));
	}
	const TSharedPtr<FJsonObject>* Field = nullptr;
	R.Loadout = Obj->TryGetObjectField(TEXT("loadout"), Field) ? *Field : MakeShared<FJsonObject>();
	R.Mastery = Obj->TryGetObjectField(TEXT("mastery"), Field) ? *Field : MakeShared<FJsonObject>();
	R.Progression = Obj->TryGetObjectField(TEXT("progression"), Field) ? *Field : MakeShared<FJsonObject>();
	double Coins = 0.0;
	if (Obj->TryGetNumberField(TEXT("coins"), Coins))
	{
		R.Coins = int64(Coins);
	}
	const TArray<TSharedPtr<FJsonValue>>* Inv = nullptr;
	if (Obj->TryGetArrayField(TEXT("inventory"), Inv))
	{
		for (const TSharedPtr<FJsonValue>& V : *Inv)
		{
			const TSharedPtr<FJsonObject> I = V->AsObject();
			if (!I.IsValid())
			{
				continue;
			}
			FNWItem Item;
			Item.ItemId = FName(I->GetStringField(TEXT("item_id")));
			Item.Quantity = I->GetIntegerField(TEXT("quantity"));
			Item.Container = I->GetStringField(TEXT("container")) == TEXT("equip") ? ENWItemContainer::Equip : ENWItemContainer::Bag;
			Item.Slot = I->GetIntegerField(TEXT("slot"));
			Item.GearScore = I->GetIntegerField(TEXT("gear_score"));
			Item.Rarity = uint8(I->GetIntegerField(TEXT("rarity")));
			const TSharedPtr<FJsonObject>* Props = nullptr;
			if (I->TryGetObjectField(TEXT("props"), Props))
			{
				NWItemProps::Read(Item, *Props);
			}
			R.Items.Add(Item);
		}
	}
	return R;
}

FString FNWCharacterRecord::StateJson() const
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("map"), Map);
	if (bHasPosition)
	{
		TSharedRef<FJsonObject> Pos = MakeShared<FJsonObject>();
		Pos->SetNumberField(TEXT("x"), Position.X);
		Pos->SetNumberField(TEXT("y"), Position.Y);
		Pos->SetNumberField(TEXT("z"), Position.Z);
		Obj->SetObjectField(TEXT("position"), Pos);
	}
	else
	{
		Obj->SetField(TEXT("position"), MakeShared<FJsonValueNull>());
	}
	Obj->SetNumberField(TEXT("yaw"), Yaw);
	if (Health >= 0.f)
	{
		Obj->SetNumberField(TEXT("health"), Health);
	}
	else
	{
		Obj->SetField(TEXT("health"), MakeShared<FJsonValueNull>());
	}
	Obj->SetStringField(TEXT("active_weapon"), ActiveWeapon);
	Obj->SetObjectField(TEXT("loadout"), Loadout.IsValid() ? Loadout : MakeShared<FJsonObject>());
	Obj->SetObjectField(TEXT("mastery"), Mastery.IsValid() ? Mastery : MakeShared<FJsonObject>());
	// O servidor de jogo é autoritativo no inventário: manda o conjunto inteiro a cada salvamento.
	Obj->SetNumberField(TEXT("coins"), double(Coins));
	TArray<TSharedPtr<FJsonValue>> Inv;
	for (const FNWItem& Item : Items)
	{
		TSharedRef<FJsonObject> I = MakeShared<FJsonObject>();
		I->SetStringField(TEXT("item_id"), Item.ItemId.ToString());
		I->SetNumberField(TEXT("quantity"), Item.Quantity);
		I->SetStringField(TEXT("container"), Item.Container == ENWItemContainer::Equip ? TEXT("equip") : TEXT("bag"));
		I->SetNumberField(TEXT("slot"), Item.Slot);
		I->SetNumberField(TEXT("gear_score"), Item.GearScore);
		I->SetNumberField(TEXT("rarity"), Item.Rarity);
		I->SetObjectField(TEXT("props"), NWItemProps::Write(Item));
		Inv.Add(MakeShared<FJsonValueObject>(I));
	}
	Obj->SetArrayField(TEXT("inventory"), Inv);
	if (Progression.IsValid())
	{
		Obj->SetObjectField(TEXT("progression"), Progression);
	}
	if (StorageCapacity >= 0.f)
	{
		Obj->SetNumberField(TEXT("storage_capacity"), StorageCapacity);
	}
	return WriteJson(Obj);
}

static const TCHAR* SessionSection = TEXT("NW.Session");

void UNWBackend::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GConfig->GetString(TEXT("NW.Server"), TEXT("Backend"), BaseUrl, GGameIni);
	GConfig->GetString(TEXT("NW.Server"), TEXT("Address"), ConnectAddress, GGameIni);
	// Testes: -nwexitafter=N fecha o processo depois de N s (clientes sem render, -nullrhi, não tiram print).
	float ExitAfter = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("-nwexitafter="), ExitAfter) && ExitAfter > 0.f)
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { FPlatformMisc::RequestExit(false, TEXT("nwexitafter")); return false; }), ExitAfter);
	}
	FParse::Value(FCommandLine::Get(), TEXT("-nwbackend="), BaseUrl);
	FParse::Value(FCommandLine::Get(), TEXT("-nwconnect="), ConnectAddress);
	BaseUrl.RemoveFromEnd(TEXT("/"));
	ServerSecret = FPlatformMisc::GetEnvironmentVariable(TEXT("NW_SERVER_SECRET"));
	if (IsRunningDedicatedServer() || GetGameInstance()->IsDedicatedServerInstance())
	{
		UE_LOG(LogNWBackend, Display, TEXT("servidor: backend %s, passe %s"), BaseUrl.IsEmpty() ? TEXT("(nenhum)") : *BaseUrl,
			IsServerEnabled() ? TEXT("exigido") : TEXT("dispensado"));
		return;
	}
	ServerSecret.Reset();  // segredo nunca fica no cliente
	if (BaseUrl.IsEmpty())
	{
		return;
	}
	GConfig->GetString(SessionSection, TEXT("Account"), AccountName, GGameUserSettingsIni);
	GConfig->GetString(SessionSection, TEXT("Token"), SessionToken, GGameUserSettingsIni);
	NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UNWBackend::HandleNetworkFailure);
	TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UNWBackend::HandleTravelFailure);
	ClientAutoConnect();
}

void UNWBackend::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	Super::Deinitialize();
}

bool UNWBackend::WantsFrontEnd(const UWorld* World) const
{
	return !BaseUrl.IsEmpty() && !bAutoConnect && !bPie && World && World->GetNetMode() == NM_Standalone;
}

void UNWBackend::HandleNetworkFailure(UWorld*, UNetDriver*, ENetworkFailure::Type Type, const FString& Error)
{
	// Recusa do nosso servidor vem com "#nw.err.*" no texto (traduzida na tela); versão diferente tem chave própria.
	const int32 KeyAt = Error.Find(TEXT("#nw."));
	if (Type == ENetworkFailure::OutdatedClient || Type == ENetworkFailure::OutdatedServer)
	{
		ConnectionError = NWText::Key(TEXT("nw.err.incompatible"));
	}
	else if (KeyAt != INDEX_NONE)
	{
		ConnectionError = Error.Mid(KeyAt);
	}
	else
	{
		ConnectionError = Error.IsEmpty() ? FString(ENetworkFailure::ToString(Type)) : Error;
	}
	UE_LOG(LogNWBackend, Warning, TEXT("falha de rede: %s"), *ConnectionError);
}

void UNWBackend::HandleTravelFailure(UWorld*, ETravelFailure::Type Type, const FString& Error)
{
	ConnectionError = Error.IsEmpty() ? FString(ETravelFailure::ToString(Type)) : Error;
	UE_LOG(LogNWBackend, Warning, TEXT("falha de viagem: %s"), *ConnectionError);
}

void UNWBackend::Request(const FString& Verb, const FString& Path, const FString& Body, bool bServer, FJsonDone Done)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(BaseUrl + Path);
	Req->SetVerb(Verb);
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (bServer)
	{
		Req->SetHeader(TEXT("X-NW-Server-Secret"), ServerSecret);
	}
	else if (!SessionToken.IsEmpty())
	{
		Req->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + SessionToken);
	}
	if (!Body.IsEmpty())
	{
		Req->SetContentAsString(Body);
	}
	Req->SetTimeout(10.f);
	Req->OnProcessRequestComplete().BindLambda([Weak = TWeakObjectPtr<UNWBackend>(this), Done = MoveTemp(Done), Verb, Path](FHttpRequestPtr, FHttpResponsePtr Res, bool bConnected)
	{
		if (!Weak.IsValid())
		{
			return;
		}
		const int32 Status = (bConnected && Res.IsValid()) ? Res->GetResponseCode() : 0;
		TSharedPtr<FJsonObject> Json = (Status > 0) ? ParseJson(Res->GetContentAsString()) : nullptr;
		if (Status == 0 || Status >= 400)
		{
			UE_LOG(LogNWBackend, Warning, TEXT("%s %s -> %d %s"), *Verb, *Path, Status, Res.IsValid() ? *Res->GetContentAsString() : TEXT("(sem conexão)"));
		}
		Done(Status, Json);
	});
	Req->ProcessRequest();
}

FString UNWBackend::ErrorMessage(int32 Status, const TSharedPtr<FJsonObject>& Body)
{
	// O nw-core manda {"error": código, "message": texto em pt}; o cliente traduz pelo código.
	FString Code, Msg;
	if (Body.IsValid() && Body->TryGetStringField(TEXT("error"), Code))
	{
		const FString Key = TEXT("nw.err.") + Code;
		const FString Text = NWText::Get(Key);
		if (Text != Key)
		{
			return Text;
		}
	}
	if (Body.IsValid() && Body->TryGetStringField(TEXT("message"), Msg))
	{
		return Msg;
	}
	return Status == 0 ? NWText::Get(TEXT("nw.err.backend_down")) : NWText::Format(TEXT("nw.err.backend_status"), {FString::FromInt(Status)});
}

// ---------------------------------------------------------------- servidor de jogo

void UNWBackend::RedeemTicket(const FString& Ticket, FRedeemDone Done)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("ticket"), Ticket);
	Request(TEXT("POST"), TEXT("/internal/tickets/redeem"), WriteJson(Body), true,
		[Weak = TWeakObjectPtr<UNWBackend>(this), Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
	{
		if (Status != 200 || !Json.IsValid())
		{
			Done({}, ErrorMessage(Status, Json));
			return;
		}
		const TSharedPtr<FJsonObject>* Character = nullptr;
		TOptional<FNWCharacterRecord> Record;
		if (Json->TryGetObjectField(TEXT("character"), Character))
		{
			Record = FNWCharacterRecord::FromJson(*Character);
		}
		if (!Record)
		{
			Done({}, NWText::Get(TEXT("nw.err.bad_response")));
			return;
		}
		Record->AccountName = Json->GetStringField(TEXT("account_name"));
		Json->TryGetBoolField(TEXT("first_join"), Record->bFirstJoin);
		Json->TryGetBoolField(TEXT("is_admin"), Record->bAdmin);
		// Sessão nova do personagem: as gravações dele recomeçam do número 1.
		int64 Session = 0;
		Json->TryGetNumberField(TEXT("session"), Session);
		if (Weak.IsValid())
		{
			FWriteQueue& Q = Weak->Writes.FindOrAdd(Record->Id);
			Q.Session = Session;
			Q.NextSeq = 0;
			Weak->RememberBag(*Record);   // o que o nw-core tem agora
		}
		Done(Record, FString());
	});
}

void UNWBackend::Write(const FString& CharacterId, const FString& Verb, const FString& Path, TSharedRef<FJsonObject> Body, const FString& Reason,
	TFunction<void(int32, TSharedPtr<FJsonObject>)> Done)
{
	FWriteQueue* Q = Writes.Find(CharacterId);
	if (!Q || Q->Session < 0)
	{
		UE_LOG(LogNWBackend, Error, TEXT("%s %s: personagem sem sessão aberta; gravação não enviada"), *Verb, *Path);
		Done(0, nullptr);
		return;
	}
	Body->SetNumberField(TEXT("session"), double(Q->Session));
	Body->SetNumberField(TEXT("seq"), double(++Q->NextSeq));
	Body->SetStringField(TEXT("reason"), Reason.Left(80));
	Q->Pending.Add({Verb, Path, Body, MoveTemp(Done)});
	PumpWrites(CharacterId);
}

void UNWBackend::PumpWrites(const FString& CharacterId)
{
	FWriteQueue* Q = Writes.Find(CharacterId);
	if (!Q || Q->bBusy || Q->Pending.Num() == 0)
	{
		return;
	}
	Q->bBusy = true;
	const FPendingWrite& W = Q->Pending[0];
	Request(W.Verb, W.Path, WriteJson(W.Body.ToSharedRef()), true, [Weak = TWeakObjectPtr<UNWBackend>(this), CharacterId](int32 Status, TSharedPtr<FJsonObject> Json)
	{
		UNWBackend* Self = Weak.Get();
		FWriteQueue* Q = Self ? Self->Writes.Find(CharacterId) : nullptr;
		if (!Q || Q->Pending.Num() == 0)
		{
			return;
		}
		FPendingWrite& W = Q->Pending[0];
		if ((Status == 0 || Status >= 500) && ++W.Tries < MaxWriteTries)
		{
			// Sem resposta: mesmo número de novo, esperando 1, 2, 4, 8, 16 s. A fila fica parada (ordem).
			const float Wait = float(1 << (W.Tries - 1));
			UE_LOG(LogNWBackend, Warning, TEXT("%s %s: nova tentativa em %.0f s"), *W.Verb, *W.Path, Wait);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, CharacterId](float)
			{
				if (UNWBackend* S = Weak.Get())
				{
					if (FWriteQueue* Q2 = S->Writes.Find(CharacterId))
					{
						Q2->bBusy = false;
						S->PumpWrites(CharacterId);
					}
				}
				return false;
			}), Wait);
			return;
		}
		FString Code;
		if (Status == 409 && Json && Json->TryGetStringField(TEXT("error"), Code) && Code == TEXT("duplicate_write"))
		{
			Status = 208;   // já estava gravado
		}
		if (Status == 0 || Status >= 400)
		{
			UE_LOG(LogNWBackend, Error, TEXT("%s %s: gravação %s recusada ou perdida (%d)"), *W.Verb, *W.Path,
				*FString::Printf(TEXT("%lld"), int64(W.Body->GetNumberField(TEXT("seq")))), Status);
		}
		TFunction<void(int32, TSharedPtr<FJsonObject>)> Done = MoveTemp(W.Done);
		Q->Pending.RemoveAt(0);
		Q->bBusy = false;
		Done(Status, Json);
		Self->PumpWrites(CharacterId);
	});
}

void UNWBackend::SaveState(const FNWCharacterRecord& Record, const FString& Reason, TFunction<void(int32)> Done, TSharedPtr<FJsonObject> Extra)
{
	TSharedPtr<FJsonObject> Body = ParseJson(Record.StateJson());
	if (!Body)
	{
		if (Done)
		{
			Done(0);
		}
		return;
	}
	if (Extra)
	{
		for (const auto& F : Extra->Values)
		{
			Body->SetField(F.Key, F.Value);
		}
	}
	// Só os espaços que mudaram desde a última gravação (docs/arquitetura-servidor.md, "Banco"): nada mudou, a bolsa não
	// vai. Cada espaço leva o valor final, então uma diferença calculada antes de outra gravação terminar continua certa.
	if (const TArray<FNWItem>* Old = PersistedBag.Find(Record.Id))
	{
		auto Key = [](const FNWItem& I) { return (I.Container == ENWItemContainer::Equip ? 100000 : 0) + I.Slot; };
		TMap<int32, const FNWItem*> Before, After;
		for (const FNWItem& I : *Old)
		{
			Before.Add(Key(I), &I);
		}
		for (const FNWItem& I : Record.Items)
		{
			After.Add(Key(I), &I);
		}
		TArray<TSharedPtr<FJsonValue>> Patch;
		auto Slot = [&Patch](int32 K, const FNWItem* Item)
		{
			TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
			P->SetStringField(TEXT("container"), K >= 100000 ? TEXT("equip") : TEXT("bag"));
			P->SetNumberField(TEXT("slot"), K % 100000);
			if (Item)
			{
				TSharedRef<FJsonObject> I = MakeShared<FJsonObject>();
				I->SetStringField(TEXT("item_id"), Item->ItemId.ToString());
				I->SetNumberField(TEXT("quantity"), Item->Quantity);
				I->SetNumberField(TEXT("gear_score"), Item->GearScore);
				I->SetNumberField(TEXT("rarity"), Item->Rarity);
				I->SetObjectField(TEXT("props"), NWItemProps::Write(*Item));
				P->SetObjectField(TEXT("item"), I);
			}
			else
			{
				P->SetField(TEXT("item"), MakeShared<FJsonValueNull>());
			}
			Patch.Add(MakeShared<FJsonValueObject>(P));
		};
		for (const TPair<int32, const FNWItem*>& B : Before)
		{
			const FNWItem* const* A = After.Find(B.Key);
			if (!A)
			{
				Slot(B.Key, nullptr);
			}
			else if (!(*A)->SameKind(*B.Value) || (*A)->Quantity != B.Value->Quantity)
			{
				Slot(B.Key, *A);
			}
		}
		for (const TPair<int32, const FNWItem*>& A : After)
		{
			if (!Before.Contains(A.Key))
			{
				Slot(A.Key, A.Value);
			}
		}
		Body->RemoveField(TEXT("inventory"));
		if (Patch.Num() > 0)
		{
			Body->SetArrayField(TEXT("inventory_patch"), Patch);
		}
	}
	const FString Name = Record.Name;
	const FString Id = Record.Id;
	const TArray<FNWItem> Sent = Record.Items;
	Write(Record.Id, TEXT("PUT"), FString::Printf(TEXT("/internal/characters/%s/state"), *Record.Id), Body.ToSharedRef(), Reason,
		[Weak = TWeakObjectPtr<UNWBackend>(this), Name, Id, Sent, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject>)
		{
			if (Status == 204 || Status == 208)
			{
				UE_LOG(LogNWBackend, Verbose, TEXT("salvo: %s"), *Name);
				if (Weak.IsValid())
				{
					Weak->PersistedBag.Add(Id, Sent);
				}
			}
			if (Done)
			{
				Done(Status);
			}
		});
}

void UNWBackend::ListNpcRewards(const FString& CharacterId, const FString& Period, TFunction<void(bool, const TArray<FString>&)> Done)
{
	Request(TEXT("GET"), FString::Printf(TEXT("/internal/characters/%s/npc-rewards?period=%s"), *CharacterId, *Period), FString(), true,
		[Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
		{
			TArray<FString> Keys;
			const TArray<TSharedPtr<FJsonValue>>* Claims = nullptr;
			if (Status != 200 || !Json || !Json->TryGetArrayField(TEXT("claims"), Claims))
			{
				Done(false, Keys);
				return;
			}
			for (const TSharedPtr<FJsonValue>& V : *Claims)
			{
				const TSharedPtr<FJsonObject> C = V->AsObject();
				Keys.Add(C->GetStringField(TEXT("kind")) + TEXT(":") + C->GetStringField(TEXT("npc")) + TEXT(":") + C->GetStringField(TEXT("period")));
			}
			Done(true, Keys);
		});
}

void UNWBackend::ClaimNpcReward(const FNWCharacterRecord& Record, const FString& Npc, const FString& Kind, const FString& Period,
	TFunction<void(bool, int32)> Done)
{
	// Corpo = estado do personagem (a rota usa só inventory e coins) + npc, kind, period.
	TSharedPtr<FJsonObject> Body = ParseJson(Record.StateJson());
	if (!Body)
	{
		Done(false, 0);
		return;
	}
	Body->SetStringField(TEXT("npc"), Npc);
	Body->SetStringField(TEXT("kind"), Kind);
	Body->SetStringField(TEXT("period"), Period);
	Write(Record.Id, TEXT("POST"), FString::Printf(TEXT("/internal/characters/%s/npc-rewards"), *Record.Id), Body.ToSharedRef(),
		FString::Printf(TEXT("npc_reward:%s:%s"), *Kind, *Npc),
		[Weak = TWeakObjectPtr<UNWBackend>(this), Record, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject>)
		{
			if ((Status == 201 || Status == 208) && Weak.IsValid())
			{
				Weak->RememberBag(Record);
			}
			Done(Status == 201 || Status == 208, Status == 208 ? 201 : Status);
		});
}

namespace
{
	TArray<FNWItem> StorageItemsFrom(const TSharedPtr<FJsonObject>& Json)
	{
		TArray<FNWItem> Out;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Json && Json->TryGetArrayField(TEXT("items"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject> I = V->AsObject();
				FNWItem Item;
				Item.ItemId = FName(*I->GetStringField(TEXT("item_id")));
				Item.Quantity = int32(I->GetNumberField(TEXT("quantity")));
				Item.Slot = int32(I->GetNumberField(TEXT("slot")));
				Item.GearScore = int32(I->GetNumberField(TEXT("gear_score")));
				Item.Rarity = uint8(I->GetNumberField(TEXT("rarity")));
				int32 Tab = 0;
				I->TryGetNumberField(TEXT("tab"), Tab);
				Item.Tab = uint8(FMath::Clamp(Tab, 0, 4));
				const TSharedPtr<FJsonObject>* Props = nullptr;
				if (I->TryGetObjectField(TEXT("props"), Props))
				{
					NWItemProps::Read(Item, *Props);
				}
				Out.Add(Item);
			}
		}
		return Out;
	}
}

void UNWBackend::IssueSerial(const FString& CharacterId, TFunction<void(bool, const FString&)> Done)
{
	Request(TEXT("POST"), FString::Printf(TEXT("/internal/characters/%s/serials"), *CharacterId), FString(), true,
		[Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
		{
			FString Serial;
			const bool bOk = Status == 201 && Json && Json->TryGetStringField(TEXT("serial"), Serial);
			Done(bOk, Serial);
		});
}

void UNWBackend::GetStorage(const FString& CharacterId, const FString& Location,
	TFunction<void(bool, int64, const TArray<FNWItem>&, const TArray<FString>&)> Done)
{
	Request(TEXT("GET"), FString::Printf(TEXT("/internal/characters/%s/storage/%s"), *CharacterId, *Location), FString(), true,
		[Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
		{
			const bool bOk = Status == 200 && Json;
			TArray<FString> Names;
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (bOk && Json->TryGetArrayField(TEXT("tab_names"), Arr))
			{
				for (const TSharedPtr<FJsonValue>& V : *Arr)
				{
					Names.Add(V->AsString());
				}
			}
			Done(bOk, bOk ? int64(Json->GetNumberField(TEXT("version"))) : 0, StorageItemsFrom(Json), Names);
		});
}

void UNWBackend::PutStorage(const FNWCharacterRecord& Record, const FString& Location, int64 Version, const TArray<FNWItem>& Items,
	const TArray<FString>& TabNames, float Capacity, const FString& Reason, TFunction<void(int32, int64)> Done)
{
	TSharedPtr<FJsonObject> Body = ParseJson(Record.StateJson());
	if (!Body)
	{
		Done(0, 0);
		return;
	}
	Body->SetNumberField(TEXT("version"), double(Version));
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FNWItem& Item : Items)
	{
		TSharedRef<FJsonObject> I = MakeShared<FJsonObject>();
		I->SetStringField(TEXT("item_id"), Item.ItemId.ToString());
		I->SetNumberField(TEXT("quantity"), Item.Quantity);
		I->SetNumberField(TEXT("slot"), Item.Slot);
		I->SetNumberField(TEXT("gear_score"), Item.GearScore);
		I->SetNumberField(TEXT("rarity"), Item.Rarity);
		I->SetObjectField(TEXT("props"), NWItemProps::Write(Item));
		I->SetNumberField(TEXT("tab"), Item.Tab);
		const FNWItemDef* D = FNWItemCatalog::Get().Find(Item.ItemId);
		I->SetNumberField(TEXT("weight"), D ? D->Weight : 0.0);   // peso unitário: o nw-core soma para o limite do mercado
		Arr.Add(MakeShared<FJsonValueObject>(I));
	}
	Body->SetArrayField(TEXT("items"), Arr);
	TArray<TSharedPtr<FJsonValue>> Names;
	for (const FString& N : TabNames)
	{
		Names.Add(MakeShared<FJsonValueString>(N));
	}
	Body->SetArrayField(TEXT("tab_names"), Names);
	Body->SetNumberField(TEXT("capacity"), Capacity);
	Write(Record.Id, TEXT("PUT"), FString::Printf(TEXT("/internal/characters/%s/storage/%s"), *Record.Id, *Location), Body.ToSharedRef(),
		Reason, [Weak = TWeakObjectPtr<UNWBackend>(this), Record, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
		{
			if ((Status == 200 || Status == 208) && Weak.IsValid())
			{
				Weak->RememberBag(Record);
			}
			Done(Status, Status == 200 && Json ? int64(Json->GetNumberField(TEXT("version"))) : 0);
		});
}

namespace
{
	/** Item do armazém e do chão no formato do nw-core (com o peso unitário do catálogo). */
	TSharedRef<FJsonObject> PlacedItemJson(const FNWItem& Item)
	{
		TSharedRef<FJsonObject> I = MakeShared<FJsonObject>();
		I->SetStringField(TEXT("item_id"), Item.ItemId.ToString());
		I->SetNumberField(TEXT("quantity"), Item.Quantity);
		I->SetNumberField(TEXT("slot"), Item.Slot);
		I->SetNumberField(TEXT("gear_score"), Item.GearScore);
		I->SetNumberField(TEXT("rarity"), Item.Rarity);
		I->SetObjectField(TEXT("props"), NWItemProps::Write(Item));
		I->SetNumberField(TEXT("tab"), Item.Tab);
		const FNWItemDef* D = FNWItemCatalog::Get().Find(Item.ItemId);
		I->SetNumberField(TEXT("weight"), D ? D->Weight : 0.0);
		return I;
	}

	TArray<TSharedPtr<FJsonValue>> PlacedItemsJson(const TArray<FNWItem>& Items)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FNWItem& Item : Items)
		{
			Arr.Add(MakeShared<FJsonValueObject>(PlacedItemJson(Item)));
		}
		return Arr;
	}

	UNWBackend::FGroundBag GroundBagFrom(const TSharedPtr<FJsonObject>& J)
	{
		UNWBackend::FGroundBag Bag;
		if (!J)
		{
			return Bag;
		}
		J->TryGetStringField(TEXT("id"), Bag.Id);
		J->TryGetStringField(TEXT("map"), Bag.Map);
		const TSharedPtr<FJsonObject>* P = nullptr;
		if (J->TryGetObjectField(TEXT("position"), P))
		{
			Bag.Position = FVector((*P)->GetNumberField(TEXT("x")), (*P)->GetNumberField(TEXT("y")), (*P)->GetNumberField(TEXT("z")));
		}
		FString Expires;
		FDateTime When;
		if (J->TryGetStringField(TEXT("expires_at"), Expires) && FDateTime::ParseIso8601(*Expires, When))
		{
			Bag.ExpiresIn = (When - FDateTime::UtcNow()).GetTotalSeconds();
		}
		Bag.Items = StorageItemsFrom(J);
		return Bag;
	}
}

void UNWBackend::DropGround(const FNWCharacterRecord& Record, const FString& Map, const FVector& Position, int32 TtlSeconds,
	const TArray<FNWItem>& Items, const FString& Reason, TFunction<void(int32, const FGroundBag&)> Done)
{
	TSharedPtr<FJsonObject> Body = ParseJson(Record.StateJson());
	if (!Body)
	{
		Done(0, FGroundBag());
		return;
	}
	Body->SetStringField(TEXT("map"), Map);
	TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetNumberField(TEXT("x"), Position.X);
	P->SetNumberField(TEXT("y"), Position.Y);
	P->SetNumberField(TEXT("z"), Position.Z);
	Body->SetObjectField(TEXT("position"), P);
	Body->SetNumberField(TEXT("ttl_seconds"), TtlSeconds);
	Body->SetArrayField(TEXT("items"), PlacedItemsJson(Items));
	Write(Record.Id, TEXT("POST"), FString::Printf(TEXT("/internal/characters/%s/ground"), *Record.Id), Body.ToSharedRef(), Reason,
		[Weak = TWeakObjectPtr<UNWBackend>(this), Record, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
		{
			if ((Status == 201 || Status == 208) && Weak.IsValid())
			{
				Weak->RememberBag(Record);
			}
			Done(Status, Status == 201 ? GroundBagFrom(Json) : FGroundBag());
		});
}

void UNWBackend::TakeGround(const FNWCharacterRecord& Record, const FString& BagId, const TArray<FNWItem>& Remaining, const FString& Reason,
	TFunction<void(int32)> Done)
{
	TSharedPtr<FJsonObject> Body = ParseJson(Record.StateJson());
	if (!Body)
	{
		Done(0);
		return;
	}
	Body->SetStringField(TEXT("character_id"), Record.Id);
	Body->SetArrayField(TEXT("items"), PlacedItemsJson(Remaining));
	Write(Record.Id, TEXT("POST"), FString::Printf(TEXT("/internal/ground/%s/take"), *BagId), Body.ToSharedRef(), Reason,
		[Weak = TWeakObjectPtr<UNWBackend>(this), Record, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject>)
		{
			if ((Status == 200 || Status == 208) && Weak.IsValid())
			{
				Weak->RememberBag(Record);
			}
			Done(Status);
		});
}

void UNWBackend::ListGround(const FString& CharacterId, TFunction<void(bool, const TArray<FGroundBag>&)> Done)
{
	Request(TEXT("GET"), FString::Printf(TEXT("/internal/characters/%s/ground"), *CharacterId), FString(), true,
		[Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
		{
			TArray<FGroundBag> Bags;
			// A rota devolve uma lista: vem em "__array" (ParseJson).
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (Status == 200 && Json && Json->TryGetArrayField(TEXT("__array"), Arr))
			{
				for (const TSharedPtr<FJsonValue>& V : *Arr)
				{
					Bags.Add(GroundBagFrom(V->AsObject()));
				}
			}
			Done(Status == 200, Bags);
		});
}

namespace
{
	FString ErrorCode(const TSharedPtr<FJsonObject>& Json)
	{
		FString Code;
		if (Json)
		{
			Json->TryGetStringField(TEXT("error"), Code);
		}
		return Code;
	}

	UNWBackend::FMarketOrder MarketOrderFrom(const TSharedPtr<FJsonObject>& J)
	{
		UNWBackend::FMarketOrder O;
		J->TryGetStringField(TEXT("id"), O.Id);
		O.bSell = J->GetStringField(TEXT("kind")) == TEXT("sell");
		J->TryGetStringField(TEXT("character_id"), O.CharacterId);
		O.Item.ItemId = FName(*J->GetStringField(TEXT("item_id")));
		O.Item.Quantity = int32(J->GetNumberField(TEXT("quantity")));
		O.Item.GearScore = int32(J->GetNumberField(TEXT("gear_score")));
		O.Item.Rarity = uint8(J->GetNumberField(TEXT("rarity")));
		const TSharedPtr<FJsonObject>* Props = nullptr;
		if (J->TryGetObjectField(TEXT("props"), Props))
		{
			NWItemProps::Read(O.Item, *Props);
		}
		O.UnitPrice = int64(J->GetNumberField(TEXT("unit_price")));
		J->TryGetStringField(TEXT("seller"), O.Seller);
		FString Expires;
		FDateTime When;
		if (J->TryGetStringField(TEXT("expires_at"), Expires) && FDateTime::ParseIso8601(*Expires, When))
		{
			O.ExpiresIn = (When - FDateTime::UtcNow()).GetTotalSeconds();
		}
		return O;
	}
}

void UNWBackend::ListOrders(TFunction<void(bool, const TArray<FMarketOrder>&)> Done)
{
	Request(TEXT("GET"), TEXT("/internal/market/global/orders"), FString(), true, [Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
	{
		TArray<FMarketOrder> Orders;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Status == 200 && Json && Json->TryGetArrayField(TEXT("__array"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				Orders.Add(MarketOrderFrom(V->AsObject()));
			}
		}
		Done(Status == 200, Orders);
	});
}

void UNWBackend::CreateOrder(const FNWCharacterRecord& Record, bool bSell, const FNWItem& Item, int64 UnitPrice, int32 DurationHours,
	const FString& Reason, TFunction<void(int32, const FString&)> Done)
{
	TSharedPtr<FJsonObject> Body = ParseJson(Record.StateJson());
	if (!Body)
	{
		Done(0, FString());
		return;
	}
	Body->SetStringField(TEXT("character_id"), Record.Id);
	Body->SetStringField(TEXT("kind"), bSell ? TEXT("sell") : TEXT("buy"));
	Body->SetStringField(TEXT("item_id"), Item.ItemId.ToString());
	Body->SetNumberField(TEXT("quantity"), Item.Quantity);
	Body->SetNumberField(TEXT("gear_score"), Item.GearScore);
	Body->SetNumberField(TEXT("rarity"), Item.Rarity);
	Body->SetObjectField(TEXT("props"), NWItemProps::Write(Item));
	Body->SetNumberField(TEXT("unit_price"), double(UnitPrice));
	Body->SetNumberField(TEXT("fee"), 0);   // sem taxas (dono, 2026-10-14)
	Body->SetNumberField(TEXT("duration_hours"), DurationHours);
	const FNWItemDef* D = FNWItemCatalog::Get().Find(Item.ItemId);
	Body->SetNumberField(TEXT("weight"), D ? D->Weight : 0.0);
	Write(Record.Id, TEXT("POST"), TEXT("/internal/market/global/orders"), Body.ToSharedRef(), Reason,
		[Weak = TWeakObjectPtr<UNWBackend>(this), Record, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
		{
			if ((Status == 201 || Status == 208) && Weak.IsValid())
			{
				Weak->RememberBag(Record);
			}
			Done(Status, ErrorCode(Json));
		});
}

void UNWBackend::FillOrder(const FNWCharacterRecord& Record, const FString& OrderId, int32 Quantity, const FNWItem* Delivered, const FString& Reason,
	TFunction<void(int32, const FString&)> Done)
{
	TSharedPtr<FJsonObject> Body = ParseJson(Record.StateJson());
	if (!Body)
	{
		Done(0, FString());
		return;
	}
	Body->SetStringField(TEXT("character_id"), Record.Id);
	Body->SetNumberField(TEXT("quantity"), Quantity);
	if (Delivered)
	{
		Body->SetNumberField(TEXT("gear_score"), Delivered->GearScore);
		Body->SetNumberField(TEXT("rarity"), Delivered->Rarity);
		Body->SetObjectField(TEXT("props"), NWItemProps::Write(*Delivered));
	}
	Write(Record.Id, TEXT("POST"), FString::Printf(TEXT("/internal/market/orders/%s/fill"), *OrderId), Body.ToSharedRef(), Reason,
		[Weak = TWeakObjectPtr<UNWBackend>(this), Record, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
		{
			if ((Status == 200 || Status == 208) && Weak.IsValid())
			{
				Weak->RememberBag(Record);
			}
			Done(Status, ErrorCode(Json));
		});
}

void UNWBackend::CancelOrder(const FString& CharacterId, const FString& OrderId, TFunction<void(int32)> Done)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("character_id"), CharacterId);
	Request(TEXT("POST"), FString::Printf(TEXT("/internal/market/orders/%s/cancel"), *OrderId), WriteJson(Body), true,
		[Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject>) { Done(Status); });
}

void UNWBackend::GetCity(const FString& Region, const FString& CharacterId, FCityDone Done)
{
	const FString Query = CharacterId.IsEmpty() ? FString() : TEXT("?character_id=") + CharacterId;
	Request(TEXT("GET"), FString::Printf(TEXT("/internal/regions/%s/city%s"), *Region, *Query), FString(), true,
		[Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json) { Done(Status, ErrorCode(Json), Json); });
}

void UNWBackend::CityTrade(const FNWCharacterRecord& Record, const FString& Region, bool bSellRaw, FName Item, int32 Quantity, int64 UnitPrice,
	const FString& Reason, FCityDone Done)
{
	TSharedPtr<FJsonObject> Body = ParseJson(Record.StateJson());
	if (!Body)
	{
		Done(0, FString(), nullptr);
		return;
	}
	Body->SetStringField(TEXT("character_id"), Record.Id);
	Body->SetStringField(TEXT("item_id"), Item.ToString());
	Body->SetNumberField(TEXT("quantity"), Quantity);
	Body->SetNumberField(TEXT("unit_price"), double(UnitPrice));
	Write(Record.Id, TEXT("POST"), FString::Printf(TEXT("/internal/regions/%s/city/%s"), *Region, bSellRaw ? TEXT("sell-raw") : TEXT("buy-item")),
		Body.ToSharedRef(), Reason,
		[Weak = TWeakObjectPtr<UNWBackend>(this), Record, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
		{
			if ((Status == 200 || Status == 208) && Weak.IsValid())
			{
				Weak->RememberBag(Record);
			}
			Done(Status, ErrorCode(Json), Json);
		});
}

void UNWBackend::CityBuy(const FNWCharacterRecord& Record, const FString& Region, int64 Price, const FString& Reason, FCityDone Done)
{
	TSharedPtr<FJsonObject> Body = ParseJson(Record.StateJson());
	if (!Body)
	{
		Done(0, FString(), nullptr);
		return;
	}
	Body->SetStringField(TEXT("character_id"), Record.Id);
	Body->SetNumberField(TEXT("price"), double(Price));
	Write(Record.Id, TEXT("POST"), FString::Printf(TEXT("/internal/regions/%s/city/buy"), *Region), Body.ToSharedRef(), Reason,
		[Weak = TWeakObjectPtr<UNWBackend>(this), Record, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
		{
			if ((Status == 200 || Status == 208) && Weak.IsValid())
			{
				Weak->RememberBag(Record);
			}
			Done(Status, ErrorCode(Json), Json);
		});
}

void UNWBackend::CityBid(const FNWCharacterRecord& Record, const FString& Region, int64 Amount, const FString& Reason, FCityDone Done)
{
	TSharedPtr<FJsonObject> Body = ParseJson(Record.StateJson());
	if (!Body)
	{
		Done(0, FString(), nullptr);
		return;
	}
	Body->SetStringField(TEXT("character_id"), Record.Id);
	Body->SetNumberField(TEXT("amount"), double(Amount));
	Write(Record.Id, TEXT("POST"), FString::Printf(TEXT("/internal/regions/%s/city/bid"), *Region), Body.ToSharedRef(), Reason,
		[Weak = TWeakObjectPtr<UNWBackend>(this), Record, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
		{
			if ((Status == 200 || Status == 208) && Weak.IsValid())
			{
				Weak->RememberBag(Record);
			}
			Done(Status, ErrorCode(Json), Json);
		});
}

void UNWBackend::CityAction(const FString& Verb, const FString& Region, const FString& Sub, TSharedRef<FJsonObject> Body, FCityDone Done)
{
	const FString Path = FString::Printf(TEXT("/internal/regions/%s/city"), *Region) + (Sub.IsEmpty() ? FString() : TEXT("/") + Sub);
	Request(Verb, Path, WriteJson(Body), true,
		[Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json) { Done(Status, ErrorCode(Json), Json); });
}

void UNWBackend::PendingCredits(const FString& CharacterId, TFunction<void(bool, int64, const FString&)> Done)
{
	Request(TEXT("GET"), FString::Printf(TEXT("/internal/characters/%s/credits"), *CharacterId), FString(), true,
		[Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
		{
			FString Until;
			if (Json)
			{
				Json->TryGetStringField(TEXT("until"), Until);
			}
			Done(Status == 200 && Json, Status == 200 && Json ? int64(Json->GetNumberField(TEXT("amount"))) : 0, Until);
		});
}

void UNWBackend::ClaimCredits(const FNWCharacterRecord& Record, const FString& Until, TFunction<void(int32, int64)> Done)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	if (!Until.IsEmpty())
	{
		Body->SetStringField(TEXT("until"), Until);
	}
	Write(Record.Id, TEXT("POST"), FString::Printf(TEXT("/internal/characters/%s/credits/claim"), *Record.Id), Body, TEXT("market_credit"),
		[Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json) { Done(Status, Status == 200 && Json ? int64(Json->GetNumberField(TEXT("amount"))) : 0); });
}

// ---------------------------------------------------------------- cliente

void UNWBackend::OpenSession(const TSharedPtr<FJsonObject>& Json)
{
	SessionToken = Json->GetStringField(TEXT("token"));
	const TSharedPtr<FJsonObject>* Account = nullptr;
	if (Json->TryGetObjectField(TEXT("account"), Account))
	{
		AccountName = (*Account)->GetStringField(TEXT("name"));
	}
	SaveSession();
}

void UNWBackend::SaveSession() const
{
	if (bAutoConnect)
	{
		return;  // testes automáticos não mexem na sessão lembrada
	}
	GConfig->SetString(SessionSection, TEXT("Account"), *AccountName, GGameUserSettingsIni);
	GConfig->SetString(SessionSection, TEXT("Token"), *SessionToken, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UNWBackend::Login(const FString& Name, const FString& Password, FDone Done)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("name"), Name.TrimStartAndEnd());
	Body->SetStringField(TEXT("password"), Password);
	++Busy;
	Request(TEXT("POST"), TEXT("/v1/auth/login"), WriteJson(Body), false, [this, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
	{
		--Busy;
		if (Status != 200 || !Json.IsValid())
		{
			Done(false, ErrorMessage(Status, Json));
			return;
		}
		OpenSession(Json);
		RefreshCharacters(Done);
	});
}

void UNWBackend::Register(const FString& Invite, const FString& Name, const FString& Password, FDone Done)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("invite"), Invite.TrimStartAndEnd());
	Body->SetStringField(TEXT("name"), Name.TrimStartAndEnd());
	Body->SetStringField(TEXT("password"), Password);
	++Busy;
	Request(TEXT("POST"), TEXT("/v1/auth/register"), WriteJson(Body), false, [this, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
	{
		--Busy;
		if (Status != 201 || !Json.IsValid())
		{
			Done(false, ErrorMessage(Status, Json));
			return;
		}
		OpenSession(Json);
		RefreshCharacters(Done);
	});
}

void UNWBackend::RefreshCharacters(FDone Done)
{
	++Busy;
	Request(TEXT("GET"), TEXT("/v1/me"), FString(), false, [this, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
	{
		--Busy;
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Status == 401)
		{
			Logout();
		}
		if (Status != 200 || !Json.IsValid() || !Json->TryGetArrayField(TEXT("characters"), List))
		{
			Done(false, Status == 401 ? FString() : ErrorMessage(Status, Json));
			return;
		}
		Characters.Reset();
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> C = V->AsObject();
			if (!C.IsValid())
			{
				continue;
			}
			FNWCharacterSummary& S = Characters.AddDefaulted_GetRef();
			S.Id = C->GetStringField(TEXT("id"));
			S.Name = C->GetStringField(TEXT("name"));
			S.ActiveWeapon = C->GetStringField(TEXT("active_weapon"));
			FString Seen;
			S.bHasLastSeen = C->TryGetStringField(TEXT("last_seen_at"), Seen) && FDateTime::ParseIso8601(*Seen, S.LastSeen);
		}
		Done(true, FString());
	});
}

void UNWBackend::CreateCharacter(const FString& Name, FDone Done)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("name"), Name.TrimStartAndEnd());
	++Busy;
	Request(TEXT("POST"), TEXT("/v1/characters"), WriteJson(Body), false, [this, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
	{
		--Busy;
		if (Status != 201)
		{
			Done(false, ErrorMessage(Status, Json));
			return;
		}
		RefreshCharacters(Done);
	});
}

void UNWBackend::DeleteCharacter(const FString& Id, FDone Done)
{
	++Busy;
	Request(TEXT("DELETE"), FString::Printf(TEXT("/v1/characters/%s"), *Id), FString(), false, [this, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
	{
		--Busy;
		if (Status != 204)
		{
			Done(false, ErrorMessage(Status, Json));
			return;
		}
		RefreshCharacters(Done);
	});
}

void UNWBackend::Play(const FString& CharacterId, FDone Done)
{
	if (ConnectAddress.IsEmpty())
	{
		Done(false, NWText::Get(TEXT("nw.err.no_address")));
		return;
	}
	++Busy;
	Request(TEXT("POST"), FString::Printf(TEXT("/v1/characters/%s/join"), *CharacterId), FString(), false, [this, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
	{
		--Busy;
		if (Status != 200 || !Json.IsValid())
		{
			Done(false, ErrorMessage(Status, Json));
			return;
		}
		const FString Url = FString::Printf(TEXT("%s?ticket=%s"), *ConnectAddress, *Json->GetStringField(TEXT("ticket")));
		UE_LOG(LogNWBackend, Display, TEXT("conectando em %s"), *ConnectAddress);
		UWorld* World = GetGameInstance()->GetWorld();
		if (!World)
		{
			Done(false, TEXT("Sem mundo para viajar."));
			return;
		}
		GEngine->SetClientTravel(World, *Url, TRAVEL_Absolute);
		Done(true, FString());
	});
}

void UNWBackend::Logout()
{
	if (!SessionToken.IsEmpty())
	{
		Request(TEXT("POST"), TEXT("/v1/auth/logout"), FString(), false, [](int32, TSharedPtr<FJsonObject>) {});
	}
	SessionToken.Reset();
	Characters.Reset();
	SaveSession();
}

// Testes sem tela: -nwlogin=<conta> -nwchar=<personagem>, senha em NW_PASSWORD.
void UNWBackend::ClientAutoConnect()
{
	FString Account, CharacterName;
	if (!FParse::Value(FCommandLine::Get(), TEXT("-nwlogin="), Account) || !FParse::Value(FCommandLine::Get(), TEXT("-nwchar="), CharacterName))
	{
		return;
	}
	bAutoConnect = true;
	UE_LOG(LogNWBackend, Display, TEXT("login de %s em %s"), *Account, *BaseUrl);
	Login(Account, FPlatformMisc::GetEnvironmentVariable(TEXT("NW_PASSWORD")), [this, CharacterName](bool bOk, const FString& Error)
	{
		if (!bOk)
		{
			UE_LOG(LogNWBackend, Error, TEXT("login falhou: %s"), *Error);
			return;
		}
		const FNWCharacterSummary* C = Characters.FindByPredicate([&](const FNWCharacterSummary& S) { return S.Name.Equals(CharacterName, ESearchCase::IgnoreCase); });
		if (!C)
		{
			UE_LOG(LogNWBackend, Error, TEXT("personagem %s não existe nessa conta"), *CharacterName);
			return;
		}
		Play(C->Id, [](bool bOk, const FString& Error)
		{
			if (!bOk)
			{
				UE_LOG(LogNWBackend, Error, TEXT("entrada falhou: %s"), *Error);
			}
		});
	});
}

// ---------------------------------------------------------------- Play do editor com backend

bool UNWBackend::EnablePie()
{
#if WITH_EDITOR
	// backend/dev.env (valores de teste, no repositório): KEY=valor por linha, aspas opcionais.
	FString Raw;
	const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../../backend/dev.env"));
	if (!FFileHelper::LoadFileToString(Raw, *File))
	{
		return false;
	}
	TMap<FString, FString> Env;
	TArray<FString> Lines;
	Raw.ParseIntoArrayLines(Lines);
	for (const FString& L : Lines)
	{
		FString K, V;
		if (!L.StartsWith(TEXT("#")) && L.Split(TEXT("="), &K, &V))
		{
			Env.Add(K.TrimStartAndEnd(), V.TrimStartAndEnd().TrimQuotes());
		}
	}
	const FString* Account = Env.Find(TEXT("NW_PIE_ACCOUNT"));
	if (!Account || Account->IsEmpty() || !Env.Contains(TEXT("NW_BACKEND")) || !Env.Contains(TEXT("NW_SERVER_SECRET")))
	{
		return false;
	}
	BaseUrl = Env[TEXT("NW_BACKEND")];
	BaseUrl.RemoveFromEnd(TEXT("/"));
	ServerSecret = Env[TEXT("NW_SERVER_SECRET")];
	PieAccount = *Account;
	PiePassword = Env.FindRef(TEXT("NW_TEST_PASSWORD"));
	PieCharacter = Env.FindRef(TEXT("NW_PIE_CHARACTER"));
	bPie = true;
	UE_LOG(LogNWBackend, Display, TEXT("Play com backend: %s, conta %s, personagem %s"), *BaseUrl, *PieAccount, *PieCharacter);
	return true;
#else
	return false;
#endif
}

void UNWBackend::DisablePie()
{
	bPie = false;
	BaseUrl.Reset();
	ServerSecret.Reset();
	SessionToken.Reset();
}

void UNWBackend::PieEnter(FRedeemDone Done)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("name"), PieAccount);
	Body->SetStringField(TEXT("password"), PiePassword);
	// A sessão fica só na memória do Play (não grava [NW.Session] no GameUserSettings).
	Request(TEXT("POST"), TEXT("/v1/auth/login"), WriteJson(Body), false, [this, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json) mutable
	{
		if (Status != 200 || !Json.IsValid())
		{
			Done({}, ErrorMessage(Status, Json));
			return;
		}
		SessionToken = Json->GetStringField(TEXT("token"));
		Request(TEXT("GET"), TEXT("/v1/me"), FString(), false, [this, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Me) mutable
		{
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			FString Id;
			if (Status == 200 && Me.IsValid() && Me->TryGetArrayField(TEXT("characters"), List))
			{
				for (const TSharedPtr<FJsonValue>& V : *List)
				{
					if (V->AsObject()->GetStringField(TEXT("name")).Equals(PieCharacter, ESearchCase::IgnoreCase))
					{
						Id = V->AsObject()->GetStringField(TEXT("id"));
					}
				}
			}
			if (Id.IsEmpty())
			{
				Done({}, FString::Printf(TEXT("personagem %s não existe (rode tools/backend_dev.sh)"), *PieCharacter));
				return;
			}
			Request(TEXT("POST"), FString::Printf(TEXT("/v1/characters/%s/join"), *Id), FString(), false,
				[this, Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Join) mutable
				{
					if (Status != 200 || !Join.IsValid())
					{
						Done({}, ErrorMessage(Status, Join));
						return;
					}
					RedeemTicket(Join->GetStringField(TEXT("ticket")), MoveTemp(Done));
				});
		});
	});
}

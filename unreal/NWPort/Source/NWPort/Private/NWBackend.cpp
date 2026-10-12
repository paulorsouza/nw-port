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
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogNWBackend, Log, All);

static TSharedPtr<FJsonObject> ParseJson(const FString& Text)
{
	TSharedPtr<FJsonObject> Obj;
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
		Inv.Add(MakeShared<FJsonValueObject>(I));
	}
	Obj->SetArrayField(TEXT("inventory"), Inv);
	if (Progression.IsValid())
	{
		Obj->SetObjectField(TEXT("progression"), Progression);
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
	return !BaseUrl.IsEmpty() && !bAutoConnect && World && World->GetNetMode() == NM_Standalone;
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
	Request(TEXT("POST"), TEXT("/internal/tickets/redeem"), WriteJson(Body), true, [Done = MoveTemp(Done)](int32 Status, TSharedPtr<FJsonObject> Json)
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
		Done(Record, FString());
	});
}

void UNWBackend::SaveState(const FNWCharacterRecord& Record)
{
	const FString Name = Record.Name;
	Request(TEXT("PUT"), FString::Printf(TEXT("/internal/characters/%s/state"), *Record.Id), Record.StateJson(), true,
		[Name](int32 Status, TSharedPtr<FJsonObject>)
		{
			if (Status == 204)
			{
				UE_LOG(LogNWBackend, Verbose, TEXT("salvo: %s"), *Name);
			}
		});
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

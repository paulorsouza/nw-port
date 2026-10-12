#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Net/Core/Connection/NetEnums.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NWItems.h"
#include "NWBackend.generated.h"

class FJsonObject;

/** Seções do GameUserSettings.ini com as escolhas do jogador (cache local do que o servidor guarda). */
namespace NWIni
{
	inline const TCHAR* Loadout = TEXT("NW.Loadout");
	inline const TCHAR* Mastery = TEXT("NW.Mastery");
}

/** Personagem como o nw-core devolve e recebe (posição em cm do Unreal). */
struct FNWCharacterRecord
{
	FString Id;
	FString Name;
	FString AccountName;
	FString Map;
	bool bHasPosition = false;
	FVector Position = FVector::ZeroVector;
	float Yaw = 0.f;
	float Health = -1.f;          // < 0 = sem valor (nasce com vida cheia)
	FString ActiveWeapon;
	TSharedPtr<FJsonObject> Loadout;  // { "Sword": ["ShieldBash", ...], ... }
	TSharedPtr<FJsonObject> Mastery;  // { "sword": ["id", ...], ... }
	int64 Coins = 0;
	TArray<FNWItem> Items;
	bool bFirstJoin = false;          // primeira entrada: o servidor dá o kit inicial

	static TOptional<FNWCharacterRecord> FromJson(const TSharedPtr<FJsonObject>& Obj);
	FString StateJson() const;
};

/** Personagem na lista da tela de entrada. */
struct FNWCharacterSummary
{
	FString Id;
	FString Name;
	FString ActiveWeapon;
	FDateTime LastSeen;
	bool bHasLastSeen = false;
};

/**
 * Conversa com o nw-core (backend Rust).
 *
 * Servidor: `-nwbackend=<url>` e a variável NW_SERVER_SECRET ligam a exigência de passe na conexão; sem isso o
 * servidor aceita qualquer um e não salva nada (arena, testes locais).
 *
 * Cliente: backend e endereço do servidor vêm de [NW.Server] Backend/Address no DefaultGame.ini (ou
 * `-nwbackend=` / `-nwconnect=`). Com backend configurado, o jogo abre na tela de entrada (FNWLoginMenu).
 * Testes sem tela: `-nwlogin=<conta> -nwchar=<personagem>` com a senha em NW_PASSWORD entra direto.
 */
UCLASS()
class NWPORT_API UNWBackend : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	bool IsServerEnabled() const { return !BaseUrl.IsEmpty() && !ServerSecret.IsEmpty(); }

	// ---- cliente
	using FDone = TFunction<void(bool bOk, const FString& Error)>;
	/** Tela de entrada ativa: cliente com backend, fora de um servidor e sem entrada automática. */
	bool WantsFrontEnd(const UWorld* World) const;
	bool HasSession() const { return !SessionToken.IsEmpty(); }
	bool IsBusy() const { return Busy > 0; }
	const FString& GetAccountName() const { return AccountName; }
	const TArray<FNWCharacterSummary>& GetCharacters() const { return Characters; }
	/** Última falha de conexão com o servidor de jogo (mostrada e limpa pela tela de entrada). */
	FString TakeConnectionError() { FString E = MoveTemp(ConnectionError); ConnectionError.Reset(); return E; }

	void Login(const FString& Name, const FString& Password, FDone Done);
	void Register(const FString& Invite, const FString& Name, const FString& Password, FDone Done);
	void RefreshCharacters(FDone Done);
	void CreateCharacter(const FString& Name, FDone Done);
	void DeleteCharacter(const FString& Id, FDone Done);
	/** Pede o passe e conecta no servidor de jogo. */
	void Play(const FString& CharacterId, FDone Done);
	void Logout();

	using FRedeemDone = TFunction<void(TOptional<FNWCharacterRecord> Record, const FString& Error)>;
	void RedeemTicket(const FString& Ticket, FRedeemDone Done);
	void SaveState(const FNWCharacterRecord& Record);

private:
	using FJsonDone = TFunction<void(int32 Status, TSharedPtr<FJsonObject> Body)>;
	void Request(const FString& Verb, const FString& Path, const FString& Body, bool bServer, FJsonDone Done);

	void ClientAutoConnect();
	void OpenSession(const TSharedPtr<FJsonObject>& Json);
	void SaveSession() const;
	void HandleNetworkFailure(UWorld* World, class UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);
	static FString ErrorMessage(int32 Status, const TSharedPtr<FJsonObject>& Body);

	FString BaseUrl;
	FString ServerSecret;
	FString SessionToken;
	FString AccountName;
	FString ConnectAddress;
	FString ConnectionError;
	TArray<FNWCharacterSummary> Characters;
	int32 Busy = 0;
	bool bAutoConnect = false;
	FDelegateHandle NetworkFailureHandle, TravelFailureHandle;
};

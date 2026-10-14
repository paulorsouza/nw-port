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
	TSharedPtr<FJsonObject> Progression;  // { "level": 3, "xp": 1200, "spent": [5], "trade": [4] } (UNWProgressionComponent)
	float StorageCapacity = -1.f;         // armazém compartilhado (kg, soma das fontes); < 0 = não manda
	int64 Coins = 0;
	TArray<FNWItem> Items;
	bool bFirstJoin = false;          // primeira entrada: o servidor dá o kit inicial
	bool bAdmin = false;              // conta admin: comandos de teste (F10) no servidor

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
	/** Salva o estado; Reason vai para o log de itens do nw-core ("periodic", "logout", "craft:<receita>"...). Done recebe
	 *  o status (204 gravado, 208 já estava gravado). */
	void SaveState(const FNWCharacterRecord& Record, const FString& Reason = TEXT("save"), TFunction<void(int32 Status)> Done = nullptr);
	/** Saco no chão (etapa 2 das gravações): soltar grava a bolsa do Record e o saco numa transação; pegar grava a bolsa e
	 *  o que sobra no saco (vazio = some; 409 ground_gone = venceu ou já foi pego); a lista traz os sacos vivos. */
	struct FGroundBag { FString Id; FString Map; FVector Position = FVector::ZeroVector; double ExpiresIn = 0.0; TArray<FNWItem> Items; };
	void DropGround(const FNWCharacterRecord& Record, const FString& Map, const FVector& Position, int32 TtlSeconds, const TArray<FNWItem>& Items,
		const FString& Reason, TFunction<void(int32 Status, const FGroundBag& Bag)> Done);
	void TakeGround(const FNWCharacterRecord& Record, const FString& BagId, const TArray<FNWItem>& Remaining, const FString& Reason,
		TFunction<void(int32 Status)> Done);
	void ListGround(const FString& CharacterId, TFunction<void(bool bOk, const TArray<FGroundBag>& Bags)> Done);

	// Mercado (posto de trocas, docs/sistemas.md §5): um só para todos (território "global"). Erro do nw-core em Code
	// (order_closed, storage_full, too_many_orders...).
	struct FMarketOrder
	{
		FString Id;
		bool bSell = true;
		FString CharacterId;
		FString Seller;          // nome do personagem dono
		FNWItem Item;            // Quantity = o que falta negociar
		int64 UnitPrice = 0;
		double ExpiresIn = 0.0;
	};
	void ListOrders(TFunction<void(bool bOk, const TArray<FMarketOrder>& Orders)> Done);
	void CreateOrder(const FNWCharacterRecord& Record, bool bSell, const FNWItem& Item, int64 UnitPrice, int32 DurationHours, const FString& Reason,
		TFunction<void(int32 Status, const FString& Code)> Done);
	void FillOrder(const FNWCharacterRecord& Record, const FString& OrderId, int32 Quantity, const FNWItem* Delivered, const FString& Reason,
		TFunction<void(int32 Status, const FString& Code)> Done);
	void CancelOrder(const FString& CharacterId, const FString& OrderId, TFunction<void(int32 Status)> Done);
	/** Créditos de venda pendentes e o instante até onde valem (o resgate leva esse instante). */
	void PendingCredits(const FString& CharacterId, TFunction<void(bool bOk, int64 Amount, const FString& Until)> Done);
	void ClaimCredits(const FNWCharacterRecord& Record, const FString& Until, TFunction<void(int32 Status, int64 Amount)> Done);

	// ---- Play do editor com o nw-core local de teste (docs/teste-backend.md)
	/** Só no editor: lê backend/dev.env (NW_BACKEND, NW_SERVER_SECRET, NW_PIE_ACCOUNT, NW_PIE_CHARACTER, NW_TEST_PASSWORD)
	 *  e liga o backend no Play sozinho (standalone). Sem NW_PIE_ACCOUNT, devolve false e o Play segue sem backend. */
	bool EnablePie();
	/** Volta a jogar sem backend (nw-core fora do ar). */
	void DisablePie();
	bool IsPie() const { return bPie; }
	/** Entra com a conta e o personagem do editor: login, passe e resgate, como um cliente no servidor dedicado. */
	void PieEnter(FRedeemDone Done);
	// Brindes dos NPCs (esteira 4, NWNpcRewards.h): o que já foi pego ("<kind>:<npc>:<period>") e a coleta, gravada
	// junto com a bolsa e as moedas do Record (Status 201 ok, 409 já pego).
	void ListNpcRewards(const FString& CharacterId, const FString& Period, TFunction<void(bool bOk, const TArray<FString>& Keys)> Done);
	void ClaimNpcReward(const FNWCharacterRecord& Record, const FString& Npc, const FString& Kind, const FString& Period,
		TFunction<void(bool bOk, int32 Status)> Done);
	// Armazém compartilhado (esteira 4, local "shared"): ler e gravar com a versão lida; a gravação leva junto a bolsa e
	// as moedas do Record, o peso unitário de cada item (catálogo), os nomes das abas e a capacidade somada pelo
	// servidor. Status 200 ok, 409 o armazém mudou (ler de novo).
	/** Número de série de um item assinado; o personagem é o artesão (docs/itens.md, Assinatura). */
	void IssueSerial(const FString& CharacterId, TFunction<void(bool bOk, const FString& Serial)> Done);
	void GetStorage(const FString& CharacterId, const FString& Location,
		TFunction<void(bool bOk, int64 Version, const TArray<FNWItem>& Items, const TArray<FString>& TabNames)> Done);
	/** Status 208 = o nw-core já tinha aplicado esta gravação (reenvio depois de perder a resposta): ler de novo, sem
	 *  refazer a mudança. */
	void PutStorage(const FNWCharacterRecord& Record, const FString& Location, int64 Version, const TArray<FNWItem>& Items,
		const TArray<FString>& TabNames, float Capacity, const FString& Reason, TFunction<void(int32 Status, int64 NewVersion)> Done);

private:
	/**
	 * Gravações de bolsa, moedas e armazém (docs/plano-servidor.md "Gravações"): uma fila por personagem, uma de cada
	 * vez, na ordem. Cada uma leva a sessão aberta no resgate do passe e um número crescente; o nw-core recusa gravação
	 * velha. Sem resposta (rede, nw-core reiniciando): tenta de novo com o mesmo número (até MaxWriteTries); o nw-core
	 * responde 409 duplicate_write se já tinha aplicado, que vira 208 para quem chamou.
	 */
	void Write(const FString& CharacterId, const FString& Verb, const FString& Path, TSharedRef<FJsonObject> Body, const FString& Reason,
		TFunction<void(int32 Status, TSharedPtr<FJsonObject> Body)> Done);
	void PumpWrites(const FString& CharacterId);
	struct FPendingWrite
	{
		FString Verb, Path;
		TSharedPtr<FJsonObject> Body;
		TFunction<void(int32, TSharedPtr<FJsonObject>)> Done;
		int32 Tries = 0;
	};
	struct FWriteQueue
	{
		int64 Session = -1;
		int64 NextSeq = 0;
		bool bBusy = false;
		TArray<FPendingWrite> Pending;
	};
	TMap<FString, FWriteQueue> Writes;
	/** Última bolsa e equipado gravados de cada personagem: o salvamento manda só os espaços que mudaram
	 *  (inventory_patch). Atualizada a cada gravação que leva a bolsa. */
	TMap<FString, TArray<FNWItem>> PersistedBag;
	void RememberBag(const FNWCharacterRecord& Record) { PersistedBag.Add(Record.Id, Record.Items); }
	static constexpr int32 MaxWriteTries = 6;
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
	bool bPie = false;
	FString PieAccount, PiePassword, PieCharacter;
	FDelegateHandle NetworkFailureHandle, TravelFailureHandle;
};

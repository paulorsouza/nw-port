#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWItems.h"
#include "NWOps.h"
#include "NWCity.h"
#include "NWMarket.h"
#include "NWInteractable.generated.h"

class ANWCharacter;

/**
 * Algo que o jogador usa com a tecla de interagir (esteira 4: NPC, estação, armazém, posto de trocas). O
 * UNWInteractorComponent do jogador escolhe o foco e o NWHUD desenha o cartão do jogo (nome + [E] ação).
 * OnInteract roda no cliente dono; o sistema chama o servidor pelo próprio componente quando precisar.
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWInteractableComponent();
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	static const TArray<TWeakObjectPtr<UNWInteractableComponent>>& All();

	/** Nome na faixa do cartão. */
	FText DisplayName;
	/** Ação na linha da tecla (chave de texto: do jogo, ex. ui_talk, ou tools/strings). */
	FString ActionKey = TEXT("ui_talk");
	/** Distância máxima (cm) do jogador ao ponto de foco. */
	float Radius = 250.f;
	/** Altura do ponto de foco (anel do cartão) acima da origem do ator. */
	float FocusHeight = 0.f;
	TFunction<void(ANWCharacter*)> OnInteract;

	FVector FocusLocation() const;
};

/**
 * Do jogador: escolhe o interagível em foco (o mais perto à frente) e trata a tecla (cliente dono). No servidor,
 * entrega os brindes dos NPCs (FNWNpcRewards) e guarda o que já foi pego, replicado ao dono para o ícone do NPC.
 */
UCLASS(ClassGroup = (NW))
class NWPORT_API UNWInteractorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWInteractorComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** true se havia foco e a ação foi feita (a coleta não recebe a tecla). */
	bool InteractPressed();

	TWeakObjectPtr<UNWInteractableComponent> Focus;

	/** Brinde do NPC (variante): o servidor confere distância, sorteio da semana e o que já foi pego. */
	UFUNCTION(Server, Reliable) void ServerNpcInteract(const FString& Variant);
	/** Chaves "<kind>:<npc>:<period>" do que o personagem já pegou (nw-core; só o dono recebe). */
	UPROPERTY(Replicated) TArray<FString> RewardClaims;
	/** Bolsa no chão (NWLootBag.h): o servidor confere dono e distância e passa o que couber para a bolsa. */
	UFUNCTION(Server, Reliable) void ServerLootTake(class ANWLootBag* Bag);
	/** Pega Quantity do item do espaço Slot da bolsa no chão (tela da bolsa: arrastar, clique direito, dividir). */
	UFUNCTION(Server, Reliable) void ServerLootTakeItem(class ANWLootBag* Bag, int32 Slot, int32 Quantity);
	/** Refino (NWCrafting.h): o servidor confere estação perto, nível, materiais e taxa; erro vira aviso ao dono. */
	UFUNCTION(Server, Reliable) void ServerRefine(const FString& StationType, FName Recipe, int32 Count);
	/** Loja (NWShop.h): o servidor confere NPC perto, preço, limite semanal e peso; grava na hora (SavePlayer). */
	UFUNCTION(Server, Reliable) void ServerShopBuy(const FString& ShopId, FName Item, int32 Quantity);
	/** Fabricar arma, escudo ou ferramenta (docs/game/crafting.md; FNWCrafting::Craft): na hora, item na bolsa; treino abaixo do
	 *  nível de fabricar. */
	UFUNCTION(Server, Reliable) void ServerCraft(const FString& StationType, FName Recipe);
	/** Melhoria do item da bolsa na mesa do ofício dele, pagando materiais (docs/game/items.md): Kind = ENWImprove, Mat =
	 *  material escolhido (FNWCrafting::ImproveMaterials). */
	UFUNCTION(Server, Reliable) void ServerImprove(const FString& StationType, int32 BagSlot, uint8 Kind, uint8 Mat);
	/** Resultado da última fabricação (Kind 255) ou melhoria (ENWImprove) para a tela da mesa: o item antes e depois. */
	UFUNCTION(Client, Reliable) void ClientCraftResult(const FNWItem& Before, const FNWItem& After, uint8 Kind);
	struct FCraftResult { FNWItem Before, After; uint8 Kind = 255; int32 Seq = 0; };
	/** Só no dono: Seq sobe a cada resultado (a tela espera um Seq maior que o do clique). */
	FCraftResult CraftResult;

	// ---- armazém compartilhado (esteira 4, docs/game/city.md "Armazém"): um por personagem, aberto em qualquer despensa,
	// 5 abas; o servidor guarda e grava no nw-core (bolsa + armazém numa transação).
	/** Aberto ("shared") ou fechado (vazio), os itens (Tab = aba), nomes das abas e capacidade; só o dono recebe. */
	UPROPERTY(Replicated) FString StorageLocation;
	UPROPERTY(Replicated) TArray<FNWItem> StorageItems;
	UPROPERTY(Replicated) TArray<FString> StorageTabNames;
	UPROPERTY(Replicated) float StorageCapacity = 0.f;
	float StorageWeight() const;
	/** Capacidade pelas fontes: armazem_por_cidade × cidades conhecidas (casas e nível da cidade quando existirem). */
	float StorageCapacityNow() const;
	/** Abre na despensa da cidade City (a cidade passa a ser conhecida). */
	UFUNCTION(Server, Reliable) void ServerStorageOpen(const FString& City);
	UFUNCTION(Server, Reliable) void ServerStorageClose();
	/** Depositar (Slot = espaço da bolsa, na aba Tab) ou retirar (Slot = espaço do armazém), Quantity da pilha. */
	UFUNCTION(Server, Reliable) void ServerStorageMove(bool bDeposit, int32 Slot, int32 Quantity, uint8 Tab);
	/** Várias pilhas inteiras de uma vez, gravadas numa só transação: "Pegar tudo" (catcontainer ContainerTakeAll) e
	 *  mover a seção inteira (catsection TransferAll). Slots da bolsa (guardar, na aba Tab) ou do armazém (retirar). */
	UFUNCTION(Server, Reliable) void ServerStorageMoveMany(bool bDeposit, const TArray<int32>& Slots, uint8 Tab);
	/** Passa a pilha do espaço Slot do armazém para a aba Tab (junta com a igual que já estiver lá). */
	UFUNCTION(Server, Reliable) void ServerStorageSetTab(int32 Slot, uint8 Tab);
	/** Nome da aba (vazio = padrão; até 24 letras). */
	UFUNCTION(Server, Reliable) void ServerStorageRenameTab(uint8 Tab, const FString& Name);

	// ---- mercado (NWMarket.h, NWMarket.cpp): só perto do posto de trocas; o dono recebe as linhas do livro.
	UFUNCTION(Server, Reliable) void ServerMarketOpen();
	UFUNCTION(Server, Reliable) void ServerMarketClose();
	UFUNCTION(Server, Reliable) void ServerMarketRefresh();
	/** Compra Quantity de uma ordem de venda (vai para o armazém). */
	UFUNCTION(Server, Reliable) void ServerMarketBuy(int32 Req, const FString& OrderId, int32 Quantity);
	/** Vende Quantity do item do espaço BagSlot para uma ordem de compra. */
	UFUNCTION(Server, Reliable) void ServerMarketSell(int32 Req, const FString& OrderId, int32 BagSlot, int32 Quantity);
	/** Cria ordem de venda com Quantity do item do espaço BagSlot. */
	UFUNCTION(Server, Reliable) void ServerMarketPostSell(int32 Req, int32 BagSlot, int32 Quantity, int64 UnitPrice, int32 Days);
	/** Cria ordem de compra (o preço × quantidade sai das moedas na hora). */
	UFUNCTION(Server, Reliable) void ServerMarketPostBuy(int32 Req, FName ItemId, int32 Quantity, int64 UnitPrice, int32 Days);
	UFUNCTION(Server, Reliable) void ServerMarketCancel(int32 Req, const FString& OrderId);
	UFUNCTION(Client, Reliable) void ClientMarketOrders(const TArray<FNWMarketRow>& Rows);
	/** Fim do pedido Req do cliente (Req vem da tela; o livro novo chega antes, na mesma ordem). bOk = gravado. */
	UFUNCTION(Client, Reliable) void ClientMarketDone(int32 Req, bool bOk);
	/** Cliente: livro de ordens recebido e o número da última atualização (a tela redesenha quando muda). */
	TArray<FNWMarketRow> MarketRows;
	int32 MarketSeq = 0;
	/** Cliente: pedidos terminados (Req, gravado) que a tela ainda não leu. */
	TArray<TPair<int32, bool>> MarketDone;

	// ---- cidade (governança; NWCity.h, NWCity.cpp): perto do Quadro do Projeto de Vila (jogadores) ou do Quadro de
	// Planejamento do Território (governador). Mesmo esquema do mercado: Req da tela, estado e fim do pedido.
	UFUNCTION(Server, Reliable) void ServerCityOpen();
	UFUNCTION(Server, Reliable) void ServerCityClose();
	UFUNCTION(Server, Reliable) void ServerCityRefresh();
	/** Vende Quantity de matéria-prima da bolsa à cidade pelo preço visto (UnitPrice). */
	UFUNCTION(Server, Reliable) void ServerCitySellRaw(int32 Req, FName Item, int32 Quantity, int64 UnitPrice);
	/** Compra Quantity de um item da lista da cidade. */
	UFUNCTION(Server, Reliable) void ServerCityBuyItem(int32 Req, FName Item, int32 Quantity, int64 UnitPrice);
	/** Compra a cidade pelo preço visto (inicial ou o pedido pelo dono). */
	UFUNCTION(Server, Reliable) void ServerCityBuy(int32 Req, int64 Price);
	/** Governador: abre o leilão de 24 h com o lance inicial Price (> 0) ou cancela (0, só sem lance). */
	UFUNCTION(Server, Reliable) void ServerCitySale(int32 Req, int64 Price);
	/** Lance no leilão da cidade (sai das moedas; volta em crédito se alguém cobrir). */
	UFUNCTION(Server, Reliable) void ServerCityBid(int32 Req, int64 Amount);
	/** Governador: preços (só os itens da lista) e imposto (< 0 = mantém). */
	UFUNCTION(Server, Reliable) void ServerCityManage(int32 Req, const TArray<FNWCityPrice>& Buy, const TArray<FNWCityPrice>& Sell, float Tax);
	/** Governador: "station" (evoluir ou consertar), "buff" ou "weapon". */
	UFUNCTION(Server, Reliable) void ServerCityProject(int32 Req, const FString& Kind, const FString& Station, const FString& Buff);
	UFUNCTION(Client, Reliable) void ClientCityState(const FNWCityState& State);
	UFUNCTION(Client, Reliable) void ClientCityDone(int32 Req, bool bOk);
	/** Cliente: cidade recebida, número da última atualização e pedidos terminados. */
	FNWCityState CityState;
	int32 CitySeq = 0;
	TArray<TPair<int32, bool>> CityDone;

private:
	void LoadClaims();
	bool bClaimsLoaded = false;
	bool bClaimsLoading = false;
	bool bClaimBusy = false;
	FString ClaimsPeriod;
	// Armazém no servidor.
	int64 StorageVersion = 0;
	bool bStorageBusy = false;
	TArray<FNWItem> MemStorage;   // sem backend (PIE)
	TArray<FString> MemTabNames;

	/** Gravação de uma operação no armazém (NWOps): PUT storage com a versão; 409 relê e refaz, 208 só relê a versão. */
	NWOps::FCommit StorageCommit();
	/** Roda uma mudança do armazém como operação gravada (NWOps::Run). */
	void RunStorageOp(TFunction<bool(UNWInteractorComponent&)> Apply);
	// Cidade no servidor.
	bool bCityOpen = false;
	FString CityRegion;
	bool NearCityBoard() const;
	/** Lê a cidade (ou usa Json, a resposta de uma ação), guarda a cópia e manda ao dono; depois Then. */
	void CitySend(TSharedPtr<FJsonObject> Json = nullptr, TFunction<void()> Then = nullptr);
	void CityFinish(int32 Req, bool bOk, TSharedPtr<FJsonObject> Json);
	void CityGovernor(int32 Req, const FString& Verb, const FString& Sub, TSharedRef<FJsonObject> Body);
	// Mercado no servidor.
	bool bMarketOpen = false;
	bool NearTradingPost() const;
	void MarketSend(bool bForce);
	/** Relê o livro, manda as linhas e depois o fim do pedido Req. */
	void MarketFinish(int32 Req, bool bOk);
	void MarketClaimCredits();
	/** Gravação de uma operação do mercado: Call grava e devolve status e código do erro do nw-core. */
	NWOps::FCommit MarketCommit(TFunction<void(const FNWCharacterRecord&, TFunction<void(int32, const FString&)>)> Call);
	/** Move uma pilha (inteira ou Quantity) sem gravar; devolve o que mudou (Quantity 0 = nada; Tab = aba no armazém). */
	FNWItem StorageApply(bool bDeposit, int32 Slot, int32 Quantity, bool bWarn, uint8 Tab);
	/** Junta na pilha igual da mesma aba (Item.Tab) ou ocupa um espaço novo nela. */
	static void StorageAdd(TArray<FNWItem>& Items, const FNWItem& Item);
	/** Tira da pilha igual, de preferência da mesma aba. */
	static bool StorageRemove(TArray<FNWItem>& Items, const FNWItem& Item);
};

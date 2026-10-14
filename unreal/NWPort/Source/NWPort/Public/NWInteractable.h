#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWItems.h"
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
	/** Refino (NWCrafting.h): o servidor confere estação perto, nível, materiais e taxa; erro vira aviso ao dono. */
	UFUNCTION(Server, Reliable) void ServerRefine(const FString& StationType, FName Recipe, int32 Count);
	/** Loja (NWShop.h): o servidor confere NPC perto, preço, limite semanal e peso; grava na hora (SavePlayer). */
	UFUNCTION(Server, Reliable) void ServerShopBuy(const FString& ShopId, FName Item, int32 Quantity);
	/** Fabricar arma, escudo ou ferramenta (docs/oficio.md; FNWCrafting::Craft): na hora, item na bolsa; treino abaixo do
	 *  nível de fabricar. */
	UFUNCTION(Server, Reliable) void ServerCraft(const FString& StationType, FName Recipe);
	/** Melhoria do item da bolsa na mesa do ofício dele, pagando materiais (docs/itens.md): Kind = ENWImprove, Mat =
	 *  material escolhido (FNWCrafting::ImproveMaterials). */
	UFUNCTION(Server, Reliable) void ServerImprove(const FString& StationType, int32 BagSlot, uint8 Kind, uint8 Mat);
	/** Resultado da última fabricação (Kind 255) ou melhoria (ENWImprove) para a tela da mesa: o item antes e depois. */
	UFUNCTION(Client, Reliable) void ClientCraftResult(const FNWItem& Before, const FNWItem& After, uint8 Kind);
	struct FCraftResult { FNWItem Before, After; uint8 Kind = 255; int32 Seq = 0; };
	/** Só no dono: Seq sobe a cada resultado (a tela espera um Seq maior que o do clique). */
	FCraftResult CraftResult;

	// ---- armazém da cidade (esteira 4): o servidor guarda e grava no nw-core (bolsa + armazém numa transação)
	/** Peso máximo do armazém (kg), padrão sem dados: storage.base_weight em NWInteractable.cpp. */
	static constexpr float StorageBaseCapacity = 1000.f;
	/** Local aberto ("settlement:queda_eterna") e os itens dele; só o dono recebe. Vazio = fechado. */
	UPROPERTY(Replicated) FString StorageLocation;
	UPROPERTY(Replicated) TArray<FNWItem> StorageItems;
	UPROPERTY(Replicated) float StorageCapacity = StorageBaseCapacity;
	float StorageWeight() const;
	UFUNCTION(Server, Reliable) void ServerStorageOpen(const FString& Location);
	UFUNCTION(Server, Reliable) void ServerStorageClose();
	/** Depositar (Slot = espaço da bolsa) ou retirar (Slot = espaço do armazém), Quantity da pilha. */
	UFUNCTION(Server, Reliable) void ServerStorageMove(bool bDeposit, int32 Slot, int32 Quantity);
	/** Várias pilhas inteiras de uma vez, gravadas numa só transação: "Pegar tudo" (catcontainer ContainerTakeAll) e
	 *  mover a seção inteira (catsection TransferAll). Slots da bolsa (guardar) ou do armazém (retirar). */
	UFUNCTION(Server, Reliable) void ServerStorageMoveMany(bool bDeposit, const TArray<int32>& Slots);

private:
	void LoadClaims();
	bool bClaimsLoaded = false;
	bool bClaimsLoading = false;
	bool bClaimBusy = false;
	FString ClaimsPeriod;
	// Armazém no servidor.
	int64 StorageVersion = 0;
	bool bStorageBusy = false;
	TMap<FString, TArray<FNWItem>> MemStorage;   // sem backend (PIE)
	void StorageSave(const TArray<FNWItem>& Moved, bool bDeposit, bool bRetry);
	/** Move uma pilha (inteira ou Quantity) sem gravar; devolve o que mudou (Quantity 0 = nada). */
	FNWItem StorageApply(bool bDeposit, int32 Slot, int32 Quantity, bool bWarn);
	static void StorageAdd(TArray<FNWItem>& Items, const FNWItem& Item);
	static bool StorageRemove(TArray<FNWItem>& Items, const FNWItem& Item);
};

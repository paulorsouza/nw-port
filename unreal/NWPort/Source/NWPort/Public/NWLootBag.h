#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWItems.h"
#include "NWLootBag.generated.h"

class ANWCharacter;
class UNWInteractableComponent;
class UStaticMeshComponent;

/**
 * Bolsa no chão (esteira 4; como a do jogo, catcontainer no modo bolsa de saque): itens soltos da bolsa ou que não
 * couberam nela (fabricar, coletar, F10). Só quem soltou vê e pega: [E] abre a tela da bolsa (inventário com a janela
 * "Itens Derrubados"), item por item ou Pegar Tudo; o que não couber fica. Some em LifeSeconds (3 min, o
 * inv_discard_life_warning do jogo). Cada jogador pode ter até MaxGroundWeight kg soltos (o excesso de bolsa cheia não conta o limite).
 * Com nw-core, soltar e pegar são operações gravadas (NWOps; saco em ground_bags/ground_items com BagId): o saco volta
 * no mesmo lugar quando o jogador entra (RestoreFor) e sai do mundo quando ele sai (RemoveFor), até vencer.
 */
UCLASS()
class NWPORT_API ANWLootBag : public AActor
{
	GENERATED_BODY()

public:
	ANWLootBag();

	static constexpr float LifeSeconds = 180.f;   // "destruídos após 3 minutos" (inv_discard_life_warning)
	static constexpr float MaxGroundWeight = 1000.f;

	UPROPERTY(ReplicatedUsing = OnRep_Items) TArray<FNWItem> Items;
	UPROPERTY(VisibleAnywhere, Category = "NW") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "NW") TObjectPtr<UNWInteractableComponent> Interact;

	/** Id do saco no nw-core (vazio = só neste servidor, sem nw-core). Só no servidor. */
	FString BagId;

	/** Servidor, sem nw-core: solta os itens numa bolsa nova aos pés do personagem. bLimit confere o peso no chão do
	 *  jogador; com erro devolve nulo e a chave do texto em OutError. Com nw-core, soltar é NWOps::Overflow. */
	static ANWLootBag* Drop(ANWCharacter* Who, const TArray<FNWItem>& InItems, bool bLimit, FString* OutError = nullptr);
	/** Cria o ator do saco (servidor). */
	static ANWLootBag* Spawn(AController* Owner, const FVector& At, const TArray<FNWItem>& InItems, const FString& InBagId, float Life);
	/** Ponto aos pés do personagem, um pouco à frente. */
	static FVector DropPoint(const ANWCharacter* Who);
	/** O jogador pode soltar mais InItems sem passar de MaxGroundWeight no chão. */
	static bool CheckLimit(const ANWCharacter* Who, const TArray<FNWItem>& InItems, FString* OutError);
	/** Sacos do jogador ainda vivos no nw-core voltam ao mundo (neste mapa). */
	static void RestoreFor(class ANWPlayerController* PC);
	/** Tira do mundo os sacos do jogador (continuam no nw-core até vencer). */
	static void RemoveFor(const AController* Owner);
	/** Peso (kg) das bolsas do jogador no chão. */
	static float GroundWeight(const AController* Owner);
	static float ItemsWeight(const TArray<FNWItem>& InItems);
	float Weight() const { return ItemsWeight(Items); }
	/** Servidor: passa para a bolsa do personagem o que couber (operação gravada); vazia, some. */
	void TakeAll(ANWCharacter* Who) { Take(Who, INDEX_NONE, MAX_int32); }
	/** Servidor: passa Quantity do item do espaço Slot (INDEX_NONE = todos), o que couber; vazia, some. */
	void Take(ANWCharacter* Who, int32 Slot, int32 Quantity);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION() void OnRep_Items();
	void UpdateLabel();
};

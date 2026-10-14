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
 * couberam nela (fabricar, coletar, F10). Só quem soltou vê e pega ([E] Pegar tudo; o que não couber fica). Some em
 * LifeSeconds. Cada jogador pode ter até MaxGroundWeight kg soltos (o excesso de bolsa cheia não conta o limite).
 * Não vai para o nw-core: some com o servidor.
 */
UCLASS()
class NWPORT_API ANWLootBag : public AActor
{
	GENERATED_BODY()

public:
	ANWLootBag();

	static constexpr float LifeSeconds = 120.f;
	static constexpr float MaxGroundWeight = 1000.f;

	UPROPERTY(ReplicatedUsing = OnRep_Items) TArray<FNWItem> Items;
	UPROPERTY(VisibleAnywhere, Category = "NW") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "NW") TObjectPtr<UNWInteractableComponent> Interact;

	/** Servidor: solta os itens numa bolsa nova aos pés do personagem. bLimit confere o peso no chão do jogador; com
	 *  erro devolve nulo e a chave do texto em OutError. */
	static ANWLootBag* Drop(ANWCharacter* Who, const TArray<FNWItem>& InItems, bool bLimit, FString* OutError = nullptr);
	/** Peso (kg) das bolsas do jogador no chão. */
	static float GroundWeight(const AController* Owner);
	float Weight() const;
	/** Servidor: passa para a bolsa do personagem o que couber; vazia, some. */
	void TakeAll(ANWCharacter* Who);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION() void OnRep_Items();
	void UpdateLabel();
};

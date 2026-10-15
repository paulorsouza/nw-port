#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWNpc.generated.h"

class UCapsuleComponent;
class UNWInteractableComponent;
class USkeletalMeshComponent;

/**
 * NPC parado da cidade (esteira 4, docs/game/city.md): aparência do .cdf, animação parada da IdleTimeline, nome.
 * Criado por ANWNpcField em cada máquina (não replica: é fixo e sai do mesmo arquivo de dados).
 * Interagir vira o NPC para o jogador e, se ele é o sorteado da semana do ofício, entrega o brinde
 * (FNWNpcRewards, servidor); sem diálogo, porque as missões do jogo não são convertidas.
 */
UCLASS()
class NWPORT_API ANWNpc : public AActor
{
	GENERATED_BODY()

public:
	ANWNpc();
	void Setup(const FString& InVariant, const FText& InName, const FString& Mesh, const FString& Anim);

	UPROPERTY(VisibleAnywhere, Category = "NW|Npc") TObjectPtr<UCapsuleComponent> Capsule;
	UPROPERTY(VisibleAnywhere, Category = "NW|Npc") TObjectPtr<USkeletalMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "NW|Npc") TObjectPtr<UNWInteractableComponent> Interact;
	/** VariantID do jogo (javelindata_variations_npcs). */
	UPROPERTY(VisibleAnywhere, Category = "NW|Npc") FString Variant;
	UPROPERTY(VisibleAnywhere, Category = "NW|Npc") FText DisplayName;
	/** Ofício (grupo dos brindes, tools/npc_rewards.json) e cidade do mapa. */
	UPROPERTY(VisibleAnywhere, Category = "NW|Npc") FString Type;
	UPROPERTY(VisibleAnywhere, Category = "NW|Npc") FString City;
};

/**
 * Põe os NPCs do mapa (Data/World/<cidade>_npcs.json, tools/make_npcs.py). Fica salvo no mapa da cidade
 * (tools/ue/import_npcs.py) e roda no servidor e nos clientes.
 */
UCLASS()
class NWPORT_API ANWNpcField : public AActor
{
	GENERATED_BODY()

public:
	ANWNpcField();
	virtual void BeginPlay() override;

	/** Arquivo em Data/World. */
	UPROPERTY(EditAnywhere, Category = "NW|Npc") FString DataFile = TEXT("queda_eterna_npcs.json");
	/** Pasta do conjunto das animações (tools/sets/npc_queda_eterna.json) e das malhas (NPCs/<SK_nome>). */
	UPROPERTY(EditAnywhere, Category = "NW|Npc") FString AssetRoot = TEXT("/Game/NW/NpcQuedaEterna");

	UPROPERTY(Transient) TArray<TObjectPtr<ANWNpc>> Npcs;

	/** Área da cidade (centro e raio em volta dos NPCs e estações + CityMargin): quem entra passa a conhecer a cidade
	 *  (armazém compartilhado, docs/game/city.md "Armazém"). */
	UPROPERTY(EditAnywhere, Category = "NW|Npc") float CityMargin = 4000.f;

private:
	FString City;
	FVector CityCenter = FVector::ZeroVector;
	float CityRadius = 0.f;
	FTimerHandle DiscoverTimer;
	void CheckDiscover();
};

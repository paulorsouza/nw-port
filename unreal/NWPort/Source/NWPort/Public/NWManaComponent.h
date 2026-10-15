#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWManaComponent.generated.h"

/**
 * Mana do jogador (sub-esteira 1a, docs/armas/manopla-gelo.md §2). Recurso das armas mágicas: manopla de gelo agora,
 * depois cajados e manopla do vazio. Servidor decide; Mana replica para a HUD.
 *
 * ManaComponent do slice do jogador e javelindata_basevitals_player (Player): mín 0, máx 100, inicial 100.
 * Custos: javelindata_manacosts_player, coluna ManaCost das linhas de ação (make_datatables.py).
 * Regeneração e atraso: aguardando manopla-gelo-001 (slice m_regenrate 1 × tabela ManaBaseRegenRate 2, sem unidade).
 * Enquanto isso RegenPerSecond = 0 (nada regenera; F10 > Encher mana).
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWManaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWManaComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float MaxMana = 100.f;
	// Sem fonte ainda (manopla-gelo-001): 0 até a RE responder.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float RegenPerSecond = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") float Mana = 100.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Dev") bool bInfiniteMana = false;

	// IfManaCost GreaterThanOrEqual: a ação só entra com mana ≥ custo.
	bool CanPay(float Cost) const { return Cost <= 0.f || bInfiniteMana || Mana >= Cost; }
	// Servidor. Fórmula com modificadores: aguardando manopla-gelo-002.
	void Pay(float Cost);
	void Add(float Amount);
	void Refill() { Mana = MaxMana; }
	float GetRatio() const { return MaxMana > 0.f ? Mana / MaxMana : 0.f; }

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWManaComponent.generated.h"

/**
 * Player mana (sub-area 1a, docs/game/weapons/ice-gauntlet.md §2). Resource for magic weapons: Ice Gauntlet now,
 * later staffs and the Void Gauntlet. The server decides; Mana replicates to the HUD.
 *
 * ManaComponent of the player slice and javelindata_basevitals_player (Player): min 0, max 100, initial 100.
 * Costs: javelindata_manacosts_player, ManaCost column of the action rows (make_datatables.py).
 * Regeneration and delay: waiting on manopla-gelo-001 (slice m_regenrate 1 × table ManaBaseRegenRate 2, no unit).
 * Meanwhile RegenPerSecond = 0 (nothing regenerates; F10 > Refill mana).
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWManaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWManaComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float MaxMana = 100.f;
	// No source yet (manopla-gelo-001): 0 until the RE answers.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Números") float RegenPerSecond = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Estado") float Mana = 100.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Dev") bool bInfiniteMana = false;

	// IfManaCost GreaterThanOrEqual: the action only starts with mana ≥ cost.
	bool CanPay(float Cost) const { return Cost <= 0.f || bInfiniteMana || Mana >= Cost; }
	// Server. Formula with modifiers: waiting on manopla-gelo-002.
	void Pay(float Cost);
	void Add(float Amount);
	void Refill() { Mana = MaxMana; }
	float GetRatio() const { return MaxMana > 0.f ? Mana / MaxMana : 0.f; }

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};

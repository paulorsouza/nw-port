#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWGatherComponent.generated.h"

class ANWCreature;
class UAnimMontage;
class UStaticMesh;

/**
 * Player gathering (F4/G1). The owner looks for the nearest gatherable (ANWGatherField) and shows the action on screen;
 * E asks the server, which checks distance, tool and that nobody is gathering there, and counts the game time
 * (BaseGatherTime ÷ tool efficiency). Walking, attacking or dying cancels. At the end: loot from the game table
 * into the inventory, and the gatherable is depleted until respawn. Everyone sees the animation and the tool in hand.
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWGatherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWGatherComponent();

	// ---- owner: gatherable in focus and action text ("Mine Iron Vein" or "Requires Pickaxe")
	int32 FocusIndex = INDEX_NONE;
	FString FocusText;
	FString FocusName;   // name of the gatherable or the carcass (card strip)
	bool bFocusBlocked = false;
	/** Carcass in focus (dead creature to skin, G3); takes priority over the gatherable. */
	TWeakObjectPtr<ANWCreature> FocusCreature;
	/** Carcass gathering index (the creature is in GatherActor). */
	static constexpr int32 SkinIndex = -2;
	/** World point of what is in focus or being gathered (HUD card); false = nothing. */
	bool GetFocusLocation(FVector& Out) const;

	bool IsGathering() const { return GatherIndex != INDEX_NONE; }
	/** 0..1 of the gathering in progress. */
	float Progress() const;
	/** Seconds left of the gathering in progress. */
	float Remaining() const;
	/** Gatherable being gathered (or INDEX_NONE). */
	int32 GetGatherIndex() const { return GatherIndex; }
	/** E key. */
	void InteractPressed();

	// Gathering animation per game action (Mining, Cutting, Chopping, Dressing, None = by hand) and tool in hand.
	UPROPERTY(EditAnywhere, Category = "NW|Coleta") TMap<FName, TSoftObjectPtr<UAnimMontage>> Montages;
	UPROPERTY(EditAnywhere, Category = "NW|Coleta") TMap<FName, TSoftObjectPtr<UStaticMesh>> ToolMeshes;

	UFUNCTION(Server, Reliable) void ServerGather(int32 Index);
	UFUNCTION(Server, Reliable) void ServerCancel();
	UFUNCTION(Server, Reliable) void ServerSkin(ANWCreature* Creature);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Gather) int32 GatherIndex = INDEX_NONE;
	UPROPERTY(ReplicatedUsing = OnRep_Gather) FName GatherAction;
	UPROPERTY(Replicated) float GatherStart = 0.f;
	UPROPERTY(Replicated) float GatherEnd = 0.f;
	UPROPERTY(Replicated) TObjectPtr<AActor> GatherActor;
	FVector GatherFrom = FVector::ZeroVector;

	UFUNCTION() void OnRep_Gather();
	void UpdateFocus();
	void Finish(bool bComplete);
	/** Tool required for the action, group name for "Requires ...". */
	static FName ToolType(FName Action);
	static FString ToolGroupKey(FName Action);
	float ServerNow() const;
	TObjectPtr<UAnimMontage> PlayingMontage;
};

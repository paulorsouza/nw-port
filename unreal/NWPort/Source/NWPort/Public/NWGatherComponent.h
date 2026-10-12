#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWGatherComponent.generated.h"

class UAnimMontage;
class UStaticMesh;

/**
 * Coleta do jogador (F4/G1). O dono procura o coletável mais perto (ANWGatherField) e mostra a ação na tela;
 * E pede ao servidor, que confere distância, ferramenta e se ninguém está coletando ali, e conta o tempo do jogo
 * (BaseGatherTime ÷ eficiência da ferramenta). Andar, atacar ou morrer cancela. No fim: saque da tabela do jogo
 * no inventário e o coletável esgota até o respawn. Todos veem a animação e a ferramenta na mão.
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWGatherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWGatherComponent();

	// ---- dono: coletável em foco e texto da ação ("Minerar Filão de Ferro" ou "Requer Picareta")
	int32 FocusIndex = INDEX_NONE;
	FString FocusText;
	bool bFocusBlocked = false;

	bool IsGathering() const { return GatherIndex != INDEX_NONE; }
	/** 0..1 da coleta em andamento. */
	float Progress() const;
	/** Tecla E. */
	void InteractPressed();

	// Animação de coleta por ação do jogo (Mining, Cutting, Chopping, Dressing, None = à mão) e ferramenta na mão.
	UPROPERTY(EditAnywhere, Category = "NW|Coleta") TMap<FName, TSoftObjectPtr<UAnimMontage>> Montages;
	UPROPERTY(EditAnywhere, Category = "NW|Coleta") TMap<FName, TSoftObjectPtr<UStaticMesh>> ToolMeshes;

	UFUNCTION(Server, Reliable) void ServerGather(int32 Index);
	UFUNCTION(Server, Reliable) void ServerCancel();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Gather) int32 GatherIndex = INDEX_NONE;
	UPROPERTY(ReplicatedUsing = OnRep_Gather) FName GatherAction;
	UPROPERTY(Replicated) float GatherStart = 0.f;
	UPROPERTY(Replicated) float GatherEnd = 0.f;
	FVector GatherFrom = FVector::ZeroVector;

	UFUNCTION() void OnRep_Gather();
	void UpdateFocus();
	void Finish(bool bComplete);
	/** Ferramenta exigida para a ação, nome do grupo para "Requer ...". */
	static FName ToolType(FName Action);
	static FString ToolGroupKey(FName Action);
	float ServerNow() const;
	TObjectPtr<UAnimMontage> PlayingMontage;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "NWPlayerState.generated.h"

/** Estado do jogador visível a todos: grupo (0 = sem grupo) e liderança. Grupos são do servidor (ANWGameMode). */
UCLASS()
class NWPORT_API ANWPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Grupo") int32 GroupId = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW|Grupo") bool bGroupLeader = false;

	bool IsGroupedWith(const ANWPlayerState* Other) const { return Other && GroupId != 0 && GroupId == Other->GroupId; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

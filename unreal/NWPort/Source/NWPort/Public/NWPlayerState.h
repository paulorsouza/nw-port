#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "NWPlayerState.generated.h"

/** Player state visible to everyone: group (0 = no group) and leadership. Groups belong to the server (ANWGameMode). */
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

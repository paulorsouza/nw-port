#pragma once

#include "CoreMinimal.h"
#include "NWCharacter.h"
#include "NWLost.generated.h"

// Sword Lost (damned): enemy that chases and alternates the game's attacks. Assets from the lost_sword set.
UCLASS()
class NWPORT_API ANWLost : public ANWCharacter
{
	GENERATED_BODY()

public:
	ANWLost(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
};

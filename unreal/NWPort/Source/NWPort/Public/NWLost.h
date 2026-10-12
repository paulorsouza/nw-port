#pragma once

#include "CoreMinimal.h"
#include "NWCharacter.h"
#include "NWLost.generated.h"

// Lost (damned) de espada: inimigo que persegue e alterna os ataques do jogo. Assets do conjunto lost_sword.
UCLASS()
class NWPORT_API ANWLost : public ANWCharacter
{
	GENERATED_BODY()

public:
	ANWLost(const FObjectInitializer& ObjectInitializer);
};

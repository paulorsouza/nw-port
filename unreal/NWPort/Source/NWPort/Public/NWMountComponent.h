#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWMountComponent.generated.h"

/**
 * Montaria do personagem (esteira 3, docs/esteiras.md e skill nw-montaria): invocar, montar, desmontar, movimento e
 * animação do cavaleiro. Vazio até a esteira começar; já existe no ANWCharacter para a esteira não precisar mexer
 * no cabeçalho do personagem. Locomoção montada: ANWCharacter::SetLocomotionOverride; teclas: NW_REGISTER_INPUT.
 */
UCLASS(ClassGroup = (NW))
class NWPORT_API UNWMountComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWMountComponent();
};

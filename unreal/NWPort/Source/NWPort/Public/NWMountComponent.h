#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWMountComponent.generated.h"

/**
 * Character mount (area 3, docs/game/mounts.md): summon, mount, dismount, movement and
 * rider animation. Empty until the area starts; it already exists in ANWCharacter so the area does not need to touch
 * the character header. Mounted locomotion: ANWCharacter::SetLocomotionOverride; keys: NW_REGISTER_INPUT.
 */
UCLASS(ClassGroup = (NW))
class NWPORT_API UNWMountComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWMountComponent();
};

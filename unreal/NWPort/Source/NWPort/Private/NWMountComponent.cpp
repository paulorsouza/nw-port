#include "NWMountComponent.h"

UNWMountComponent::UNWMountComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

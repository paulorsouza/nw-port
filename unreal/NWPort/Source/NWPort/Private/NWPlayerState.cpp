#include "NWPlayerState.h"

#include "Net/UnrealNetwork.h"

void ANWPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANWPlayerState, GroupId);
	DOREPLIFETIME(ANWPlayerState, bGroupLeader);
}

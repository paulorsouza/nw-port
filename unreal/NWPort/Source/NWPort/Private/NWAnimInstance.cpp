#include "NWAnimInstance.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"
#include "NWCharacter.h"
#include "NWCombatComponent.h"

void UNWAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const ANWCharacter* Char = Cast<ANWCharacter>(TryGetPawnOwner());
	if (!Char)
	{
		return;
	}
	const FVector Vel = Char->GetVelocity();
	Speed = Vel.Size2D();
	Direction = Speed > 1.f ? UKismetAnimationLibrary::CalculateDirection(Vel, Char->GetActorRotation()) : 0.f;
	const float Yaw = Char->GetActorRotation().Yaw;
	if (DeltaSeconds > 0.f)
	{
		TurnRate = FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(LastYaw, Yaw)) / DeltaSeconds;
	}
	LastYaw = Yaw;
	// Override de locomoção (montaria, coleta) pode tirar o sprint: ANWCharacter::SetLocomotionOverride.
	bSprinting = Char->bSprinting && Speed > 450.f && !Char->LocomotionBlocksSprint();
	bInAir = Char->GetCharacterMovement()->IsFalling();
	bBlocking = Char->Combat && Char->Combat->State == ENWCombatState::Blocking;
	bDead = Char->Combat && Char->Combat->State == ENWCombatState::Dead;
}

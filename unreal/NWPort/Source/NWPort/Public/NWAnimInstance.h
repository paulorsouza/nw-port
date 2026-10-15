#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "NWCombatTypes.h"
#include "NWAnimInstance.generated.h"

/**
 * Base of ABP_NW. Computes the variables the graph uses:
 *   locomotion: BS_Strafe (Direction, Speed) or BS_Sprint (Direction, Turn) when bSprinting;
 *   bBlocking plays the block loop; montages go into the DefaultSlot slot.
 */
UCLASS()
class NWPORT_API UNWAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "NW") float Speed = 0.f;      // cm/s on the plane
	UPROPERTY(BlueprintReadOnly, Category = "NW") float Direction = 0.f;  // degrees, -180..180, relative to the body
	UPROPERTY(BlueprintReadOnly, Category = "NW") float TurnRate = 0.f;   // rad/s (BS_Sprint axis)
	UPROPERTY(BlueprintReadOnly, Category = "NW") bool bSprinting = false;
	UPROPERTY(BlueprintReadOnly, Category = "NW") bool bBlocking = false;
	UPROPERTY(BlueprintReadOnly, Category = "NW") bool bInAir = false;
	UPROPERTY(BlueprintReadOnly, Category = "NW") bool bDead = false;
	// Locomotion blend space of the active weapon (the ABP blend space node reads this pin).
	UPROPERTY(BlueprintReadOnly, Category = "NW") TObjectPtr<class UBlendSpace> Locomotion;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	float LastYaw = 0.f;
};

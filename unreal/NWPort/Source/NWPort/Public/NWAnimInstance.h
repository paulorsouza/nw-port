#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "NWCombatTypes.h"
#include "NWAnimInstance.generated.h"

/**
 * Base do ABP_NW. Calcula as variáveis que o grafo usa:
 *   locomoção: BS_Strafe (Direção, Velocidade) ou BS_Sprint (Direção, Giro) quando bSprinting;
 *   bBlocking toca o loop de bloqueio; montagens entram no slot DefaultSlot.
 */
UCLASS()
class NWPORT_API UNWAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "NW") float Speed = 0.f;      // cm/s no plano
	UPROPERTY(BlueprintReadOnly, Category = "NW") float Direction = 0.f;  // graus, -180..180, relativo ao corpo
	UPROPERTY(BlueprintReadOnly, Category = "NW") float TurnRate = 0.f;   // rad/s (eixo do BS_Sprint)
	UPROPERTY(BlueprintReadOnly, Category = "NW") bool bSprinting = false;
	UPROPERTY(BlueprintReadOnly, Category = "NW") bool bBlocking = false;
	UPROPERTY(BlueprintReadOnly, Category = "NW") bool bInAir = false;
	UPROPERTY(BlueprintReadOnly, Category = "NW") bool bDead = false;
	// Blend space de locomoção da arma ativa (o nó de blend space do ABP lê este pino).
	UPROPERTY(BlueprintReadOnly, Category = "NW") TObjectPtr<class UBlendSpace> Locomotion;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	float LastYaw = 0.f;
};

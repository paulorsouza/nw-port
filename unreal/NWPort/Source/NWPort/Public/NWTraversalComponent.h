#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWTraversalComponent.generated.h"

class ACharacter;
class UAnimMontage;
class UCharacterMovementComponent;

UENUM(BlueprintType)
enum class ENWTraversal : uint8
{
	None,
	StepUpLow,
	StepUp,
	VaultLow,
	Vault,
	MantleLow,
	MantleHigh,
};

// Resultado da detecção: o que fazer e onde o personagem termina (pés).
USTRUCT()
struct FNWTraversalPlan
{
	GENERATED_BODY()

	UPROPERTY() ENWTraversal Kind = ENWTraversal::None;
	UPROPERTY() FVector End = FVector::ZeroVector;   // posição final dos pés
	UPROPERTY() float Yaw = 0.f;                     // de frente para o obstáculo
	UPROPERTY() float Height = 0.f;                  // altura do topo acima dos pés (cm)
};

/**
 * Pulo e travessia (degrau, vault, mantle) do jogador, com os parâmetros do TraversalComponent do jogo
 * (slices/player.dynamicslice, TraversalRaycastParams).
 *
 * Espaço: se há obstáculo à frente na faixa de algum movimento, faz a travessia (montagem com root motion
 * ajustado à altura e à distância reais, como o "vault fixup" do jogo); senão, pulo normal.
 * Pulo, queda e pouso são montagens no lugar, tocadas localmente a partir do estado do CharacterMovement.
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWTraversalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWTraversalComponent();

	// ---- faixas do jogo (cm) ----
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float StepUpMinHeight = 25.f;
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float StepUpMaxHeight = 130.f;
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float StepUpRange = 75.f;
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float VaultMinHeight = 25.f;
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float VaultMaxHeight = 130.f;
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float VaultRange = 75.f;
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float VaultMinThickness = 20.f;
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float VaultMaxThickness = 175.f;
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float VaultMaxLandingDiff = 20.f;
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float MantleMinHeight = 120.f;
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float MantleMaxHeight = 300.f;
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float MantleRange = 100.f;
	// Topo precisa ser chão (normal com Z acima disto) e a parede quase vertical.
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float MinFloorNormalZ = 0.7071f;
	// Queda acima disto (s no ar) usa o pouso pesado.
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float HardLandAirTime = 1.0f;
	// Depois disto no ar, a queda vira o loop de "debater".
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float FlailAirTime = 1.2f;
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") bool bDebugDraw = false;
	// Desligado em personagens sem as animações do jogador (ex.: o Lost, outro esqueleto).
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") bool bEnabled = true;

	// Chamar no cliente dono ao apertar pular.
	void JumpPressed();
	UFUNCTION(BlueprintPure, Category = "NW") bool IsTraversing() const { return Active != ENWTraversal::None; }

	// Detecção pura (sem efeito), usada no cliente e repetida no servidor.
	FNWTraversalPlan Detect() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(Server, Reliable) void ServerTraverse();
	UFUNCTION(NetMulticast, Reliable) void MulticastStartTraversal(ENWTraversal Kind, FVector Start, FNWTraversalPlan Plan);

private:
	void LoadMontages();
	UAnimMontage* MontageFor(ENWTraversal Kind) const;
	void FinishTraversal();
	FTransform WarpRootMotion(const FTransform& Delta, UCharacterMovementComponent* Move, float DeltaSeconds);
	void TickAirAnimation(float DeltaTime);
	UAnimMontage* Directional(const TArray<TObjectPtr<UAnimMontage>>& Set) const;
	bool CapsuleFits(const FVector& Feet) const;
	ACharacter* GetCharacter() const;

	UPROPERTY(Replicated) ENWTraversal Active = ENWTraversal::None;

	UPROPERTY() TMap<ENWTraversal, TObjectPtr<UAnimMontage>> TraversalMontages;
	// Root motion natural de cada montagem de travessia (cm): frente e cima, no espaço da malha.
	TMap<ENWTraversal, FVector2D> NaturalMotion;
	UPROPERTY() TObjectPtr<UAnimMontage> JumpStand;
	UPROPERTY() TArray<TObjectPtr<UAnimMontage>> JumpDir;     // F, FR, R, BR, B, BL, L, FL
	UPROPERTY() TArray<TObjectPtr<UAnimMontage>> LandMoveDir; // mesma ordem
	UPROPERTY() TObjectPtr<UAnimMontage> Fall;
	UPROPERTY() TObjectPtr<UAnimMontage> FallFlail;
	UPROPERTY() TObjectPtr<UAnimMontage> Land;
	UPROPERTY() TObjectPtr<UAnimMontage> LandHard;

	// Travessia em curso (todas as máquinas).
	FVector TravStart = FVector::ZeroVector;
	FNWTraversalPlan TravPlan;
	FVector2D WarpScale = FVector2D(1.f, 1.f);   // frente, cima (multiplicador)
	FVector2D WarpAdd = FVector2D::ZeroVector;   // frente, cima (cm por segundo de montagem)
	TObjectPtr<UAnimMontage> TravMontage;
	ECollisionResponse SavedStaticResponse = ECR_Block;
	ECollisionResponse SavedDynamicResponse = ECR_Block;

	// Animação no ar (local).
	bool bWasFalling = false;
	float NextAirCheck = 0.f;
	float AirTime = 0.f;
	TObjectPtr<UAnimMontage> AirMontage;
};

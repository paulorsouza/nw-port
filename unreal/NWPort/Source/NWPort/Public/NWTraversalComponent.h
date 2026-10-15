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

// Detection result: what to do and where the character ends up (feet).
USTRUCT()
struct FNWTraversalPlan
{
	GENERATED_BODY()

	UPROPERTY() ENWTraversal Kind = ENWTraversal::None;
	UPROPERTY() FVector End = FVector::ZeroVector;   // final position of the feet
	UPROPERTY() float Yaw = 0.f;                     // facing the obstacle
	UPROPERTY() float Height = 0.f;                  // height of the top above the feet (cm)
};

/**
 * Player jump and traversal (step, vault, mantle), with the parameters of the game's TraversalComponent
 * (slices/player.dynamicslice, TraversalRaycastParams).
 *
 * Space: if there is an obstacle ahead within the range of some move, performs the traversal (montage with root motion
 * adjusted to the real height and distance, like the game's "vault fixup"); otherwise, a normal jump.
 * Jump, fall and landing are in-place montages, played locally from the CharacterMovement state.
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWTraversalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWTraversalComponent();

	// ---- game ranges (cm) ----
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
	// The top must be ground (normal with Z above this) and the wall almost vertical.
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float MinFloorNormalZ = 0.7071f;
	// A fall above this (s in the air) uses the heavy landing.
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float HardLandAirTime = 1.0f;
	// After this long in the air, the fall becomes the "flailing" loop.
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") float FlailAirTime = 1.2f;
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") bool bDebugDraw = false;
	// Off on characters without the player animations (e.g. the Lost, another skeleton).
	UPROPERTY(EditAnywhere, Category = "NW|Travessia") bool bEnabled = true;

	// Call on the owning client when jump is pressed.
	void JumpPressed();
	UFUNCTION(BlueprintPure, Category = "NW") bool IsTraversing() const { return Active != ENWTraversal::None; }

	// Pure detection (no side effects), used on the client and repeated on the server.
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
	// Natural root motion of each traversal montage (cm): forward and up, in mesh space.
	TMap<ENWTraversal, FVector2D> NaturalMotion;
	UPROPERTY() TObjectPtr<UAnimMontage> JumpStand;
	UPROPERTY() TArray<TObjectPtr<UAnimMontage>> JumpDir;     // F, FR, R, BR, B, BL, L, FL
	UPROPERTY() TArray<TObjectPtr<UAnimMontage>> LandMoveDir; // same order
	UPROPERTY() TObjectPtr<UAnimMontage> Fall;
	UPROPERTY() TObjectPtr<UAnimMontage> FallFlail;
	UPROPERTY() TObjectPtr<UAnimMontage> Land;
	UPROPERTY() TObjectPtr<UAnimMontage> LandHard;

	// Traversal in progress (all machines).
	FVector TravStart = FVector::ZeroVector;
	FNWTraversalPlan TravPlan;
	FVector2D WarpScale = FVector2D(1.f, 1.f);   // forward, up (multiplier)
	FVector2D WarpAdd = FVector2D::ZeroVector;   // forward, up (cm per second of montage)
	TObjectPtr<UAnimMontage> TravMontage;
	ECollisionResponse SavedStaticResponse = ECR_Block;
	ECollisionResponse SavedDynamicResponse = ECR_Block;

	// In-air animation (local).
	bool bWasFalling = false;
	float NextAirCheck = 0.f;
	float AirTime = 0.f;
	TObjectPtr<UAnimMontage> AirMontage;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWBrainComponent.generated.h"

class ANWCharacter;

/** Fighting style. The numbers of each one live in the component fields (presets in UNWBrainComponent::ApplyStyle). */
UENUM(BlueprintType)
enum class ENWBrainStyle : uint8
{
	// Chases and attacks at the interval (the old behavior of the attacking dummy).
	Brawler,
	// Keeps distance, circles, comes in with a sequence of hits and backs off; blocks and dodges when it can.
	Duelist,
	// Pack: at most AttackersPerTarget attack together; the others surround from the flanks waiting for their turn.
	Pack,
	// Charge from afar (approach attack) and short hit up close; after the charge it is exposed.
	Charger,
	// Stays in the preferred distance band, backs off if the target comes close, and shoots.
	Archer,
};

/** An attack the brain can choose: action table row, distance band (cm), weight and cooldown. */
USTRUCT(BlueprintType)
struct FNWBrainAttack
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Action;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinRange = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxRange = 250.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Weight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Cooldown = 0.f;
	// Attack that moves forward (charge, leap): keeps pushing forward while the montage plays.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bGapCloser = false;
	// Hits in sequence (light combo): how many requests of the same attack, slotted by the combat buffer.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Chain = 1;
	// Next actions, each one when the previous ends (e.g. prepare charge → charge → brake).
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> FollowUps;
};

/**
 * Enemy combat AI (server only). Picks a target (closest player within the owner's AggroRange),
 * walks, circles, attacks and defends by requesting the same actions the player requests from UNWCombatComponent: no
 * shortcuts, the AI obeys windows, cooldowns and stamina.
 *
 * With the component active, ANWCharacter does not run the old TickDummy.
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWBrainComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro") ENWBrainStyle Style = ENWBrainStyle::Brawler;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro") TArray<FNWBrainAttack> Attacks;
	// Pause between charges (s, drawn between the two).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro") FVector2D AttackPause = FVector2D(1.2f, 2.4f);
	// Waiting distance: duelist and pack circle at it; archer shoots from it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro") float HoldDistance = 380.f;
	// Chance to raise the guard / dodge when the target attacks up close (needs the BlockStart/Dodge_* rows).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro", meta = (ClampMin = 0, ClampMax = 1)) float BlockChance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro", meta = (ClampMin = 0, ClampMax = 1)) float DodgeChance = 0.f;
	// Time to react to a hit from the target (s).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro") float ReactionTime = 0.18f;
	// How many of this style can attack the same target at the same time (pack and duelists).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro") int32 AttackersPerTarget = 1;
	// Quadruped: walks facing where it goes (circles in an arc) and only faces the target to attack.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Cérebro") bool bFaceMovement = false;

	/** Applies the default numbers of a style (the fields can be tuned afterwards). */
	void ApplyStyle(ENWBrainStyle NewStyle);

	ANWCharacter* GetTarget() const { return Target.Get(); }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	enum class EMode : uint8 { Approach, Hold, Attack, Recover };

	ANWCharacter* FindTarget(float& OutDist) const;
	const FNWBrainAttack* PickAttack(float Dist) const;
	void StartAttack(const FNWBrainAttack& A);
	void TickDefense(ANWCharacter* Me, ANWCharacter* T, float Dist, float DeltaTime);
	void Steer(ANWCharacter* Me, const FVector& Dir, bool bSlow);
	void Face(ANWCharacter* Me, const FVector& Dir, float DeltaTime, float Rate);
	bool TakeToken();
	void ReleaseToken();
	int32 PackIndex() const;

	TWeakObjectPtr<ANWCharacter> Target;
	EMode Mode = EMode::Approach;
	float ModeTime = 0.f;
	float ModeLength = 0.f;
	float NextAttackAt = 0.f;
	float StrafeSign = 1.f;
	bool bHasToken = false;
	const FNWBrainAttack* Current = nullptr;
	int32 ChainLeft = 0;
	int32 FollowIndex = 0;
	float ChainTimer = 0.f;
	TMap<FName, float> ReadyAt;
	// Defense: last target hit already evaluated (action number) and when to react.
	int32 SeenTargetAction = -1;
	float ReactAt = -1.f;
	bool bWillBlock = false;
	bool bWillDodge = false;
	float BlockUntil = 0.f;
};

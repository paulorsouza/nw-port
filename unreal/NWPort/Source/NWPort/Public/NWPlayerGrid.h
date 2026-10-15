#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NWPlayerGrid.generated.h"

class ANWCharacter;

/**
 * Players near a point without scanning the world (docs/tech/architecture.md, rule 1).
 *
 * Uniform grid (cells of CellSize) with the characters that count as players (ANWCharacter::IsPlayerLike: with a
 * connection or simulated by the load test), rebuilt every RebuildSeconds with a single pass over the characters. Whoever
 * needs "players closer than R" (creature sight, spawn, aggro) queries only the surrounding cells.
 * It may be up to RebuildSeconds behind: whoever queries checks the distance again.
 */
UCLASS()
class NWPORT_API UNWPlayerGrid : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr float CellSize = 5000.f;       // 50 m
	static constexpr float RebuildSeconds = 0.25f;

	static UNWPlayerGrid* Get(const UWorld* World);
	/** Calls Fn for each player, alive or dead, closer than Radius (2D) to At. */
	void ForEachNear(const FVector& At, float Radius, TFunctionRef<void(ANWCharacter*)> Fn);
	/** All players (position from the last rebuild). */
	const TArray<TWeakObjectPtr<ANWCharacter>>& All() { Refresh(); return Players; }

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:
	TArray<TWeakObjectPtr<ANWCharacter>> Players;
	TMap<FIntPoint, TArray<int32>> Cells;
	double BuiltAt = -1.0;
	void Refresh();
	static FIntPoint CellOf(const FVector& P) { return FIntPoint(FMath::FloorToInt(P.X / CellSize), FMath::FloorToInt(P.Y / CellSize)); }
};

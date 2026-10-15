#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWLoadTest.generated.h"

class ANWCharacter;

/**
 * Server load test (docs/tech/architecture.md, "Load test").
 *
 * - Simulated players: `nw.sim <n> [raio_m]` (or `-nwsim=<n> -nwsimraio=<m>` on the server command line) creates n
 *   characters on the server, without a connection, with an AI controller and marked as players (ANWCharacter::bSimPlayer:
 *   creature spawning and AI treat them as players). Roles: 30% walk in the city (40 m from the start), 50% walk across the map
 *   (up to the radius), 20% walk and attack. `nw.sim 0` removes all.
 * - Report: `nw.perf 1` (or `-nwperf`) writes every 10 s to the log (`NWPerf:`) and to Saved/Perf/perf_<date>.csv the
 *   work time of the world cycle (mean, p95, max, without the wait of the 30/s limit), the real interval between cycles,
 *   connected players, simulated, creatures, characters, bytes sent per second and memory.
 * - Profile: `-nwcsv=<cycles>` writes Unreal's CSV profile (time per category in each cycle) `-nwcsvdelay=<s>` (default
 *   15) after creating the simulated players, in Saved/Profiling/CSV (`tools/perf_csv.py` summarizes).
 * - Gatherers: `-nwsimgather` makes every simulated player a gatherer instead: tools in the bag, walks to the nearest
 *   free gatherable, gathers through the real server path (ServerGather) standing still, then the next one.
 * - Experiments: `-nwexp=meshcol,floor` changes a setting on all characters only in the test, to measure before
 *   taking it to the game (meshcol: mesh without collision; floor: fewer floor checks).
 *
 * The test itself costs little (one step per simulated player each cycle, without scanning the world), but it is in the measurement.
 */
UCLASS(NotPlaceable, Transient)
class NWPORT_API ANWLoadTest : public AActor
{
	GENERATED_BODY()

public:
	ANWLoadTest();

	/** Server: keeps n simulated players in the world (creates or removes), spread up to RadiusM meters from the start. */
	static void SetSims(UWorld* World, int32 Count, float RadiusM);
	/** Turns the world performance report on or off. */
	static void SetPerf(UWorld* World, bool bOn);

	virtual void Tick(float DeltaSeconds) override;

private:
	enum class ERole : uint8 { City, Roam, Fight, Gather };
	struct FSim
	{
		TWeakObjectPtr<ANWCharacter> Pawn;
		ERole Role = ERole::Roam;
		FVector Goal = FVector::ZeroVector;
		FVector LastPos = FVector::ZeroVector;
		double NextCheck = 0.0;
		double NextAttack = 0.0;
		int32 Node = INDEX_NONE;   // gatherer: target gatherable
		bool bWasGathering = false;
	};
	TArray<FSim> Sims;
	FVector Anchor = FVector::ZeroVector;
	float Radius = 60000.f;
	bool bHasAnchor = false;
	/** Gatherers: gatherables within the radius in 50 m cells, computed once (the region has ~100 thousand). */
	TMap<FIntPoint, TArray<int32>> NodeCells;
	bool bNodesReady = false;
	/** Gatherers: counts for the 10 s report (the test checks itself). */
	int32 GatherStarts = 0, GatherRefused = 0, GatherNoNode = 0;
	double NextGatherReport = 0.0;

	static ANWLoadTest* Get(UWorld* World, bool bCreate);
	void Resize(int32 Count);
	bool FindGround(const FVector& Around, float R, FVector& Out) const;
	void PickGoal(FSim& S);
	/** Gatherer: nearest free gatherable around the character (another gatherer's target does not count). */
	void PickNode(FSim& S, const FVector& Pos);
	void TickGatherer(FSim& S, ANWCharacter* P, const FVector& Pos, double Now);
};

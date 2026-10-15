#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWSpawnField.generated.h"

class ANWCreature;

/**
 * Creature spawners of the region (G3, Data/World/<region>_spawns.json, tools/make_spawns.py). Server only:
 * every second, spawns what is within SpawnRadius of some player (and past its respawn time) and removes what
 * is far from everyone (out of combat). Only creatures with a converted set (boar, wolf...).
 */
UCLASS()
class NWPORT_API ANWSpawnField : public AActor
{
	GENERATED_BODY()

public:
	ANWSpawnField();
	static FString DataFileFor(const UWorld* World);
	virtual void BeginPlay() override;
	static ANWSpawnField* Get(const UWorld* World);
	/** Spawner with a set closest to From, farther than MinDist (dev menu: go to a creature). */
	bool NearestSpawn(const FVector& From, float MinDist, FVector& Out) const;

	UPROPERTY(EditAnywhere, Category = "NW|Spawn") float SpawnRadius = 6000.f;
	UPROPERTY(EditAnywhere, Category = "NW|Spawn") float DespawnRadius = 9000.f;
	UPROPERTY(EditAnywhere, Category = "NW|Spawn") FVector2D RespawnSeconds = FVector2D(120.f, 300.f);
	UPROPERTY(EditAnywhere, Category = "NW|Spawn") float CorpseSeconds = 60.f;

private:
	struct FSpawn
	{
		FName Creature;
		FVector Location;
		float Yaw = 0.f;
		TArray<FVector> Path;
		TArray<float> Waits;
		TWeakObjectPtr<ANWCreature> Alive;
		double ReadyAt = 0.0;
		double DiedAt = -1.0;
	};
	void Step();
	bool Load(const FString& File);
	FTimerHandle Timer;
	TArray<FSpawn> Spawns;
};

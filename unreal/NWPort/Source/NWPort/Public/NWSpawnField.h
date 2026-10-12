#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWSpawnField.generated.h"

class ANWCreature;

/**
 * Spawners de criaturas da região (G3, Data/World/<região>_spawns.json, tools/make_spawns.py). Só no servidor:
 * a cada segundo nasce o que está a até SpawnRadius de algum jogador (e já passou do tempo de renascer) e some o que
 * ficou longe de todos (fora de combate). Só criaturas com conjunto convertido (javali, lobo...).
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
	/** Spawner com conjunto mais perto de From, a mais de MinDist (menu de dev: ir até uma criatura). */
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

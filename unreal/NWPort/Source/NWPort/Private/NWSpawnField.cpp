#include "NWSpawnField.h"
#include "NWPlayerGrid.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "NWCombatComponent.h"
#include "NWCreature.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"

ANWSpawnField::ANWSpawnField()
{
	bReplicates = false;   // only the server decides; the creatures replicate on their own
}

FString ANWSpawnField::DataFileFor(const UWorld* World)
{
	const FString Map = World ? World->GetMapName() : FString();
	if (Map.Contains(TEXT("SouthWindsward")))
	{
		return FPaths::ProjectDir() / TEXT("Data/World/guardaventos_spawns.json");
	}
	return FString();
}

ANWSpawnField* ANWSpawnField::Get(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ANWSpawnField> It(const_cast<UWorld*>(World));
	return It ? *It : nullptr;
}

bool ANWSpawnField::NearestSpawn(const FVector& From, float MinDist, FVector& Out) const
{
	float Best = TNumericLimits<float>::Max();
	for (const FSpawn& S : Spawns)
	{
		const float D = FVector::Dist2D(S.Location, From);
		if (D > MinDist && D < Best)
		{
			Best = D;
			Out = S.Location;
		}
	}
	return Best < TNumericLimits<float>::Max();
}

bool ANWSpawnField::Load(const FString& File)
{
	FNWCreatureCatalog::Get().Load(File);
	FString Raw;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Raw, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("spawns")))
	{
		const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
		FSpawn S;
		S.Creature = FName(A[0]->AsString());
		const FNWCreatureDef* D = FNWCreatureCatalog::Get().Find(S.Creature);
		if (!D || D->Set.IsNone())
		{
			continue;   // set not converted yet
		}
		S.Location = FVector(A[1]->AsNumber(), A[2]->AsNumber(), A[3]->AsNumber());
		S.Yaw = A[4]->AsNumber();
		for (const TSharedPtr<FJsonValue>& W : A[5]->AsArray())
		{
			const TArray<TSharedPtr<FJsonValue>>& P = W->AsArray();
			S.Path.Add(FVector(P[0]->AsNumber(), P[1]->AsNumber(), P[2]->AsNumber()));
			S.Waits.Add(FMath::FRandRange(P[4]->AsNumber(), FMath::Max(P[4]->AsNumber(), P[5]->AsNumber())));
		}
		Spawns.Add(MoveTemp(S));
	}
	return Spawns.Num() > 0;
}

void ANWSpawnField::BeginPlay()
{
	Super::BeginPlay();
	const FString File = DataFileFor(GetWorld());
	if (!HasAuthority() || File.IsEmpty() || !Load(File))
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("NW: %d creature spawners"), Spawns.Num());
	GetWorldTimerManager().SetTimer(Timer, this, &ANWSpawnField::Step, 1.f, true, 2.f);
}

void ANWSpawnField::Step()
{
	UWorld* World = GetWorld();
	TArray<FVector> Players;
	if (UNWPlayerGrid* Grid = UNWPlayerGrid::Get(World))
	{
		for (const TWeakObjectPtr<ANWCharacter>& P : Grid->All())
		{
			if (P.IsValid())
			{
				Players.Add(P->GetActorLocation());
			}
		}
	}
	const double Now = World->GetTimeSeconds();
	auto Nearest = [&Players](const FVector& L)
	{
		float Best = TNumericLimits<float>::Max();
		for (const FVector& P : Players)
		{
			Best = FMath::Min(Best, float(FVector::Dist2D(P, L)));
		}
		return Best;
	};
	int32 Budget = 4;   // at most 4 spawns per second (avoids a hitch when arriving at a POI)
	for (FSpawn& S : Spawns)
	{
		if (ANWCreature* C = S.Alive.Get())
		{
			const bool bDead = C->Combat->State == ENWCombatState::Dead;
			if (bDead && S.DiedAt < 0.0)
			{
				S.DiedAt = Now;
			}
			// Carcass disappears after a while (or soon after being skinned); respawns later.
			if (bDead && (Now - S.DiedAt > CorpseSeconds || (C->bSkinned && Now - S.DiedAt > 5.0)))
			{
				C->Destroy();
				S.Alive.Reset();
				S.ReadyAt = Now + FMath::FRandRange(RespawnSeconds.X, RespawnSeconds.Y);
				continue;
			}
			// Far from everyone and out of combat: disappears without counting as a death.
			if (!bDead && Nearest(C->GetActorLocation()) > DespawnRadius && !C->Combat->IsBusy())
			{
				C->Destroy();
				S.Alive.Reset();
			}
			continue;
		}
		if (Now < S.ReadyAt || Budget <= 0 || Nearest(S.Location) > SpawnRadius)
		{
			continue;
		}
		const FNWCreatureDef* D = FNWCreatureCatalog::Get().Find(S.Creature);
		// Ground under the game point (the data height is the game's; the Unreal terrain may differ a little).
		FVector At = S.Location + FVector(0.f, 0.f, 150.f);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, S.Location + FVector(0.f, 0.f, 1000.f), S.Location - FVector(0.f, 0.f, 2000.f), ECC_WorldStatic))
		{
			At = Hit.Location + FVector(0.f, 0.f, 120.f);
		}
		const FTransform T(FRotator(0.f, S.Yaw, 0.f), At);
		ANWCreature* C = World->SpawnActorDeferred<ANWCreature>(ANWCreature::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!C || !D)
		{
			continue;
		}
		C->Setup(*D, S.Path, S.Waits);
		C->FinishSpawning(T);
		S.Alive = C;
		S.DiedAt = -1.0;
		--Budget;
	}
}

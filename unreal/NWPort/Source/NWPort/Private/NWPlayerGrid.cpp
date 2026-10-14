#include "NWPlayerGrid.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "NWCharacter.h"

UNWPlayerGrid* UNWPlayerGrid::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UNWPlayerGrid>() : nullptr;
}

bool UNWPlayerGrid::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UNWPlayerGrid::Refresh()
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (BuiltAt >= 0.0 && Now - BuiltAt < RebuildSeconds)
	{
		return;
	}
	BuiltAt = Now;
	Players.Reset();
	Cells.Reset();
	// Uma passada pelos personagens a cada RebuildSeconds, no lugar de uma por criatura por ciclo.
	for (TActorIterator<ANWCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsPlayerLike() && !It->IsActorBeingDestroyed())
		{
			Cells.FindOrAdd(CellOf(It->GetActorLocation())).Add(Players.Add(*It));
		}
	}
}

void UNWPlayerGrid::ForEachNear(const FVector& At, float Radius, TFunctionRef<void(ANWCharacter*)> Fn)
{
	Refresh();
	const FIntPoint Lo = CellOf(At - FVector(Radius, Radius, 0.f)), Hi = CellOf(At + FVector(Radius, Radius, 0.f));
	const float R2 = Radius * Radius;
	for (int32 X = Lo.X; X <= Hi.X; ++X)
	{
		for (int32 Y = Lo.Y; Y <= Hi.Y; ++Y)
		{
			const TArray<int32>* C = Cells.Find(FIntPoint(X, Y));
			if (!C)
			{
				continue;
			}
			for (int32 i : *C)
			{
				ANWCharacter* P = Players[i].Get();
				if (P && FVector::DistSquared2D(P->GetActorLocation(), At) <= R2)
				{
					Fn(P);
				}
			}
		}
	}
}

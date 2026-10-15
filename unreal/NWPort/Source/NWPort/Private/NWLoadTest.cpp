#include "NWLoadTest.h"

#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NWCharacter.h"
#include "NWCombatComponent.h"
#include "NWCreature.h"
#include "NWGatherComponent.h"
#include "NWGatherField.h"
#include "NWInventoryComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogNWLoad, Log, All);

namespace
{
	// ---------------------------------------------------------------- performance report

	/** Work time of each world cycle (start → end of UWorld::Tick, without the wait of the cycle limit). */
	struct FNWPerf
	{
		TWeakObjectPtr<UWorld> World;
		FDelegateHandle Start, End;
		double TickStart = 0.0, WindowStart = 0.0, LastEnd = 0.0;
		TArray<float> WorkMs, FrameMs;
		FString CsvPath;

		void On(UWorld* W)
		{
			Off();
			World = W;
			WindowStart = FPlatformTime::Seconds();
			Start = FWorldDelegates::OnWorldTickStart.AddLambda([this](UWorld* Wd, ELevelTick, float)
			{
				if (Wd == World.Get())
				{
					TickStart = FPlatformTime::Seconds();
				}
			});
			End = FWorldDelegates::OnWorldTickEnd.AddLambda([this](UWorld* Wd, ELevelTick, float)
			{
				if (Wd != World.Get() || TickStart <= 0.0)
				{
					return;
				}
				const double Now = FPlatformTime::Seconds();
				WorkMs.Add(float((Now - TickStart) * 1000.0));
				if (LastEnd > 0.0)
				{
					FrameMs.Add(float((Now - LastEnd) * 1000.0));
				}
				LastEnd = Now;
				if (Now - WindowStart >= 10.0)
				{
					Report(Wd, Now - WindowStart);
					WindowStart = Now;
				}
			});
			CsvPath = FPaths::ProjectSavedDir() / TEXT("Perf") / FString::Printf(TEXT("perf_%s.csv"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
			FFileHelper::SaveStringToFile(TEXT("tempo_s,ciclos,trabalho_med_ms,trabalho_p95_ms,trabalho_max_ms,intervalo_med_ms,jogadores,simulados,criaturas,personagens,saida_kbps,memoria_mb\n"), *CsvPath);
			UE_LOG(LogNWLoad, Display, TEXT("NWPerf on: %s"), *CsvPath);
		}

		void Off()
		{
			FWorldDelegates::OnWorldTickStart.Remove(Start);
			FWorldDelegates::OnWorldTickEnd.Remove(End);
			Start.Reset();
			End.Reset();
			WorkMs.Reset();
			FrameMs.Reset();
			TickStart = LastEnd = 0.0;
		}

		void Report(UWorld* Wd, double Seconds)
		{
			if (WorkMs.Num() == 0)
			{
				return;
			}
			TArray<float> Sorted = WorkMs;
			Sorted.Sort();
			float Sum = 0.f, FrameSum = 0.f;
			for (float V : WorkMs)
			{
				Sum += V;
			}
			for (float V : FrameMs)
			{
				FrameSum += V;
			}
			const float Avg = Sum / WorkMs.Num();
			const float P95 = Sorted[FMath::Min(Sorted.Num() - 1, int32(Sorted.Num() * 0.95f))];
			const float Max = Sorted.Last();
			const float Frame = FrameMs.Num() ? FrameSum / FrameMs.Num() : 0.f;
			// Counts: scanning the world once every 10 s is a cost of the report, not of the game.
			int32 Players = 0, Sims = 0, Creatures = 0, Chars = 0;
			for (FConstPlayerControllerIterator It = Wd->GetPlayerControllerIterator(); It; ++It)
			{
				Players += It->Get() && It->Get()->GetNetConnection() ? 1 : 0;
			}
			for (TActorIterator<ANWCharacter> It(Wd); It; ++It)
			{
				++Chars;
				Sims += It->bSimPlayer ? 1 : 0;
				Creatures += It->IsA<ANWCreature>() ? 1 : 0;
			}
			const UNetDriver* Net = Wd->GetNetDriver();
			const float OutKbps = Net ? Net->OutBytesPerSecond * 8.f / 1000.f : 0.f;
			const float MemMb = float(FPlatformMemory::GetStats().UsedPhysical) / (1024.f * 1024.f);
			UE_LOG(LogNWLoad, Display, TEXT("NWPerf: %d cycles in %.0f s | work avg %.2f p95 %.2f max %.2f ms | interval %.1f ms | players %d simulated %d creatures %d characters %d | out %.0f kbit/s | memory %.0f MB"),
				WorkMs.Num(), Seconds, Avg, P95, Max, Frame, Players, Sims, Creatures, Chars, OutKbps, MemMb);
			const FString Line = FString::Printf(TEXT("%.0f,%d,%.3f,%.3f,%.3f,%.2f,%d,%d,%d,%d,%.0f,%.0f\n"), Wd->GetTimeSeconds(), WorkMs.Num(), Avg, P95, Max, Frame,
				Players, Sims, Creatures, Chars, OutKbps, MemMb);
			FFileHelper::SaveStringToFile(Line, *CsvPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
			WorkMs.Reset();
			FrameMs.Reset();
		}
	};
	FNWPerf Perf;

	// ---------------------------------------------------------------- commands

	UWorld* ServerWorld(UWorld* Given)
	{
		if (Given && Given->GetNetMode() != NM_Client)
		{
			return Given;
		}
		for (const FWorldContext& C : GEngine ? GEngine->GetWorldContexts() : TIndirectArray<FWorldContext>())
		{
			UWorld* W = C.World();
			if (W && (C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE) && W->GetNetMode() != NM_Client)
			{
				return W;
			}
		}
		return nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs SimCmd(TEXT("nw.sim"),
		TEXT("Load test: nw.sim <n> [raio_m=600] creates n simulated players on the server (0 removes)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UWorld* W = ServerWorld(World))
			{
				ANWLoadTest::SetSims(W, Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 600.f);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs PerfCmd(TEXT("nw.perf"),
		TEXT("Server performance report every 10 s: nw.perf 1 | nw.perf 0."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UWorld* W = ServerWorld(World))
			{
				ANWLoadTest::SetPerf(W, Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
			}
		}));

	/** Server command line: -nwperf turns on the report and -nwsim=<n> -nwsimraio=<m> creates the simulated players 10 s
	 *  after the map comes up (creature spawning and the rest have already started). */
	struct FNWLoadAuto
	{
		FNWLoadAuto()
		{
			FWorldDelegates::OnWorldInitializedActors.AddLambda([](const FActorsInitializedParams& P)
			{
				UWorld* W = P.World;
				if (!W || W->GetNetMode() == NM_Client || !(W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE))
				{
					return;
				}
				int32 Count = 0;
				float RadiusM = 600.f;
				FParse::Value(FCommandLine::Get(), TEXT("-nwsim="), Count);
				FParse::Value(FCommandLine::Get(), TEXT("-nwsimraio="), RadiusM);
				if (FParse::Param(FCommandLine::Get(), TEXT("nwperf")))
				{
					ANWLoadTest::SetPerf(W, true);
				}
				if (Count > 0)
				{
					FTimerHandle H;
					W->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WeakW = TWeakObjectPtr<UWorld>(W), Count, RadiusM]()
					{
						if (UWorld* W2 = WeakW.Get())
						{
							ANWLoadTest::SetSims(W2, Count, RadiusM);
							// -nwcsv=<cycles>: Unreal CSV profile (time per category) 15 s later, in Saved/Profiling/CSV.
							int32 Frames = 0;
							float Delay = 15.f;
							FParse::Value(FCommandLine::Get(), TEXT("-nwcsvdelay="), Delay);
							if (FParse::Value(FCommandLine::Get(), TEXT("-nwcsv="), Frames) && Frames > 0)
							{
								FTimerHandle H2;
								W2->GetTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda([WeakW, Frames]()
								{
									if (UWorld* W3 = WeakW.Get(); W3 && GEngine)
									{
										GEngine->Exec(W3, *FString::Printf(TEXT("csvprofile frames=%d start"), Frames));
									}
								}), FMath::Max(1.f, Delay), false);
							}
						}
					}), 10.f, false);
				}
			});
		}
	};
	FNWLoadAuto LoadAuto;
}

// ---------------------------------------------------------------- simulated players

ANWLoadTest::ANWLoadTest()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
}

ANWLoadTest* ANWLoadTest::Get(UWorld* World, bool bCreate)
{
	TActorIterator<ANWLoadTest> It(World);
	if (It)
	{
		return *It;
	}
	return bCreate ? World->SpawnActor<ANWLoadTest>() : nullptr;
}

void ANWLoadTest::SetSims(UWorld* World, int32 Count, float RadiusM)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	if (ANWLoadTest* T = Get(World, Count > 0))
	{
		T->Radius = FMath::Max(10.f, RadiusM) * 100.f;
		T->Resize(FMath::Clamp(Count, 0, 2000));
	}
}

void ANWLoadTest::SetPerf(UWorld* World, bool bOn)
{
	if (bOn)
	{
		Perf.On(World);
	}
	else
	{
		Perf.Off();
	}
}

bool ANWLoadTest::FindGround(const FVector& Around, float R, FVector& Out) const
{
	for (int32 Try = 0; Try < 8; ++Try)
	{
		const FVector2D Off = FMath::RandPointInCircle(R);
		const FVector Top = Around + FVector(Off.X, Off.Y, 50000.f);
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(NWLoadGround), false);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Top, Top - FVector(0.f, 0.f, 100000.f), ECC_Visibility, Q))
		{
			Out = Hit.ImpactPoint + FVector(0.f, 0.f, 100.f);
			return true;
		}
	}
	return false;
}

void ANWLoadTest::PickGoal(FSim& S)
{
	const float R = S.Role == ERole::City ? 4000.f : Radius;
	FVector G;
	if (FindGround(Anchor, R, G))
	{
		S.Goal = G;
	}
}

void ANWLoadTest::Resize(int32 Count)
{
	UWorld* World = GetWorld();
	if (!bHasAnchor)
	{
		AGameModeBase* GM = World->GetAuthGameMode();
		const AActor* Start = GM ? GM->FindPlayerStart(nullptr) : nullptr;
		Anchor = Start ? Start->GetActorLocation() : FVector::ZeroVector;
		bHasAnchor = true;
	}
	while (Sims.Num() > Count)
	{
		if (ANWCharacter* P = Sims.Last().Pawn.Get())
		{
			P->Destroy();
		}
		Sims.Pop();
	}
	int32 Failed = 0;
	while (Sims.Num() < Count && Failed < 50)
	{
		FSim S;
		const int32 Pick = Sims.Num() % 10;
		S.Role = FParse::Param(FCommandLine::Get(), TEXT("nwsimgather")) ? ERole::Gather
			: Pick < 3 ? ERole::City : Pick < 8 ? ERole::Roam : ERole::Fight;
		FVector Where;
		if (!FindGround(Anchor, S.Role == ERole::City ? 4000.f : Radius, Where))
		{
			++Failed;
			continue;
		}
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const FTransform At(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Where);
		ANWCharacter* C = World->SpawnActorDeferred<ANWCharacter>(ANWCharacter::StaticClass(), At, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!C)
		{
			++Failed;
			continue;
		}
		C->bSimPlayer = true;
		C->Tags.Add(TEXT("NWSim"));
		C->FinishSpawning(At);
		if (S.Role == ERole::Gather && C->Inventory)
		{
			C->Inventory->GiveTools();
		}
		S.Pawn = C;
		S.LastPos = Where;
		S.NextCheck = World->GetTimeSeconds() + FMath::FRandRange(0.f, 3.f);
		S.NextAttack = World->GetTimeSeconds() + FMath::FRandRange(0.f, 2.f);
		PickGoal(S);
		Sims.Add(S);
	}
	// Load-test-only experiments (-nwexp=anim,...): they measure a change before it goes into the game.
	FString Exp;
	FParse::Value(FCommandLine::Get(), TEXT("-nwexp="), Exp);
	for (TActorIterator<ANWCharacter> It(World); It; ++It)
	{
		if (Exp.Contains(TEXT("meshcol")))
		{
			// Mesh without collision or overlap on the server (hit and movement use the capsule).
			It->GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			It->GetMesh()->SetGenerateOverlapEvents(false);
		}
		if (Exp.Contains(TEXT("nooverlap")))
		{
			// Capsule without overlap events: movement stops querying overlaps at every step.
			It->GetCapsuleComponent()->SetGenerateOverlapEvents(false);
		}
		if (Exp.Contains(TEXT("floor")))
		{
			It->GetCharacterMovement()->bAlwaysCheckFloor = false;
			It->GetCharacterMovement()->bUseFlatBaseForFloorChecks = true;
		}
	}
	UE_LOG(LogNWLoad, Display, TEXT("nw.sim: %d simulated (radius %.0f m, start %s)%s"), Sims.Num(), Radius / 100.f, *Anchor.ToCompactString(),
		Failed ? *FString::Printf(TEXT(", %d without ground"), Failed) : TEXT(""));
}

void ANWLoadTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextGatherReport && Sims.Num() && Sims[0].Role == ERole::Gather)
	{
		int32 Gathering = 0, Dead = 0;
		for (const FSim& S : Sims)
		{
			const ANWCharacter* P = S.Pawn.Get();
			Dead += !P || P->Combat->State == ENWCombatState::Dead;
			Gathering += P && P->Gather && P->Gather->IsGathering();
		}
		if (NextGatherReport > 0.0)
		{
			UE_LOG(LogNWLoad, Display, TEXT("nw.sim gather: %d gathering, %d dead, %d started, %d refused, %d without a free gatherable in 10 s"),
				Gathering, Dead, GatherStarts, GatherRefused, GatherNoNode);
		}
		GatherStarts = GatherRefused = GatherNoNode = 0;
		NextGatherReport = Now + 10.0;
	}
	for (FSim& S : Sims)
	{
		ANWCharacter* P = S.Pawn.Get();
		if (!P || P->Combat->State == ENWCombatState::Dead)
		{
			continue;
		}
		const FVector Pos = P->GetActorLocation();
		if (S.Role == ERole::Gather)
		{
			TickGatherer(S, P, Pos, Now);
			continue;
		}
		FVector To = S.Goal - Pos;
		To.Z = 0.f;
		// Like a player: holds the direction until arriving; stopped for 3 s (stuck) or arrived, another destination.
		if (To.SizeSquared() < 200.f * 200.f || Now >= S.NextCheck)
		{
			if (Now >= S.NextCheck && FVector::DistSquared2D(Pos, S.LastPos) < 100.f * 100.f)
			{
				PickGoal(S);
			}
			else if (To.SizeSquared() < 200.f * 200.f)
			{
				PickGoal(S);
			}
			S.LastPos = Pos;
			S.NextCheck = Now + 3.0;
		}
		if (Pos.Z < Anchor.Z - 50000.f)
		{
			// Fell off the world: comes back.
			FVector G;
			if (FindGround(Anchor, 2000.f, G))
			{
				P->TeleportTo(G, P->GetActorRotation());
			}
			continue;
		}
		P->AddMovementInput(To.GetSafeNormal());
		if (S.Role == ERole::Fight && Now >= S.NextAttack)
		{
			P->Combat->RequestLightAttack();
			S.NextAttack = Now + FMath::FRandRange(1.2f, 2.5f);
		}
	}
}

void ANWLoadTest::PickNode(FSim& S, const FVector& Pos)
{
	S.Node = INDEX_NONE;
	const ANWGatherField* Field = ANWGatherField::Get(GetWorld());
	if (!Field)
	{
		return;
	}
	TSet<int32> Taken;
	for (const FSim& O : Sims)
	{
		if (&O != &S && O.Node != INDEX_NONE)
		{
			Taken.Add(O.Node);
		}
	}
	const FTransform& T = Field->GetActorTransform();
	constexpr float Cell = 5000.f;
	if (!bNodesReady)
	{
		bNodesReady = true;
		int32 N = 0;
		for (int32 I = 0; I < Field->Num(); ++I)
		{
			const FVector L = T.TransformPosition(Field->Node(I).Transform.GetLocation());
			if (FVector::DistSquared2D(L, Anchor) <= Radius * Radius)
			{
				NodeCells.FindOrAdd(FIntPoint(FMath::FloorToInt(L.X / Cell), FMath::FloorToInt(L.Y / Cell))).Add(I);
				++N;
			}
		}
		UE_LOG(LogNWLoad, Display, TEXT("nw.sim: %d gatherables within the radius"), N);
	}
	// Nearest free one, searching rings of 50 m cells outward (up to ~400 m); a little random so they spread.
	const FIntPoint C(FMath::FloorToInt(Pos.X / Cell), FMath::FloorToInt(Pos.Y / Cell));
	float Best = TNumericLimits<float>::Max();
	for (int32 Ring = 0; Ring <= 8 && S.Node == INDEX_NONE; ++Ring)
	{
		for (int32 DX = -Ring; DX <= Ring; ++DX)
		{
			for (int32 DY = -Ring; DY <= Ring; ++DY)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Ring)
				{
					continue;
				}
				const TArray<int32>* List = NodeCells.Find(C + FIntPoint(DX, DY));
				if (!List)
				{
					continue;
				}
				for (const int32 I : *List)
				{
					if (Field->IsDepleted(I) || Taken.Contains(I))
					{
						continue;
					}
					const float D = FVector::DistSquared2D(T.TransformPosition(Field->Node(I).Transform.GetLocation()), Pos)
						* FMath::FRandRange(1.f, 1.5f);
					if (D < Best)
					{
						Best = D;
						S.Node = I;
					}
				}
			}
		}
	}
	if (S.Node == INDEX_NONE)
	{
		++GatherNoNode;
	}
	if (S.Node != INDEX_NONE)
	{
		S.Goal = T.TransformPosition(Field->Node(S.Node).Transform.GetLocation());
	}
}

void ANWLoadTest::TickGatherer(FSim& S, ANWCharacter* P, const FVector& Pos, double Now)
{
	UNWGatherComponent* G = P->Gather;
	const ANWGatherField* Field = ANWGatherField::Get(GetWorld());
	if (!G || !Field)
	{
		return;
	}
	if (G->IsGathering())
	{
		S.bWasGathering = true;
		S.LastPos = Pos;
		S.NextCheck = Now + 3.0;
		return;
	}
	if (S.Node == INDEX_NONE && Now < S.NextCheck)
	{
		return;   // waiting after a refusal or with nothing free around
	}
	if (S.bWasGathering || S.Node == INDEX_NONE || Field->IsDepleted(S.Node))
	{
		// Finished (or never had one, or someone emptied it): next gatherable.
		S.bWasGathering = false;
		PickNode(S, Pos);
		S.LastPos = Pos;
		S.NextCheck = Now + 3.0;
		if (S.Node == INDEX_NONE)
		{
			return;
		}
	}
	if (Field->InReach(S.Node, Pos, 50.f))
	{
		// Stops first, like a player before pressing E (moving cancels the gathering).
		P->GetCharacterMovement()->StopMovementImmediately();
		G->ServerGather(S.Node);
		if (G->IsGathering())
		{
			++GatherStarts;
		}
		else
		{
			// Refused (in combat, someone else there): another one in a moment.
			++GatherRefused;
			S.Node = INDEX_NONE;
			S.bWasGathering = false;
			S.NextCheck = Now + 1.0;
		}
		return;
	}
	if (Now >= S.NextCheck)
	{
		if (FVector::DistSquared2D(Pos, S.LastPos) < 100.f * 100.f)
		{
			PickNode(S, Pos);   // stuck on the way
		}
		S.LastPos = Pos;
		S.NextCheck = Now + 3.0;
	}
	FVector To = S.Goal - Pos;
	To.Z = 0.f;
	P->AddMovementInput(To.GetSafeNormal());
}

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
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogNWLoad, Log, All);

namespace
{
	// ---------------------------------------------------------------- relatório de desempenho

	/** Tempo de trabalho de cada ciclo do mundo (início → fim do UWorld::Tick, sem a espera do limite de ciclos). */
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
			UE_LOG(LogNWLoad, Display, TEXT("NWPerf ligado: %s"), *CsvPath);
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
			// Contagens: varrer o mundo uma vez a cada 10 s é custo do relatório, não do jogo.
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
			UE_LOG(LogNWLoad, Display, TEXT("NWPerf: %d ciclos em %.0f s | trabalho med %.2f p95 %.2f max %.2f ms | intervalo %.1f ms | jogadores %d simulados %d criaturas %d personagens %d | saída %.0f kbit/s | memória %.0f MB"),
				WorkMs.Num(), Seconds, Avg, P95, Max, Frame, Players, Sims, Creatures, Chars, OutKbps, MemMb);
			const FString Line = FString::Printf(TEXT("%.0f,%d,%.3f,%.3f,%.3f,%.2f,%d,%d,%d,%d,%.0f,%.0f\n"), Wd->GetTimeSeconds(), WorkMs.Num(), Avg, P95, Max, Frame,
				Players, Sims, Creatures, Chars, OutKbps, MemMb);
			FFileHelper::SaveStringToFile(Line, *CsvPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
			WorkMs.Reset();
			FrameMs.Reset();
		}
	};
	FNWPerf Perf;

	// ---------------------------------------------------------------- comandos

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
		TEXT("Teste de carga: nw.sim <n> [raio_m=600] cria n jogadores simulados no servidor (0 remove)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UWorld* W = ServerWorld(World))
			{
				ANWLoadTest::SetSims(W, Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 600.f);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs PerfCmd(TEXT("nw.perf"),
		TEXT("Relatório de desempenho do servidor a cada 10 s: nw.perf 1 | nw.perf 0."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UWorld* W = ServerWorld(World))
			{
				ANWLoadTest::SetPerf(W, Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
			}
		}));

	/** Linha de comando do servidor: -nwperf liga o relatório e -nwsim=<n> -nwsimraio=<m> cria os simulados 10 s depois
	 *  de o mapa subir (o spawn de criaturas e o resto já começaram). */
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
							// -nwcsv=<ciclos>: perfil CSV da Unreal (tempo por categoria) 15 s depois, em Saved/Profiling/CSV.
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

// ---------------------------------------------------------------- simulados

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
		S.Role = Pick < 3 ? ERole::City : Pick < 8 ? ERole::Roam : ERole::Fight;
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
		S.Pawn = C;
		S.LastPos = Where;
		S.NextCheck = World->GetTimeSeconds() + FMath::FRandRange(0.f, 3.f);
		S.NextAttack = World->GetTimeSeconds() + FMath::FRandRange(0.f, 2.f);
		PickGoal(S);
		Sims.Add(S);
	}
	// Experimentos só do teste de carga (-nwexp=anim,...): medem uma mudança antes de ela ir para o jogo.
	FString Exp;
	FParse::Value(FCommandLine::Get(), TEXT("-nwexp="), Exp);
	for (TActorIterator<ANWCharacter> It(World); It; ++It)
	{
		if (Exp.Contains(TEXT("meshcol")))
		{
			// Malha sem colisão nem sobreposição no servidor (golpe e movimento usam a cápsula).
			It->GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			It->GetMesh()->SetGenerateOverlapEvents(false);
		}
		if (Exp.Contains(TEXT("nooverlap")))
		{
			// Cápsula sem eventos de sobreposição: o movimento deixa de consultar sobreposições a cada passo.
			It->GetCapsuleComponent()->SetGenerateOverlapEvents(false);
		}
		if (Exp.Contains(TEXT("floor")))
		{
			It->GetCharacterMovement()->bAlwaysCheckFloor = false;
			It->GetCharacterMovement()->bUseFlatBaseForFloorChecks = true;
		}
	}
	UE_LOG(LogNWLoad, Display, TEXT("nw.sim: %d simulados (raio %.0f m, início %s)%s"), Sims.Num(), Radius / 100.f, *Anchor.ToCompactString(),
		Failed ? *FString::Printf(TEXT(", %d sem chão"), Failed) : TEXT(""));
}

void ANWLoadTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetWorld()->GetTimeSeconds();
	for (FSim& S : Sims)
	{
		ANWCharacter* P = S.Pawn.Get();
		if (!P || P->Combat->State == ENWCombatState::Dead)
		{
			continue;
		}
		const FVector Pos = P->GetActorLocation();
		FVector To = S.Goal - Pos;
		To.Z = 0.f;
		// Como um jogador: segura a direção até chegar; parado 3 s (preso) ou chegou, outro destino.
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
			// Caiu do mundo: volta.
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
